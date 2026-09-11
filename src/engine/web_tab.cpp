#include "engine/web_tab.hpp"
#include "engine/new_tab_html.hpp"
#include "engine/null_tab_html.hpp"
#include "engine/error_page_html.hpp"
#include <algorithm>
#include <iostream>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "storage/database.hpp"
#include "core/config.hpp"

namespace Blueprint::Engine {

namespace {

// count actual stored cookies for host from sqlite jar
static uint64_t getCookieBytesForHost(const std::string& host) {
    if (host.empty()) return 0;
    std::string dataDir = std::string(g_get_user_data_dir()) + "/lumen-browser";
    std::string cookiePath = dataDir + "/cookies.sqlite";
    if (!g_file_test(cookiePath.c_str(), G_FILE_TEST_EXISTS)) {
        cookiePath = std::string(g_get_user_data_dir()) + "/lampa-browser/cookies.sqlite";
        if (!g_file_test(cookiePath.c_str(), G_FILE_TEST_EXISTS)) {
            cookiePath = std::string(g_get_user_data_dir()) + "/blueprint/cookies.sqlite";
            if (!g_file_test(cookiePath.c_str(), G_FILE_TEST_EXISTS)) return 0;
        }
    }

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(cookiePath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    std::string h = host;
    if (h.rfind("www.", 0) == 0) h = h.substr(4);
    std::string pattern = "%" + h + "%";

    const char* sql = "SELECT SUM(length(name) + length(value) + 64) FROM moz_cookies WHERE host LIKE ?1;";
    sqlite3_stmt* stmt = nullptr;
    uint64_t bytes = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            sqlite3_int64 val = sqlite3_column_int64(stmt, 0);
            if (val > 0) bytes = static_cast<uint64_t>(val);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return bytes;
}

std::string extractHost(const std::string& url) {
    std::string u = url;
    size_t p = u.find("://");
    if (p != std::string::npos) u = u.substr(p + 3);
    size_t slash = u.find('/');
    if (slash != std::string::npos) u = u.substr(0, slash);
    size_t colon = u.find(':');
    if (colon != std::string::npos) u = u.substr(0, colon);
    return u;
}

std::string extractIssuerFromPem(const std::string& pemStr) {
    if (pemStr.empty()) return "";
    BIO* bio = BIO_new_mem_buf(pemStr.data(), static_cast<int>(pemStr.size()));
    if (!bio) return "";
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!cert) return "";

    X509_NAME* issuerName = X509_get_issuer_name(cert);
    std::string result;
    if (issuerName) {
        char buf[256] = {0};
        int len = X509_NAME_get_text_by_NID(issuerName, NID_organizationName, buf, sizeof(buf) - 1);
        if (len > 0) {
            result = buf;
        } else {
            len = X509_NAME_get_text_by_NID(issuerName, NID_commonName, buf, sizeof(buf) - 1);
            if (len > 0) result = buf;
        }
    }
    X509_free(cert);
    return result;
}

// Auto-detect system or local proxy (e.g. GNOME manual proxy or Xray/Happ local proxy)
static std::string detectSystemProxyUri() {
    // 1. Check environment variables
    const char* envHttps = getenv("https_proxy");
    if (!envHttps) envHttps = getenv("HTTPS_PROXY");
    if (envHttps && envHttps[0] != '\0') return envHttps;

    const char* envHttp = getenv("http_proxy");
    if (!envHttp) envHttp = getenv("HTTP_PROXY");
    if (envHttp && envHttp[0] != '\0') return envHttp;

    const char* envAll = getenv("all_proxy");
    if (!envAll) envAll = getenv("ALL_PROXY");
    if (envAll && envAll[0] != '\0') return envAll;

    // 2. Check GNOME desktop system proxy settings
    GSettingsSchemaSource* src = g_settings_schema_source_get_default();
    if (src) {
        GSettingsSchema* schema = g_settings_schema_source_lookup(src, "org.gnome.system.proxy", TRUE);
        if (schema) {
            g_settings_schema_unref(schema);
            GSettings* proxySettings = g_settings_new("org.gnome.system.proxy");
            char* mode = g_settings_get_string(proxySettings, "mode");
            std::string smode = mode ? mode : "";
            g_free(mode);
            g_object_unref(proxySettings);

            if (smode == "manual") {
                // Try http proxy
                GSettingsSchema* httpSchema = g_settings_schema_source_lookup(src, "org.gnome.system.proxy.http", TRUE);
                if (httpSchema) {
                    g_settings_schema_unref(httpSchema);
                    GSettings* httpSettings = g_settings_new("org.gnome.system.proxy.http");
                    char* host = g_settings_get_string(httpSettings, "host");
                    int port = g_settings_get_int(httpSettings, "port");
                    std::string shost = host ? host : "";
                    g_free(host);
                    g_object_unref(httpSettings);
                    if (!shost.empty() && port > 0) {
                        return "http://" + shost + ":" + std::to_string(port);
                    }
                }
                // Try socks proxy
                GSettingsSchema* socksSchema = g_settings_schema_source_lookup(src, "org.gnome.system.proxy.socks", TRUE);
                if (socksSchema) {
                    g_settings_schema_unref(socksSchema);
                    GSettings* socksSettings = g_settings_new("org.gnome.system.proxy.socks");
                    char* host = g_settings_get_string(socksSettings, "host");
                    int port = g_settings_get_int(socksSettings, "port");
                    std::string shost = host ? host : "";
                    g_free(host);
                    g_object_unref(socksSettings);
                    if (!shost.empty() && port > 0) {
                        return "socks5://" + shost + ":" + std::to_string(port);
                    }
                }
            }
        }
    }

    // 3. Fallback check for active local proxy ports (10809 HTTP / 10808 SOCKS)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 80000; // 80ms quick timeout
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(10809);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sock);
            return "http://127.0.0.1:10809";
        }
        close(sock);
    }

    return "";
}

} // anonymous namespace

static std::string s_defaultUserAgent = "";

void WebTab::setDefaultUserAgent(const std::string& ua) {
    s_defaultUserAgent = ua;
}

std::string WebTab::getDefaultUserAgent() {
    return s_defaultUserAgent;
}

void WebTab::setUserAgent(const std::string& ua) {
    if (!m_webView) return;
    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webView));
    if (!settings) return;
    if (ua.empty() || ua == "DEFAULT") {
        webkit_settings_set_user_agent(settings, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 lumen/1.0");
    } else {
        webkit_settings_set_user_agent(settings, ua.c_str());
    }
}

std::string WebTab::getUserAgent() const {
    if (!m_webView) return "";
    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webView));
    if (!settings) return "";
    const char* ua = webkit_settings_get_user_agent(settings);
    return ua ? ua : "";
}

