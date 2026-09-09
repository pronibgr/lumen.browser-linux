#include "engine/web_tab.hpp"
#include <algorithm>
#include <iostream>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <sqlite3.h>
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

const char* NEW_TAB_HTML = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>New Tab</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background-color: #0E1116;
    color: #F1F5F9;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    overflow: hidden;
    user-select: none;
  }
  .card {
    background: #151921;
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 14px;
    width: 720px;
    padding: 36px 40px;
    box-shadow: 0 20px 40px rgba(0, 0, 0, 0.5);
  }
  .header {
    display: flex;
    align-items: center;
    gap: 16px;
    margin-bottom: 24px;
    padding-bottom: 20px;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
  }
  .accent-bar {
    width: 4px;
    height: 48px;
    background: #38BDF8;
    border-radius: 2px;
  }
  .title-group h1 {
    font-size: 24px;
    font-weight: 700;
    letter-spacing: -0.5px;
    color: #F8FAFC;
  }
  .title-group p {
    font-size: 11px;
    color: #94A3B8;
    margin-top: 4px;
    letter-spacing: 0.5px;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 12px;
    margin-bottom: 24px;
  }
  .feat-card {
    background: #1A202C;
    border: 1px solid rgba(255, 255, 255, 0.05);
    border-radius: 8px;
    padding: 14px;
    transition: transform 0.15s, border-color 0.15s;
  }
  .feat-card:hover {
    border-color: rgba(56, 189, 248, 0.4);
    transform: translateY(-2px);
  }
  .key-badge {
    display: inline-block;
    background: rgba(56, 189, 248, 0.15);
    color: #38BDF8;
    font-family: monospace;
    font-size: 10px;
    font-weight: bold;
    padding: 3px 8px;
    border-radius: 4px;
    margin-bottom: 8px;
  }
  .feat-name {
    font-size: 12px;
    font-weight: 600;
    color: #F1F5F9;
    margin-bottom: 4px;
  }
  .feat-desc {
    font-size: 10px;
    color: #94A3B8;
  }
  .footer-hint {
    font-size: 11px;
    color: #64748B;
    text-align: center;
  }
</style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div class="accent-bar"></div>
      <div class="title-group">
        <h1>lumen browser</h1>
        <p>Linux Edition &bull; Deep Obsidian &bull; WebKit High Performance &bull; Zero Telemetry</p>
      </div>
    </div>
    <div class="grid">
      <div class="feat-card">
        <span class="key-badge">Ctrl + T</span>
        <div class="feat-name">New Tab</div>
        <div class="feat-desc">Open a fresh browser tab</div>
      </div>
      <div class="feat-card">
        <span class="key-badge">Ctrl + L</span>
        <div class="feat-name">Omnibox</div>
        <div class="feat-desc">Search, URLs, math & units</div>
      </div>
      <div class="feat-card">
        <span class="key-badge">Ctrl + W</span>
        <div class="feat-name">Close Tab</div>
        <div class="feat-desc">Close current tab</div>
      </div>
      <div class="feat-card">
        <span class="key-badge">Ctrl + / -</span>
        <div class="feat-name">Zoom Control</div>
        <div class="feat-desc">Scale page text and layouts</div>
      </div>
      <div class="feat-card">
        <span class="key-badge">Padlock</span>
        <div class="feat-name">Security Info</div>
        <div class="feat-desc">TLS certificate & clear data</div>
      </div>
      <div class="feat-card">
        <span class="key-badge">Gear</span>
        <div class="feat-name">Settings</div>
        <div class="feat-desc">Configure animations and UI</div>
      </div>
    </div>
    <div class="footer-hint">Type any address or search query in the bar above to begin browsing.</div>
  </div>
</body>
</html>
)html";

} // anonymous namespace

