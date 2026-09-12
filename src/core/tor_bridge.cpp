#include "core/tor_bridge.hpp"
#include "storage/database.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace Blueprint::Core::TorBridge {

#if !defined(LUMEN_NO_WEBKIT)
static WebKitWebContext* s_torContext = nullptr;
static int s_cachedTorPort = 0;
#endif

static std::string toLower(std::string_view str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool isOnionUrl(const std::string& url, std::string* outHost) {
    if (url.empty()) return false;

    // Skip leading whitespace
    size_t start = url.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;

    std::string_view view(url.c_str() + start, url.size() - start);

    // Skip protocol scheme if present (e.g. "http://", "https://")
    size_t protoPos = view.find("://");
    if (protoPos != std::string_view::npos) {
        view.remove_prefix(protoPos + 3);
    }

    // Strip credentials (user:pass@) if present
    size_t atPos = view.find('@');
    if (atPos != std::string_view::npos && atPos < view.find('/')) {
        view.remove_prefix(atPos + 1);
    }

    // Extract host until end of host (stop at '/', '?', '#', or ':')
    size_t hostEnd = view.find_first_of("/?#:");
    std::string_view hostView = (hostEnd != std::string_view::npos) ? view.substr(0, hostEnd) : view;

    std::string host = toLower(hostView);
    if (outHost) {
        *outHost = host;
    }

    if (host == "onion") return true;

    constexpr std::string_view SUFFIX = ".onion";
    if (host.size() >= SUFFIX.size() &&
        host.compare(host.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX) == 0) {
        return true;
    }

    return false;
}

OnionV3Parts parseOnionV3(const std::string& url) {
    OnionV3Parts parts;
    if (url.empty()) return parts;

    std::string work = url;
    // Protocol
    size_t pPos = work.find("://");
    if (pPos != std::string::npos) {
        parts.proto = work.substr(0, pPos + 3);
        work = work.substr(pPos + 3);
    } else {
        parts.proto = "http://";
    }

    // Path & query
    size_t slashPos = work.find_first_of("/?#");
    if (slashPos != std::string::npos) {
        parts.path = work.substr(slashPos);
        work = work.substr(0, slashPos);
    }

    // Port
    size_t colonPos = work.rfind(':');
    if (colonPos != std::string::npos && work.find(']') == std::string::npos) {
        work = work.substr(0, colonPos);
    }

    std::string host = toLower(work);
    constexpr std::string_view SUFFIX = ".onion";
    if (host.size() >= SUFFIX.size() &&
        host.compare(host.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX) == 0) {
        parts.tld = ".onion";
        std::string name = host.substr(0, host.size() - SUFFIX.size());

        // Extract last subdomain if multiple (e.g. "sub.domainv3")
        size_t lastDot = name.rfind('.');
        std::string v3name = (lastDot != std::string::npos) ? name.substr(lastDot + 1) : name;

        if (v3name.length() >= 56) {
            parts.isV3 = true;
            parts.head = v3name.substr(0, 6);
            parts.body = "··············································";
            parts.tail = v3name.substr(v3name.length() - 4);
        } else {
            parts.isV3 = false;
            parts.head = v3name;
        }
    } else {
        parts.head = host;
    }

    return parts;
}

bool probeTorDaemon(int port, int timeoutMs) {
    if (port <= 0 || port > 65535) return false;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    // Set non-blocking socket
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int res = connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    bool connected = false;
    if (res == 0) {
        connected = true;
    } else if (errno == EINPROGRESS) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sock, &wset);

        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        res = select(sock + 1, nullptr, &wset, nullptr, &tv);
        if (res > 0) {
            int so_error = 0;
            socklen_t len = sizeof(so_error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0 && so_error == 0) {
                connected = true;
            }
        }
    }

    if (!connected) {
        close(sock);
        return false;
    }

    // Set blocking mode with a brief timeout to perform SOCKS5 handshake verification
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }
    timeval ioTv{};
    int ioTimeout = (timeoutMs > 250) ? timeoutMs : 250;
    ioTv.tv_sec = ioTimeout / 1000;
    ioTv.tv_usec = (ioTimeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &ioTv, sizeof(ioTv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &ioTv, sizeof(ioTv));

    // SOCKS5 greeting: [VER=0x05, NMETHODS=1, METHOD=0x00 (NO AUTH)]
    const uint8_t greet[] = { 0x05, 0x01, 0x00 };
    if (send(sock, greet, sizeof(greet), MSG_NOSIGNAL) != sizeof(greet)) {
        close(sock);
        return false;
    }

    // Expected response: [VER=0x05, METHOD=0x00]
    uint8_t resp[2] = { 0, 0 };
    ssize_t recvd = recv(sock, resp, sizeof(resp), 0);
    close(sock);

    return (recvd == 2 && resp[0] == 0x05 && resp[1] == 0x00);
}

#if !defined(LUMEN_NO_WEBKIT)
WebKitWebContext* getTorWebContext(int port) {
    if (port <= 0 || port > 65535) port = 9050;

    if (s_torContext && s_cachedTorPort == port) {
        return s_torContext;
    }

    if (s_torContext) {
        g_object_unref(s_torContext);
        s_torContext = nullptr;
    }

    // 1. Completely isolated ephemeral data manager (never touches disk)
    WebKitWebsiteDataManager* manager = webkit_website_data_manager_new_ephemeral();
    webkit_website_data_manager_set_tls_errors_policy(manager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    // 2. Strict SOCKS5 proxy (GIO GSocks5Proxy with remote DNS resolution)
    std::string proxyUri = "socks5://127.0.0.1:" + std::to_string(port);
    WebKitNetworkProxySettings* proxySettings = webkit_network_proxy_settings_new(proxyUri.c_str(), nullptr);
    if (proxySettings) {
        webkit_website_data_manager_set_network_proxy_settings(manager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, proxySettings);
        webkit_network_proxy_settings_free(proxySettings);
    }

    // 3. WebContext
    s_torContext = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
        "website-data-manager", manager,
        "process-swap-on-cross-site-navigation-enabled", TRUE,
        NULL));
    g_object_unref(manager);

    // 4. Scheme registration for internal lumen:// pages
    WebKitSecurityManager* secMgr = webkit_web_context_get_security_manager(s_torContext);
    if (secMgr) {
        webkit_security_manager_register_uri_scheme_as_local(secMgr, "lumen");
        webkit_security_manager_register_uri_scheme_as_cors_enabled(secMgr, "lumen");
    }

    s_cachedTorPort = port;
    return s_torContext;
}

void resetTorWebContext() {
    if (s_torContext) {
        g_object_unref(s_torContext);
        s_torContext = nullptr;
        s_cachedTorPort = 0;
    }
}
#endif

bool isTorRoutingEnabled() {
    return Storage::Database::instance().getSetting("onion_routing_enabled", "0") == "1";
}

void setTorRoutingEnabled(bool enabled) {
    Storage::Database::instance().setSetting("onion_routing_enabled", enabled ? "1" : "0");
}

int getTorPort() {
    std::string pStr = Storage::Database::instance().getSetting("onion_tor_port", "9050");
    try {
        int p = std::stoi(pStr);
        if (p > 0 && p <= 65535) return p;
    } catch (...) {}
    return 9050;
}

void setTorPort(int port) {
    if (port <= 0 || port > 65535) port = 9050;
    Storage::Database::instance().setSetting("onion_tor_port", std::to_string(port));
#if !defined(LUMEN_NO_WEBKIT)
    resetTorWebContext();
#endif
}

} // namespace Blueprint::Core::TorBridge