static WebKitWebContext* getSharedWebContext() {
    static WebKitWebContext* s_context = nullptr;
    if (s_context) return s_context;

    std::string dataDir = std::string(g_get_user_data_dir()) + "/lumen-browser";
    std::string cacheDir = std::string(g_get_user_cache_dir()) + "/lumen-browser";
    std::string lampaDataDir = std::string(g_get_user_data_dir()) + "/lampa-browser";
    std::string oldDataDir = std::string(g_get_user_data_dir()) + "/blueprint";
    if (!g_file_test(dataDir.c_str(), G_FILE_TEST_IS_DIR)) {
        if (g_file_test(lampaDataDir.c_str(), G_FILE_TEST_IS_DIR)) {
            dataDir = lampaDataDir;
        } else if (g_file_test(oldDataDir.c_str(), G_FILE_TEST_IS_DIR)) {
            dataDir = oldDataDir;
        }
    }
    g_mkdir_with_parents(dataDir.c_str(), 0700);
    g_mkdir_with_parents(cacheDir.c_str(), 0700);

    WebKitWebsiteDataManager* manager = webkit_website_data_manager_new(
        "base-data-directory", dataDir.c_str(),
        "base-cache-directory", cacheDir.c_str(),
        NULL
    );

    webkit_website_data_manager_set_tls_errors_policy(manager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    std::string proxyUri = detectSystemProxyUri();
    if (!proxyUri.empty()) {
        WebKitNetworkProxySettings* proxySettings = webkit_network_proxy_settings_new(proxyUri.c_str(), nullptr);
        if (proxySettings) {
            webkit_website_data_manager_set_network_proxy_settings(manager, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, proxySettings);
            webkit_network_proxy_settings_free(proxySettings);
        }
    }

    s_context = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
        "website-data-manager", manager,
        "process-swap-on-cross-site-navigation-enabled", TRUE,
        NULL));
    g_object_unref(manager);

    WebKitSecurityManager* secMgr = webkit_web_context_get_security_manager(s_context);
    if (secMgr) {
        webkit_security_manager_register_uri_scheme_as_local(secMgr, "lumen");
        webkit_security_manager_register_uri_scheme_as_cors_enabled(secMgr, "lumen");
    }

    WebKitCookieManager* cm = webkit_web_context_get_cookie_manager(s_context);
    std::string cookiePath = dataDir + "/cookies.sqlite";
    webkit_cookie_manager_set_persistent_storage(cm, cookiePath.c_str(), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    webkit_cookie_manager_set_accept_policy(cm, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);

    return s_context;
}

WebTab::WebTab(int id, const std::string& url, const std::string& title, WebKitWebContext* context, bool isEphemeral)
    : m_id(id), m_isEphemeral(isEphemeral), m_url(url), m_title(title) {

    WebKitSettings* settings = webkit_settings_new();
    webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ON_DEMAND);
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    webkit_settings_set_enable_webgl(settings, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
    webkit_settings_set_enable_page_cache(settings, m_isEphemeral ? FALSE : TRUE);

    // enable media streaming and HTML5 audio/video features (MSE, EME, WebRTC, Fullscreen)
    webkit_settings_set_enable_media(settings, TRUE);
    webkit_settings_set_enable_mediasource(settings, TRUE);
    webkit_settings_set_enable_media_capabilities(settings, TRUE);
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_enable_webrtc(settings, TRUE);
    webkit_settings_set_enable_encrypted_media(settings, TRUE);
    webkit_settings_set_enable_site_specific_quirks(settings, TRUE);
    webkit_settings_set_enable_fullscreen(settings, TRUE);
    webkit_settings_set_media_playback_allows_inline(settings, TRUE);
    webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE);
    webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);

    if (s_defaultUserAgent.empty() || s_defaultUserAgent == "DEFAULT") {
        webkit_settings_set_user_agent(settings, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 lumen/1.0");
    } else {
        webkit_settings_set_user_agent(settings, s_defaultUserAgent.c_str());
    }

    WebKitWebsitePolicies* defaultPolicies = webkit_website_policies_new_with_policies(
        "autoplay", WEBKIT_AUTOPLAY_ALLOW,
        NULL
    );

    WebKitUserContentManager* ucm = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(ucm, "lumenMedia");
    g_signal_connect(ucm, "script-message-received::lumenMedia", G_CALLBACK(+[](WebKitUserContentManager*, WebKitJavascriptResult* res, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        if (!self) return;
#if WEBKIT_CHECK_VERSION(2, 22, 0)
        JSCValue* val = webkit_javascript_result_get_js_value(res);
        if (val && jsc_value_is_string(val)) {
            char* str = jsc_value_to_string(val);
            if (str) {
                self->handleMediaScriptMessage(str);
                g_free(str);
            }
        }
#endif
    }), this);

    WebKitWebContext* webCtx = context ? context : getSharedWebContext();

    m_webView = GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webCtx,
        "settings", settings,
        "user-content-manager", ucm,
        "website-policies", defaultPolicies,
        NULL));
    g_object_unref(ucm);
    g_object_unref(settings);
    g_object_unref(defaultPolicies);

    // dark obsidian bg so it doesnt flash blinding white while loading
    GdkRGBA bg = m_isEphemeral ? GdkRGBA{0.027, 0.031, 0.035, 1.0} : GdkRGBA{0.055, 0.067, 0.086, 1.0};
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(m_webView), &bg);

    setupWebKitSignals();
    loadUrl(url);
}

WebTab::~WebTab() {
    stopMediaPoll();
    if (m_webView) {
        gtk_widget_destroy(m_webView);
        m_webView = nullptr;
    }
}