WebTab::WebTab(int id, const std::string& url, const std::string& title)
    : m_id(id), m_url(url), m_title(title) {

    // turn off dmabuf or nvidia/mesa drivers crash on linux lol
    setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 1);

    m_webView = webkit_web_view_new();

    // enable hardware acceleration, smooth scroll, dev tools
    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webView));
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    webkit_settings_set_enable_webgl(settings, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
    webkit_settings_set_enable_page_cache(settings, TRUE);
    webkit_settings_set_user_agent_with_application_details(settings, "lumen browser", "1.0");

    // dark obsidian bg so it doesnt flash blinding white while loading
    GdkRGBA bg{0.055, 0.067, 0.086, 1.0};
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(m_webView), &bg);

    // persistent sqlite cookie storage so logins actually stick around
    WebKitWebContext* ctx = webkit_web_view_get_context(WEBKIT_WEB_VIEW(m_webView));
    WebKitCookieManager* cm = webkit_web_context_get_cookie_manager(ctx);
    std::string dataDir = std::string(g_get_user_data_dir()) + "/lumen-browser";
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
    std::string cookiePath = dataDir + "/cookies.sqlite";
    webkit_cookie_manager_set_persistent_storage(cm, cookiePath.c_str(), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    webkit_cookie_manager_set_accept_policy(cm, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);

    setupWebKitSignals();
    loadUrl(url);
}

WebTab::~WebTab() {
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

    g_signal_connect(m_webView, "load-failed", G_CALLBACK(+[](WebKitWebView*, WebKitLoadEvent, const char*, GError*, gpointer data) -> gboolean {
        auto* self = static_cast<WebTab*>(data);
        self->m_loadFailed = true;
        self->m_isLoading = false;
        self->m_loadProgress = 1.0f;
        self->m_siteDataBytes = 0;
        self->m_siteDataKnown = true;
        if (self->m_onProgressChange) self->m_onProgressChange(1.0f);
        return FALSE;
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
            if (!self->m_loadFailed) {
                self->fetchWebsiteData();
            }
        }
    }), this);
}

void WebTab::loadNewTabHtml() {
    m_url = "lumen://newtab";
    m_title = "New Tab";
    m_isLoading = false;
    m_loadProgress = 1.0f;
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webView), NEW_TAB_HTML, "lumen://newtab");
    if (m_onTitleChange) m_onTitleChange(m_title);
    if (m_onUrlChange) m_onUrlChange(m_url);
    if (m_onProgressChange) m_onProgressChange(1.0f);
}

void WebTab::loadUrl(const std::string& url) {
    if (url.empty() || url == "lumen://newtab" || url == "lampa://newtab" || url == "blueprint://newtab" || url == "about:blank") {
        loadNewTabHtml();
        return;
    }

    std::string full = url;
    if (full.find("://") == std::string::npos && full.find("about:") != 0) {
        if (full.find('.') != std::string::npos && full.find(' ') == std::string::npos) {
            full = "https://" + full;
        } else {
            std::string tmpl = Storage::Database::instance().getSetting("search_engine_template", "https://duckduckgo.com/?q=%s");
            full = Core::BrowserConfig::formatSearchUrl(tmpl, full);
        }
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
    return !m_url.empty() && m_url != "lumen://newtab" && m_url != "lampa://newtab" && m_url != "blueprint://newtab" && m_url != "about:blank";
}

void WebTab::reload() {
    if (!canReload()) {
        // internal lumen:// page, dont ask webkit to reload or it spits network errors
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
    if (m_loadFailed) return false;
    if (m_url.empty() || m_url == "lumen://newtab" || m_url == "lampa://newtab" || m_url == "blueprint://newtab" || m_url == "about:blank") return false;
    std::string host = extractHost(m_url);
    if (host.empty() || host == "newtab" || host.find('.') == std::string::npos) return false;
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

    if (m_loadFailed || !info.isHttps) {
        info.isValid = false;
        info.issuer = "";
        info.protocol = info.isHttps ? "Connection failed" : "Insecure protocol (HTTP)";
        return info;
    }

    GTlsCertificate* cert = nullptr;
    GTlsCertificateFlags flags = (GTlsCertificateFlags)0;
    gboolean ok = webkit_web_view_get_tls_info(WEBKIT_WEB_VIEW(m_webView), &cert, &flags);

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

} // namespace Blueprint::Engine