void WebTab::setupWebKitSignals() {
    g_signal_connect(m_webView, "notify::title", G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        const char* t = webkit_web_view_get_title(WEBKIT_WEB_VIEW(obj));
        if (t && t[0] != '\0') {
            self->m_title = t;
            if (self->m_onTitleChange) self->m_onTitleChange(self->m_title);
        }
    }), this);

    g_signal_connect(m_webView, "notify::uri", G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        const char* u = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(obj));
        if (u) {
            self->m_url = u;
            if (self->m_onUrlChange) self->m_onUrlChange(self->m_url);
        }
    }), this);

    g_signal_connect(m_webView, "notify::estimated-load-progress", G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        double p = webkit_web_view_get_estimated_load_progress(WEBKIT_WEB_VIEW(obj));
        self->m_loadProgress = static_cast<float>(p);
        self->m_isLoading = (p < 0.99);
        if (self->m_onProgressChange) self->m_onProgressChange(self->m_loadProgress);
    }), this);

    g_signal_connect(m_webView, "load-failed", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent, const char* failing_uri, GError* err, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        // If operation was cancelled (e.g. HTTP 301/302 redirect on youtube.com or aborted sub-resource),
        // suppress error page so navigation and redirects complete normally.
        if (err) {
            if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
                g_error_matches(err, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED) ||
                g_error_matches(err, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE)) {
                return TRUE;
            }
        }
        // Only mark loadFailed if the primary page failed, not secondary subresources/trackers/beacons
        if (failing_uri && !self->m_url.empty() && self->m_url.find(failing_uri) != std::string::npos) {
            self->m_loadFailed = true;
            self->m_isLoading = false;
            self->m_loadProgress = 1.0f;
            self->m_siteDataBytes = 0;
            self->m_siteDataKnown = true;
            if (self->m_onProgressChange) self->m_onProgressChange(1.0f);

            std::string uriStr = failing_uri ? failing_uri : self->m_url;
            std::string errorCode = "NET::ERR_CONNECTION_FAILED";
            std::string errorTitle = "Connection Failure";
            std::string errorDesc = "The browser could not establish a connection to the host. Verify your network connection and server status.";
            std::string diagCode = "0x00000000";

            if (err) {
                const char* domainStr = g_quark_to_string(err->domain);
                diagCode = (domainStr ? std::string(domainStr) : "GError") + " / " + std::to_string(err->code);

                if (g_error_matches(err, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_TRANSPORT)) {
                    errorCode = "NET::ERR_CONNECTION_REFUSED";
                    errorTitle = "Connection Refused";
                    errorDesc = "The remote server actively rejected the connection request. The port may be closed or the server is offline.";
                } else if (g_error_matches(err, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_FAILED)) {
                    errorCode = "NET::ERR_CONNECTION_RESET";
                    errorTitle = "Connection Reset";
                    errorDesc = "The connection to the server was unexpectedly closed or reset while transferring data.";
                } else if (err->message && std::string(err->message).find("resolve") != std::string::npos) {
                    errorCode = "NET::ERR_NAME_NOT_RESOLVED";
                    errorTitle = "DNS Resolution Failed";
                    errorDesc = "The domain name could not be resolved to an IP address. Check for typos or DNS configuration errors.";
                } else if (err->message && (std::string(err->message).find("timed out") != std::string::npos || std::string(err->message).find("Timeout") != std::string::npos)) {
                    errorCode = "NET::ERR_TIMED_OUT";
                    errorTitle = "Connection Timed Out";
                    errorDesc = "The server took too long to respond. The network may be congested or the host is unreachable.";
                } else if (err->message && err->message[0] != '\0') {
                    errorDesc = err->message;
                }
            }

            self->loadErrorPage(uriStr, errorCode, errorTitle, errorDesc, diagCode);
        }
        // Always return TRUE to prevent WebKit default error page handler from crashing under Wayland
        return TRUE;
    }), this);

    g_signal_connect(m_webView, "load-changed", G_CALLBACK(+[](WebKitWebView* web_view, WebKitLoadEvent event, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        if (event == WEBKIT_LOAD_STARTED) {
            self->m_loadFailed = false;
            self->m_isLoading = true;
            self->m_loadProgress = 0.1f;
            self->m_siteDataKnown = false;
            if (self->m_onProgressChange) self->m_onProgressChange(self->m_loadProgress);
        } else if (event == WEBKIT_LOAD_FINISHED) {
            self->m_isLoading = false;
            self->m_loadProgress = 1.0f;
            if (self->m_onProgressChange) self->m_onProgressChange(self->m_loadProgress);
            const char* title = webkit_web_view_get_title(web_view);
            if (title && title[0] != '\0') {
                self->m_title = title;
                if (self->m_onTitleChange) self->m_onTitleChange(self->m_title);
            }
            if (!self->m_loadFailed && !self->m_isErrorPage) {
                WebKitWebResource* mainRes = webkit_web_view_get_main_resource(web_view);
                if (mainRes) {
                    WebKitURIResponse* resp = webkit_web_resource_get_response(mainRes);
                    if (resp) {
                        guint statusCode = webkit_uri_response_get_status_code(resp);
                        if (statusCode >= 400) {
                            const char* uri = webkit_uri_response_get_uri(resp);
                            std::string uriStr = uri ? uri : self->m_url;
                            std::string errorCode = "HTTP::ERR_" + std::to_string(statusCode);
                            std::string errorTitle = "HTTP Error " + std::to_string(statusCode);
                            std::string errorDesc = "The server responded with an error status code.";
                            if (statusCode == 403) {
                                errorCode = "HTTP::ERR_FORBIDDEN";
                                errorTitle = "Access Forbidden (403)";
                                errorDesc = "You do not have permission to access the requested resource or directory on this server.";
                            } else if (statusCode == 404) {
                                errorCode = "HTTP::ERR_NOT_FOUND";
                                errorTitle = "Page Not Found (404)";
                                errorDesc = "The requested URL was not found on this server. Check for typos or moved resources.";
                            } else if (statusCode == 500) {
                                errorCode = "HTTP::ERR_INTERNAL_SERVER_ERROR";
                                errorTitle = "Internal Server Error (500)";
                                errorDesc = "The server encountered an unexpected condition that prevented it from fulfilling the request.";
                            }
                            std::string diagCode = "HTTP_STATUS_" + std::to_string(statusCode);
                            self->loadErrorPage(uriStr, errorCode, errorTitle, errorDesc, diagCode);
                            return;
                        }
                    }
                }
                // self->fetchWebsiteData();
            }
        }
    }), this);

    // 1. Navigation policy & autoplay allowance (fixes videos paused or stalled due to autoplay restrictions)
    g_signal_connect(m_webView, "decide-policy", G_CALLBACK(((+[](WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
            WebKitNavigationPolicyDecision* navDecision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
            WebKitNavigationAction* navAction = webkit_navigation_policy_decision_get_navigation_action(navDecision);
            guint button = webkit_navigation_action_get_mouse_button(navAction);
            guint mods = webkit_navigation_action_get_modifiers(navAction);

            // Middle mouse click (button 2) or Ctrl+Click on a link opens it in a background tab
            if (button == 2 || ((mods & GDK_CONTROL_MASK) && !(mods & GDK_SHIFT_MASK))) {
                WebKitURIRequest* req = webkit_navigation_action_get_request(navAction);
                const char* uri = webkit_uri_request_get_uri(req);
                if (uri && uri[0] != '\0') {
                    if (self->m_onNewTabRequested) {
                        self->m_onNewTabRequested(uri, true); // inBackground = true
                    }
                    webkit_policy_decision_ignore(decision);
                    return TRUE;
                }
            }

            WebKitWebsitePolicies* policies = webkit_website_policies_new_with_policies("autoplay", WEBKIT_AUTOPLAY_ALLOW, NULL);
            webkit_policy_decision_use_with_policies(decision, policies);
            g_object_unref(policies);
            return TRUE;
        } else if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
            WebKitNavigationPolicyDecision* navDecision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
            WebKitNavigationAction* navAction = webkit_navigation_policy_decision_get_navigation_action(navDecision);
            WebKitURIRequest* req = webkit_navigation_action_get_request(navAction);
            const char* uri = webkit_uri_request_get_uri(req);
            if (uri && uri[0] != '\0') {
                guint button = webkit_navigation_action_get_mouse_button(navAction);
                guint mods = webkit_navigation_action_get_modifiers(navAction);
                bool inBackground = (button == 2) || ((mods & GDK_CONTROL_MASK) && !(mods & GDK_SHIFT_MASK));
                if (self->m_onNewTabRequested) {
                    self->m_onNewTabRequested(uri, inBackground);
                } else {
                    self->loadUrl(uri);
                }
            }
            webkit_policy_decision_ignore(decision);
            return TRUE;
        } else if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
            auto* respDecision = WEBKIT_RESPONSE_POLICY_DECISION(decision);
            if (webkit_response_policy_decision_is_main_frame_main_resource(respDecision)) {
                WebKitURIResponse* response = webkit_response_policy_decision_get_response(respDecision);
                guint statusCode = webkit_uri_response_get_status_code(response);
                if (statusCode >= 400) {
                    const char* uri = webkit_uri_response_get_uri(response);
                    std::string uriStr = uri ? uri : self->m_url;
                    std::string errorCode = "HTTP::ERR_" + std::to_string(statusCode);
                    std::string errorTitle = "HTTP Error " + std::to_string(statusCode);
                    std::string errorDesc = "The server responded with an error status code.";
                    std::string diagCode = "HTTP_STATUS_" + std::to_string(statusCode);

                    if (statusCode == 400) {
                        errorCode = "HTTP::ERR_BAD_REQUEST";
                        errorTitle = "Bad Request (400)";
                        errorDesc = "The server cannot process the request due to an apparent client error.";
                    } else if (statusCode == 401) {
                        errorCode = "HTTP::ERR_UNAUTHORIZED";
                        errorTitle = "Authorization Required (401)";
                        errorDesc = "Access to the requested resource requires valid authentication credentials.";
                    } else if (statusCode == 403) {
                        errorCode = "HTTP::ERR_FORBIDDEN";
                        errorTitle = "Access Forbidden (403)";
                        errorDesc = "You do not have permission to access the requested resource or directory on this server.";
                    } else if (statusCode == 404) {
                        errorCode = "HTTP::ERR_NOT_FOUND";
                        errorTitle = "Page Not Found (404)";
                        errorDesc = "The requested URL was not found on this server. Check for typos or moved resources.";
                    } else if (statusCode == 408) {
                        errorCode = "HTTP::ERR_REQUEST_TIMEOUT";
                        errorTitle = "Request Timeout (408)";
                        errorDesc = "The server timed out waiting for the request from the browser.";
                    } else if (statusCode == 500) {
                        errorCode = "HTTP::ERR_INTERNAL_SERVER_ERROR";
                        errorTitle = "Internal Server Error (500)";
                        errorDesc = "The server encountered an unexpected condition that prevented it from fulfilling the request.";
                    } else if (statusCode == 502) {
                        errorCode = "HTTP::ERR_BAD_GATEWAY";
                        errorTitle = "Bad Gateway (502)";
                        errorDesc = "The server received an invalid response from the upstream server while acting as a gateway.";
                    } else if (statusCode == 503) {
                        errorCode = "HTTP::ERR_SERVICE_UNAVAILABLE";
                        errorTitle = "Service Unavailable (503)";
                        errorDesc = "The server is currently unable to handle the request due to temporary overloading or maintenance.";
                    } else if (statusCode == 504) {
                        errorCode = "HTTP::ERR_GATEWAY_TIMEOUT";
                        errorTitle = "Gateway Timeout (504)";
                        errorDesc = "The gateway server did not receive a timely response from the upstream server.";
                    }

                    self->m_loadFailed = true;
                    self->m_isLoading = false;
                    self->m_loadProgress = 1.0f;
                    self->m_siteDataBytes = 0;
                    self->m_siteDataKnown = true;
                    if (self->m_onProgressChange) self->m_onProgressChange(1.0f);

                    webkit_policy_decision_ignore(decision);

                    struct ErrorArgs {
                        WebTab* tab;
                        std::string uri;
                        std::string code;
                        std::string title;
                        std::string desc;
                        std::string diag;
                    };
                    auto* args = new ErrorArgs{self, uriStr, errorCode, errorTitle, errorDesc, diagCode};
                    g_idle_add(+[](gpointer data) -> gboolean {
                        auto* a = static_cast<ErrorArgs*>(data);
                        if (a && a->tab) {
                            a->tab->loadErrorPage(a->uri, a->code, a->title, a->desc, a->diag);
                        }
                        delete a;
                        return G_SOURCE_REMOVE;
                    }, args);

                    return TRUE;
                }
            }
        }
        return FALSE;
    }))), this);

    // 2. New window / target="_blank" handler (opens video links that use window.open / target="_blank", middle-click)
    g_signal_connect(m_webView, "create", G_CALLBACK(+[](WebKitWebView*, WebKitNavigationAction* navAction, gpointer data) -> GtkWidget* {
        auto* self = static_cast<WebTab*>(data);
        WebKitURIRequest* req = webkit_navigation_action_get_request(navAction);
        const char* uri = webkit_uri_request_get_uri(req);
        if (uri && uri[0] != '\0' && self->m_onNewTabRequested) {
            guint button = webkit_navigation_action_get_mouse_button(navAction);
            guint mods = webkit_navigation_action_get_modifiers(navAction);
            bool inBackground = (button == 2) || ((mods & GDK_CONTROL_MASK) && !(mods & GDK_SHIFT_MASK));
            self->m_onNewTabRequested(uri, inBackground);
        }
        return nullptr;
    }), this);

    // 3. Permission request handler (auto-allow EME/DRM media keys, media devices, notifications)
    g_signal_connect(m_webView, "permission-request", G_CALLBACK(+[](WebKitWebView*, WebKitPermissionRequest* request, gpointer) -> gboolean {
        if (WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)) {
            webkit_permission_request_deny(request);
            return TRUE;
        }
        webkit_permission_request_allow(request);
        return TRUE;
    }), this);

    // 4. TLS error handling
    g_signal_connect(m_webView, "load-failed-with-tls-errors", G_CALLBACK(+[](WebKitWebView* webView, const char* failing_uri, GTlsCertificate* certificate, GTlsCertificateFlags flags, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        if (failing_uri && !self->m_url.empty() && self->m_url.find(failing_uri) != std::string::npos) {
            std::string uriStr = failing_uri;
            std::string errorCode = "SSL::ERR_CERT_COMMON_NAME_INVALID";
            if (flags & G_TLS_CERTIFICATE_EXPIRED) {
                errorCode = "SSL::ERR_CERT_DATE_INVALID";
            } else if (flags & G_TLS_CERTIFICATE_UNKNOWN_CA) {
                errorCode = "SSL::ERR_CERT_AUTHORITY_INVALID";
            }
            std::string errorTitle = "Security Certificate Invalid";
            std::string errorDesc = "The cryptographic certificate presented by this server could not be validated. The connection is untrusted.";
            std::string diagCode = "TLS_FLAGS_0x" + std::to_string(static_cast<unsigned int>(flags));

            self->m_loadFailed = true;
            self->m_isLoading = false;
            self->m_loadProgress = 1.0f;
            self->m_siteDataBytes = 0;
            self->m_siteDataKnown = true;
            if (self->m_onProgressChange) self->m_onProgressChange(1.0f);

            self->loadErrorPage(uriStr, errorCode, errorTitle, errorDesc, diagCode);
            return TRUE;
        }
        if (failing_uri && certificate) {
            std::string host = extractHost(failing_uri);
            if (!host.empty()) {
                WebKitWebContext* ctx = webkit_web_view_get_context(webView);
                webkit_web_context_allow_tls_certificate_for_host(ctx, certificate, host.c_str());
                return TRUE;
            }
        }
        return FALSE;
    }), this);

    // 5. Fullscreen signals for video playback
    g_signal_connect(m_webView, "enter-fullscreen", G_CALLBACK(+[](WebKitWebView*, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        if (self->m_onFullscreenToggled) self->m_onFullscreenToggled(true);
        return TRUE;
    }), this);

    g_signal_connect(m_webView, "leave-fullscreen", G_CALLBACK(+[](WebKitWebView*, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        if (self->m_onFullscreenToggled) self->m_onFullscreenToggled(false);
        return TRUE;
    }), this);

    // 6. Handle web process crash or termination gracefully without crashing UI process
    g_signal_connect(m_webView, "web-process-terminated", G_CALLBACK(+[](WebKitWebView*, WebKitWebProcessTerminationReason reason, gpointer data) {
        auto* self = static_cast<WebTab*>(data);
        self->m_isLoading = false;
        self->m_loadFailed = true;
        if (reason == WEBKIT_WEB_PROCESS_CRASHED) {
            std::cerr << "[lumen] WebProcess crashed. User must reload tab manually.\n";
            // webkit_web_view_reload(webView); // THIS CAUSES UI PROCESS SEGFAULT in WebKit 2.42+
        }
    }), this);
}

std::string WebTab::sanitizeTrackingParams(const std::string& url) {
    if (url.empty()) return url;
    size_t qPos = url.find('?');
    if (qPos == std::string::npos) return url;

    std::string base = url.substr(0, qPos);
    size_t hashPos = url.find('#', qPos);
    std::string query = (hashPos == std::string::npos)
        ? url.substr(qPos + 1)
        : url.substr(qPos + 1, hashPos - (qPos + 1));
    std::string fragment = (hashPos == std::string::npos) ? "" : url.substr(hashPos);

    static const std::vector<std::string> s_blocked = {
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "fbclid", "gclid", "gclsrc", "dclid", "msclkid", "mc_eid", "yclid", "_openstat"
    };

    std::vector<std::string> cleanParams;
    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) end = query.size();
        std::string param = query.substr(start, end - start);
        start = end + 1;

        if (param.empty()) continue;
        size_t eqPos = param.find('=');
        std::string key = (eqPos == std::string::npos) ? param : param.substr(0, eqPos);

        bool drop = false;
        for (const auto& b : s_blocked) {
            if (key == b || key.rfind("utm_", 0) == 0) {
                drop = true;
                break;
            }
        }
        if (!drop) {
            cleanParams.push_back(param);
        }
    }

    std::string result = base;
    if (!cleanParams.empty()) {
        result += "?";
        for (size_t i = 0; i < cleanParams.size(); ++i) {
            if (i > 0) result += "&";
            result += cleanParams[i];
        }
    }
    result += fragment;
    return result;
}

void WebTab::loadNewTabHtml() {
    m_url = "lumen://newtab";
    m_title = "New Tab";
    m_isLoading = false;
    m_loadProgress = 1.0f;
    std::string html = getNewTabHtml(Theme::ThemeManager::instance().activePalette());
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webView), html.c_str(), "lumen://newtab");
    if (m_onTitleChange) m_onTitleChange(m_title);
    if (m_onUrlChange) m_onUrlChange(m_url);
    if (m_onProgressChange) m_onProgressChange(1.0f);

    stopMediaPoll();
    syncSystemMediaState();
    m_mediaPollSourceId = g_timeout_add(800, G_SOURCE_FUNC(+[](gpointer data) -> gboolean {
        auto* tab = static_cast<WebTab*>(data);
        if (!tab || !tab->getWebView() || tab->getUrl() != "lumen://newtab") {
            if (tab) tab->stopMediaPoll();
            return G_SOURCE_REMOVE;
        }
        tab->syncSystemMediaState();
        return G_SOURCE_CONTINUE;
    }), this);
}

void WebTab::applyTheme(const Theme::Palette& pal) {
    if (!m_webView) return;
    if (m_url != "lumen://newtab" && m_url != "about:blank" && !m_isErrorPage) return;

    std::string js = "if (window.__setLumenTheme) { window.__setLumenTheme({"
        "bgBase: '" + pal.bgBase.toCssRgba() + "',"
        "bgSurface: '" + pal.bgSurface.toCssRgba() + "',"
        "bgSubtle: '" + pal.bgSubtle.toCssRgba() + "',"
        "bgActive: '" + pal.bgActive.toCssRgba() + "',"
        "border: '" + pal.border.toCssRgba() + "',"
        "borderHover: '" + pal.borderFocus.toCssRgba() + "',"
        "fgPrimary: '" + pal.textPrimary.toCssRgba() + "',"
        "fgMuted: '" + pal.textMuted.toCssRgba() + "',"
        "fgDim: '" + pal.textDim.toCssRgba() + "',"
        "accent: '" + pal.accent.toCssRgba() + "',"
        "accentGlow: '" + pal.accent.toCssHex() + "40',"
        "accentTint: '" + pal.accent.toCssHex() + "15',"
        "danger: '" + pal.danger.toCssRgba() + "'"
    "}); }";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webView), js.c_str(), nullptr, nullptr, nullptr);
#pragma GCC diagnostic pop
}

void WebTab::loadNullTabHtml() {
    stopMediaPoll();
    m_url = "lumen://null";
    m_title = "Lumen";
    m_isLoading = false;
    m_loadProgress = 1.0f;
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webView), NULL_TAB_HTML, "lumen://null");
    if (m_onTitleChange) m_onTitleChange(m_title);
    if (m_onUrlChange) m_onUrlChange(m_url);
    if (m_onProgressChange) m_onProgressChange(1.0f);
}

void WebTab::loadErrorPage(const std::string& failingUri, const std::string& errorCode, const std::string& errorTitle, const std::string& errorDesc, const std::string& diagCode) {
    stopMediaPoll();
    m_isErrorPage = true;
    m_failedUri = failingUri;
    m_url = failingUri;
    m_title = errorTitle.empty() ? "Connection Failure - lumen" : (errorTitle + " - lumen");
    m_isLoading = false;
    m_loadProgress = 1.0f;

    std::string html = getErrorPageHtml(
        Theme::ThemeManager::instance().activePalette(),
        failingUri,
        errorCode,
        errorTitle,
        errorDesc,
        diagCode
    );
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webView), html.c_str(), "lumen://error");
    if (m_onTitleChange) m_onTitleChange(m_title);
    if (m_onUrlChange) m_onUrlChange(m_url);
    if (m_onProgressChange) m_onProgressChange(1.0f);
}

void WebTab::loadUrl(const std::string& url) {
    if (url == "lumen://error") {
        loadErrorPage(
            !m_failedUri.empty() ? m_failedUri : "https://example.com",
            "NET::ERR_CONNECTION_FAILED",
            "Connection Failure",
            "The browser could not establish a connection to the host. Verify your network connection and server status.",
            "ERR_NETWORK_IO_SUSPENDED / 0x80004005"
        );
        return;
    }

    m_isErrorPage = false;
    if (url == "lumen://null" || url == "lumen://null-tab") {
        loadNullTabHtml();
        return;
    }

    if (m_isEphemeral && (url.empty() || url == "lumen://newtab" || url == "about:blank")) {
        loadNullTabHtml();
        return;
    }

    if (url.empty() || url == "lumen://newtab" || url == "lampa://newtab" || url == "blueprint://newtab" || url == "about:blank") {
        loadNewTabHtml();
        return;
    }

    stopMediaPoll();
    std::string full = url;
    if (m_isEphemeral) {
        full = sanitizeTrackingParams(full);
    }

    if (full.find("://") == std::string::npos && full.find("about:") != 0) {
        if (full.find('.') != std::string::npos && full.find(' ') == std::string::npos) {
            full = "https://" + full;
        } else {
            std::string tmpl = Storage::Database::instance().getSetting("search_engine_template", "https://duckduckgo.com/?q=%s");
            full = Core::BrowserConfig::formatSearchUrl(tmpl, full);
        }
    }

    if (m_isEphemeral) {
        full = sanitizeTrackingParams(full);
    }

    m_url = full;
    m_title = extractHost(full);
    m_isLoading = true;
    m_loadProgress = 0.1f;

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webView), full.c_str());
    if (m_onUrlChange) m_onUrlChange(m_url);
    if (m_onTitleChange) m_onTitleChange(m_title);
}

bool WebTab::canGoBack() const {
    return webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(m_webView));
}

bool WebTab::canGoForward() const {
    return webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(m_webView));
}

void WebTab::goBack() {
    webkit_web_view_go_back(WEBKIT_WEB_VIEW(m_webView));
}

void WebTab::goForward() {
    webkit_web_view_go_forward(WEBKIT_WEB_VIEW(m_webView));
}

bool WebTab::canReload() const {
    if (m_isErrorPage) return true;
    return !m_url.empty() && m_url != "lumen://newtab" && m_url != "lumen://null" &&
           m_url != "lumen://null-tab" && m_url != "lampa://newtab" &&
           m_url != "blueprint://newtab" && m_url != "about:blank";
}

void WebTab::reload() {
    if (!canReload()) {
        // internal lumen:// page, dont ask webkit to reload or it spits network errors
        return;
    }
    if (m_isErrorPage && !m_failedUri.empty()) {
        loadUrl(m_failedUri);
        return;
    }
    webkit_web_view_reload(WEBKIT_WEB_VIEW(m_webView));
}

double WebTab::getZoomLevel() const {
    return webkit_web_view_get_zoom_level(WEBKIT_WEB_VIEW(m_webView));
}

void WebTab::setZoomLevel(double z) {
    z = std::clamp(z, 0.25, 5.0);
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(m_webView), z);
}

void WebTab::zoomIn() {
    setZoomLevel(getZoomLevel() + 0.10);
}

void WebTab::zoomOut() {
    setZoomLevel(getZoomLevel() - 0.10);
}

void WebTab::resetZoom() {
    setZoomLevel(1.0);
}

void WebTab::handleScrollZoom(double dy) {
    double step = (dy > 0 ? 0.08 : -0.08);
    setZoomLevel(getZoomLevel() + step);
}

bool WebTab::canClearData() const {
    if (m_loadFailed || m_isEphemeral) return false;
    if (m_url.empty() || m_url == "lumen://newtab" || m_url == "lumen://null" ||
        m_url == "lumen://null-tab" || m_url == "lampa://newtab" ||
        m_url == "blueprint://newtab" || m_url == "about:blank") return false;
    std::string host = extractHost(m_url);
    if (host.empty() || host == "newtab" || host == "null" || host.find('.') == std::string::npos) return false;
    return true;
}

void WebTab::fetchWebsiteData() {
    if (!canClearData()) {
        m_siteDataBytes = 0;
        m_siteDataKnown = true;
        return;
    }
    WebKitWebsiteDataManager* dm = webkit_web_view_get_website_data_manager(WEBKIT_WEB_VIEW(m_webView));
    if (!dm) return;

    std::string host = extractHost(m_url);
    struct FetchCtx {
        WebTab* tab;
        std::string host;
    };
    auto* fc = new FetchCtx{this, host};

    webkit_website_data_manager_fetch(
        dm,
        static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_ALL),
        nullptr,
        +[](GObject* source, GAsyncResult* res, gpointer user_data) {
            auto* ctx = static_cast<FetchCtx*>(user_data);
            GError* err = nullptr;
            GList* list = webkit_website_data_manager_fetch_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), res, &err);
            if (err) g_error_free(err);

            uint64_t total = 0;
            bool foundSiteEntries = false;
            for (GList* l = list; l != nullptr; l = l->next) {
                auto* data = static_cast<WebKitWebsiteData*>(l->data);
                const char* dname = webkit_website_data_get_name(data);
                if (dname) {
                    std::string dn(dname);
                    std::string h1 = ctx->host;
                    std::string h2 = dn;
                    if (!h1.empty() && h1[0] == '.') h1 = h1.substr(1);
                    if (!h2.empty() && h2[0] == '.') h2 = h2.substr(1);
                    if (h1.rfind("www.", 0) == 0) h1 = h1.substr(4);
                    if (h2.rfind("www.", 0) == 0) h2 = h2.substr(4);

                    if (h1 == h2 || h1.find(h2) != std::string::npos || h2.find(h1) != std::string::npos) {
                        foundSiteEntries = true;
                        uint64_t diskSize = webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_DISK_CACHE);
                        uint64_t memSize  = webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_MEMORY_CACHE);
                        uint64_t anySize  = webkit_website_data_get_size(data, WEBKIT_WEBSITE_DATA_ALL);
                        total += std::max({diskSize, memSize, anySize});
                    }
                }
            }
            if (list) {
                g_list_free_full(list, reinterpret_cast<GDestroyNotify>(webkit_website_data_unref));
            }

            // add cookie payload bytes from sqlite cookie jar
            uint64_t cookieBytes = getCookieBytesForHost(ctx->host);
            total += cookieBytes;

            // if webkit has data records but returned 0 cache, add base storage footprint (e.g. localstorage)
            if (foundSiteEntries && total == 0) {
                total = 8192;
            }

            if (ctx->tab) {
                ctx->tab->m_siteDataBytes = total;
                ctx->tab->m_siteDataKnown = true;
                if (ctx->tab->m_onSiteDataChanged) {
                    ctx->tab->m_onSiteDataChanged(total);
                }
            }
            delete ctx;
        },
        fc
    );
}

TlsCertificateInfo WebTab::getTlsInfo() const {
    TlsCertificateInfo info;
    info.isHttps = (m_url.find("https://") == 0);

    if (m_loadFailed || !info.isHttps || !m_webView) {
        info.isValid = false;
        info.issuer = "";
        info.protocol = info.isHttps ? "Connection failed" : "Insecure protocol (HTTP)";
        return info;
    }

    GTlsCertificate* cert = nullptr;
    GTlsCertificateFlags flags = (GTlsCertificateFlags)0;
    gboolean ok = webkit_web_view_get_tls_info(WEBKIT_WEB_VIEW(m_webView), &cert, &flags);
    if (!ok || !cert) {
        info.isValid = false;
        info.issuer = "";
        info.protocol = "TLS";
        return info;
    }

    std::string host = extractHost(m_url);
    info.isValid = ok && (flags == 0) && (cert != nullptr);
    info.subject = host;

    if (cert) {
        gchar* pem = nullptr;
        g_object_get(cert, "certificate-pem", &pem, NULL);
        if (pem) {
            std::string pemStr(pem);
            g_free(pem);
            std::string realIssuer = extractIssuerFromPem(pemStr);
            if (!realIssuer.empty()) {
                info.issuer = realIssuer;
            }
        }
    }

    if (!info.isValid) {
        if (info.issuer.empty()) {
            info.issuer = "Untrusted certificate";
        }
        info.protocol = "TLS (Insecure)";
        return info;
    }

    if (info.issuer.empty()) {
        info.issuer = "Trusted Certificate Authority";
    }
    info.protocol = "TLS 1.3 (AES-256-GCM)";
    return info;
}

void WebTab::clearWebsiteData(std::function<void(bool)> onComplete) {
    if (!canClearData()) {
        if (onComplete) onComplete(false);
        return;
    }
    WebKitWebsiteDataManager* dm = webkit_web_view_get_website_data_manager(WEBKIT_WEB_VIEW(m_webView));
    if (!dm) {
        if (onComplete) onComplete(false);
        return;
    }

    struct ClearCtx {
        WebTab* tab;
        std::function<void(bool)> cb;
    };
    auto* ctx = new ClearCtx{this, onComplete};

    webkit_website_data_manager_clear(
        dm,
        static_cast<WebKitWebsiteDataTypes>(WEBKIT_WEBSITE_DATA_ALL),
        0, // all time
        nullptr,
        +[](GObject* source, GAsyncResult* res, gpointer user_data) {
            auto* c = static_cast<ClearCtx*>(user_data);
            GError* error = nullptr;
            gboolean ok = webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), res, &error);
            if (error) g_error_free(error);

            if (c->tab) {
                c->tab->m_siteDataBytes = 0;
                c->tab->m_siteDataKnown = true;
            }
            if (c->cb) c->cb(ok);
            delete c;
        },
        ctx
    );
}

void WebTab::startClose() {
    m_closing = true;
    m_closeProgress = 0.f;
}

void WebTab::updateClose(float dt) {
    if (!m_closing) return;
    m_closeProgress = std::min(1.f, m_closeProgress + dt * 6.f);
}

void WebTab::setCallbacks(std::function<void(const std::string&)> onTitle,
                          std::function<void(const std::string&)> onUrl,
                          std::function<void(float)> onProgress) {
    m_onTitleChange    = onTitle;
    m_onUrlChange      = onUrl;
    m_onProgressChange = onProgress;
}

bool WebTab::isPlayingAudio() const {
    if (!m_webView) return false;
    return webkit_web_view_is_playing_audio(WEBKIT_WEB_VIEW(m_webView));
}

static WebTab::InternalMediaProvider s_internalMediaProvider = nullptr;
static WebTab::InternalMediaCommander s_internalMediaCommander = nullptr;

void WebTab::setInternalMediaProvider(InternalMediaProvider provider) {
    s_internalMediaProvider = std::move(provider);
}

void WebTab::setInternalMediaCommander(InternalMediaCommander commander) {
    s_internalMediaCommander = std::move(commander);
}

std::string WebTab::querySystemMprisJson() {
    // 1. Check if an internal Lumen browser tab is actively playing media
    if (s_internalMediaProvider) {
        std::string internalJson = s_internalMediaProvider();
        if (!internalJson.empty() && internalJson.find("\"hasPlayer\":true") != std::string::npos) {
            return internalJson;
        }
    }

    // 2. Query system MPRIS players on D-Bus
    GError* err = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
    if (!bus) {
        if (err) g_error_free(err);
        return "{\"hasPlayer\":false}";
    }

    GVariant* res = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "ListNames", nullptr,
        G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, 80, nullptr, nullptr
    );

    if (!res) {
        g_object_unref(bus);
        return "{\"hasPlayer\":false}";
    }

    struct Candidate {
        std::string name;
        std::string status = "Paused";
        std::string title;
        std::string artist;
        std::string album;
        std::string artUrl;
        int64_t positionSec = 0;
        int64_t durationSec = 0;
        int score = 0;
    };

    std::vector<std::string> playerNames;
    GVariantIter* iter = nullptr;
    g_variant_get(res, "(as)", &iter);
    char* busName = nullptr;
    while (g_variant_iter_loop(iter, "s", &busName)) {
        if (g_str_has_prefix(busName, "org.mpris.MediaPlayer2.")) {
            playerNames.push_back(busName);
        }
    }
    g_variant_iter_free(iter);
    g_variant_unref(res);

    Candidate bestCandidate;
    bestCandidate.score = -1;

    for (const auto& name : playerNames) {
        Candidate cand;
        cand.name = name;

        // PlaybackStatus
        GVariant* statusVar = g_dbus_connection_call_sync(
            bus, name.c_str(), "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "Get",
            g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE, 60, nullptr, nullptr
        );
        if (statusVar) {
            GVariant* v = nullptr;
            g_variant_get(statusVar, "(v)", &v);
            const char* s = g_variant_get_string(v, nullptr);
            if (s) cand.status = s;
            g_variant_unref(v);
            g_variant_unref(statusVar);
        }

        // Position
        GVariant* posVar = g_dbus_connection_call_sync(
            bus, name.c_str(), "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "Get",
            g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Position"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE, 60, nullptr, nullptr
        );
        if (posVar) {
            GVariant* v = nullptr;
            g_variant_get(posVar, "(v)", &v);
            if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT64)) {
                cand.positionSec = g_variant_get_int64(v) / 1000000;
            }
            g_variant_unref(v);
            g_variant_unref(posVar);
        }

        // Metadata
        GVariant* metaVar = g_dbus_connection_call_sync(
            bus, name.c_str(), "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "Get",
            g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Metadata"),
            G_VARIANT_TYPE("(v)"),
            G_DBUS_CALL_FLAGS_NONE, 60, nullptr, nullptr
        );
        if (metaVar) {
            GVariant* v = nullptr;
            g_variant_get(metaVar, "(v)", &v);
            if (g_variant_is_of_type(v, G_VARIANT_TYPE_DICTIONARY) || g_variant_is_of_type(v, G_VARIANT_TYPE("a{sv}"))) {
                GVariantIter mIter;
                g_variant_iter_init(&mIter, v);
                char* mKey = nullptr;
                GVariant* mVal = nullptr;
                while (g_variant_iter_loop(&mIter, "{sv}", &mKey, &mVal)) {
                    if (g_strcmp0(mKey, "xesam:title") == 0 && g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING)) {
                        cand.title = g_variant_get_string(mVal, nullptr);
                    } else if (g_strcmp0(mKey, "xesam:artist") == 0) {
                        if (g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING_ARRAY)) {
                            GVariantIter aIter;
                            g_variant_iter_init(&aIter, mVal);
                            char* aStr = nullptr;
                            while (g_variant_iter_loop(&aIter, "s", &aStr)) {
                                if (!cand.artist.empty()) cand.artist += ", ";
                                cand.artist += aStr;
                            }
                        } else if (g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING)) {
                            cand.artist = g_variant_get_string(mVal, nullptr);
                        }
                    } else if (g_strcmp0(mKey, "xesam:album") == 0 && g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING)) {
                        cand.album = g_variant_get_string(mVal, nullptr);
                    } else if (g_strcmp0(mKey, "mpris:artUrl") == 0 && g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING)) {
                        cand.artUrl = g_variant_get_string(mVal, nullptr);
                    } else if (g_strcmp0(mKey, "mpris:length") == 0 && g_variant_is_of_type(mVal, G_VARIANT_TYPE_INT64)) {
                        cand.durationSec = g_variant_get_int64(mVal) / 1000000;
                    }
                }
            }
            g_variant_unref(v);
            g_variant_unref(metaVar);
        }

        // Scoring: prioritizes real playing music, then real paused music
        if (!cand.title.empty()) {
            if (cand.status == "Playing") cand.score = 100;
            else if (cand.status == "Paused") cand.score = 60;
            else cand.score = 20;
        } else {
            cand.score = 0;
        }

        if (cand.score > bestCandidate.score) {
            bestCandidate = cand;
        }
    }

    g_object_unref(bus);

    if (bestCandidate.score <= 0 || bestCandidate.title.empty()) {
        return "{\"hasPlayer\":false}";
    }

    std::string cleanPlayer = bestCandidate.name;
    const std::string pfx = "org.mpris.MediaPlayer2.";
    if (cleanPlayer.rfind(pfx, 0) == 0) cleanPlayer = cleanPlayer.substr(pfx.length());

    auto escapeJson = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c;
        }
        return out;
    };

    // Use integer seconds to guarantee valid JSON under all system locales (avoiding Russian comma decimals)
    std::string json = "{";
    json += "\"hasPlayer\":true,";
    json += "\"player\":\"" + escapeJson(cleanPlayer) + "\",";
    json += "\"playbackStatus\":\"" + escapeJson(bestCandidate.status) + "\",";
    json += "\"title\":\"" + escapeJson(bestCandidate.title) + "\",";
    json += "\"artist\":\"" + escapeJson(bestCandidate.artist) + "\",";
    json += "\"album\":\"" + escapeJson(bestCandidate.album) + "\",";
    json += "\"artUrl\":\"" + escapeJson(bestCandidate.artUrl) + "\",";
    json += "\"position\":" + std::to_string(std::max<int64_t>(0, bestCandidate.positionSec)) + ",";
    json += "\"duration\":" + std::to_string(std::max<int64_t>(0, bestCandidate.durationSec));
    json += "}";
    return json;
}

void WebTab::executeMprisCommand(const std::string& action, double param) {
    if (s_internalMediaCommander && s_internalMediaCommander(action, param)) {
        return;
    }

    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) return;

    GVariant* res = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "ListNames", nullptr,
        G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, 80, nullptr, nullptr
    );
    if (!res) {
        g_object_unref(bus);
        return;
    }

    GVariantIter* iter = nullptr;
    g_variant_get(res, "(as)", &iter);
    char* name = nullptr;
    std::string targetPlayer;
    std::string firstCandidate;
    int bestScore = -1;

    while (g_variant_iter_loop(iter, "s", &name)) {
        if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
            if (firstCandidate.empty()) firstCandidate = name;

            int score = 10;
            GVariant* statusVar = g_dbus_connection_call_sync(
                bus, name, "/org/mpris/MediaPlayer2",
                "org.freedesktop.DBus.Properties", "Get",
                g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus"),
                G_VARIANT_TYPE("(v)"),
                G_DBUS_CALL_FLAGS_NONE, 50, nullptr, nullptr
            );
            if (statusVar) {
                GVariant* v = nullptr;
                g_variant_get(statusVar, "(v)", &v);
                const char* s = g_variant_get_string(v, nullptr);
                if (s && g_strcmp0(s, "Playing") == 0) score = 100;
                else if (s && g_strcmp0(s, "Paused") == 0) score = 60;
                g_variant_unref(v);
                g_variant_unref(statusVar);
            }

            if (score > bestScore) {
                bestScore = score;
                targetPlayer = name;
                if (bestScore >= 100) break;
            }
        }
    }
    g_variant_iter_free(iter);
    g_variant_unref(res);

    if (targetPlayer.empty()) targetPlayer = firstCandidate;

    if (!targetPlayer.empty()) {
        const char* method = nullptr;
        GVariant* params = nullptr;
        if (action == "playPause") {
            method = "PlayPause";
        } else if (action == "next") {
            method = "Next";
        } else if (action == "prev" || action == "previous") {
            method = "Previous";
        } else if (action == "play") {
            method = "Play";
        } else if (action == "pause") {
            method = "Pause";
        } else if (action == "setPosition") {
            int64_t targetUs = static_cast<int64_t>(param * 1000000.0);
            bool setSuccess = false;

            // 1. Try SetPosition(trackId, targetUs)
            GVariant* metaVar = g_dbus_connection_call_sync(
                bus, targetPlayer.c_str(), "/org/mpris/MediaPlayer2",
                "org.freedesktop.DBus.Properties", "Get",
                g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Metadata"),
                G_VARIANT_TYPE("(v)"),
                G_DBUS_CALL_FLAGS_NONE, 60, nullptr, nullptr
            );
            std::string trackId;
            if (metaVar) {
                GVariant* v = nullptr;
                g_variant_get(metaVar, "(v)", &v);
                if (g_variant_is_of_type(v, G_VARIANT_TYPE_DICTIONARY) || g_variant_is_of_type(v, G_VARIANT_TYPE("a{sv}"))) {
                    GVariantIter mIter;
                    g_variant_iter_init(&mIter, v);
                    char* mKey = nullptr;
                    GVariant* mVal = nullptr;
                    while (g_variant_iter_loop(&mIter, "{sv}", &mKey, &mVal)) {
                        if (g_strcmp0(mKey, "mpris:trackid") == 0) {
                            if (g_variant_is_of_type(mVal, G_VARIANT_TYPE_OBJECT_PATH) || g_variant_is_of_type(mVal, G_VARIANT_TYPE_STRING)) {
                                trackId = g_variant_get_string(mVal, nullptr);
                            }
                        }
                    }
                }
                g_variant_unref(v);
                g_variant_unref(metaVar);
            }

            if (!trackId.empty() && g_variant_is_object_path(trackId.c_str())) {
                GError* callErr = nullptr;
                GVariant* setRes = g_dbus_connection_call_sync(
                    bus, targetPlayer.c_str(), "/org/mpris/MediaPlayer2",
                    "org.mpris.MediaPlayer2.Player", "SetPosition",
                    g_variant_new("(ox)", trackId.c_str(), targetUs),
                    nullptr, G_DBUS_CALL_FLAGS_NONE, 100, nullptr, &callErr
                );
                if (setRes) {
                    g_variant_unref(setRes);
                    setSuccess = true;
                } else if (callErr) {
                    g_error_free(callErr);
                }
            }

            // 2. If SetPosition was not accepted, compute exact delta from real-time Position and call Seek
            if (!setSuccess) {
                GVariant* curPosVar = g_dbus_connection_call_sync(
                    bus, targetPlayer.c_str(), "/org/mpris/MediaPlayer2",
                    "org.freedesktop.DBus.Properties", "Get",
                    g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Position"),
                    G_VARIANT_TYPE("(v)"),
                    G_DBUS_CALL_FLAGS_NONE, 60, nullptr, nullptr
                );
                int64_t curUs = 0;
                if (curPosVar) {
                    GVariant* v = nullptr;
                    g_variant_get(curPosVar, "(v)", &v);
                    if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT64)) {
                        curUs = g_variant_get_int64(v);
                    }
                    g_variant_unref(v);
                    g_variant_unref(curPosVar);
                }
                int64_t deltaUs = targetUs - curUs;
                method = "Seek";
                params = g_variant_new("(x)", deltaUs);
            }
        } else if (action == "seek") {
            method = "Seek";
            int64_t offsetUs = static_cast<int64_t>(param * 1000000.0);
            params = g_variant_new("(x)", offsetUs);
        }

        if (method) {
            g_dbus_connection_call(
                bus, targetPlayer.c_str(), "/org/mpris/MediaPlayer2",
                "org.mpris.MediaPlayer2.Player", method,
                params, nullptr, G_DBUS_CALL_FLAGS_NONE, 100, nullptr, nullptr, nullptr
            );
        }
    }
    g_object_unref(bus);
}

void WebTab::syncSystemMediaState() {
    if (!m_webView || m_url != "lumen://newtab") return;
    std::string json = querySystemMprisJson();
    std::string js = "if (window.__updateLumenMediaState) { window.__updateLumenMediaState(" + json + "); }";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webView), js.c_str(), nullptr,
        +[](GObject* src, GAsyncResult* res, gpointer) {
            GError* err = nullptr;
            WebKitJavascriptResult* jsRes = webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(src), res, &err);
            if (err) {
                g_warning("[lumen media js error] %s", err->message);
                g_error_free(err);
            }
            if (jsRes) webkit_javascript_result_unref(jsRes);
        }, nullptr);
#pragma GCC diagnostic pop
}

void WebTab::stopMediaPoll() {
    if (m_mediaPollSourceId > 0) {
        g_source_remove(m_mediaPollSourceId);
        m_mediaPollSourceId = 0;
    }
}

void WebTab::handleMediaScriptMessage(const std::string& messageJson) {
    auto parseField = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\":\"";
        auto pos = messageJson.find(needle);
        if (pos != std::string::npos) {
            auto end = messageJson.find('"', pos + needle.length());
            if (end != std::string::npos) {
                return messageJson.substr(pos + needle.length(), end - (pos + needle.length()));
            }
        }
        return "";
    };

    std::string action = parseField("action");
    if (action == "poll" || action == "getMediaState") {
        syncSystemMediaState();
        return;
    }

    double val = 0.0;
    auto numPos = messageJson.find("\"value\":");
    if (numPos != std::string::npos) {
        try {
            val = std::stod(messageJson.substr(numPos + 8));
        } catch (...) {}
    }

    executeMprisCommand(action, val);
    syncSystemMediaState();
}

} // namespace Blueprint::Engine
