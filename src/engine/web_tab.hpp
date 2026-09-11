#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include "theme/colors.hpp"

namespace Blueprint::Engine {

struct TlsCertificateInfo {
    bool isHttps = false;
    bool isValid = false;
    std::string issuer = "Google Trust Services LLC";
    std::string subject;
    std::string protocol = "TLS 1.3 (X.509 256-bit)";
};

class WebTab {
public:
    WebTab(int id, const std::string& url = "lumen://newtab", const std::string& title = "New Tab",
           WebKitWebContext* context = nullptr, bool isEphemeral = false);
    ~WebTab();

    int  getId()   const { return m_id; }
    const std::string& getUrl()   const { return m_url; }
    const std::string& getTitle() const { return m_title; }
    float getLoadProgress() const { return m_loadProgress; }
    bool  isLoading()       const { return m_isLoading; }
    bool  isClosing()       const { return m_closing; }
    float getCloseProgress() const { return m_closeProgress; }
    bool  isEphemeral()     const { return m_isEphemeral; }

    void setLoadProgress(float p) { m_loadProgress = p; }
    void setIsLoading(bool v)     { m_isLoading = v; }
    void setTitle(const std::string& t) { m_title = t; }

    bool canGoBack()    const;
    bool canGoForward() const;
    bool canReload()    const;
    void goBack();
    void goForward();
    void reload();
    void loadUrl(const std::string& url);

    // Zooming
    double getZoomLevel() const;
    void   setZoomLevel(double z);
    void   zoomIn();
    void   zoomOut();
    void   resetZoom();
    void   handleScrollZoom(double dy);

    // TLS info
    TlsCertificateInfo getTlsInfo() const;

    // Website data clearing and real monitoring
    bool canClearData() const;
    uint64_t getSiteDataBytes() const { return m_siteDataBytes; }
    void fetchWebsiteData();
    void clearWebsiteData(std::function<void(bool success)> onComplete = nullptr);

    // User-Agent configuration
    static void setDefaultUserAgent(const std::string& ua);
    static std::string getDefaultUserAgent();
    void setUserAgent(const std::string& ua);
    std::string getUserAgent() const;

    // WebKit Widget
    GtkWidget* getWebView() const { return m_webView; }
    unsigned int getTexture() const { return 0; }

    // Closing animation
    void startClose();
    void updateClose(float dt);

    void setCallbacks(std::function<void(const std::string& title)> onTitle,
                      std::function<void(const std::string& url)> onUrl,
                      std::function<void(float progress)> onProgress);

    // callback when site storage size finishes fetching
    void setOnSiteDataChanged(std::function<void(uint64_t bytes)> cb) { m_onSiteDataChanged = cb; }

    // callback when page requests opening a new window/tab (e.g. video links with target="_blank", middle-click)
    void setOnNewTabRequested(std::function<void(const std::string& url, bool inBackground)> cb) { m_onNewTabRequested = cb; }

    // callback when HTML5 video enters or leaves fullscreen
    void setOnFullscreenToggled(std::function<void(bool fullscreen)> cb) { m_onFullscreenToggled = cb; }

    static std::string sanitizeTrackingParams(const std::string& url);

    void applyTheme(const Theme::Palette& pal);
    void loadErrorPage(const std::string& failingUri, const std::string& errorCode, const std::string& errorTitle, const std::string& errorDesc, const std::string& diagCode = "");
    bool isErrorPage() const { return m_isErrorPage; }

    // Audio playback status
    bool isPlayingAudio() const;

    // System MPRIS and Internal Media Integration
    using InternalMediaProvider = std::function<std::string()>;
    using InternalMediaCommander = std::function<bool(const std::string& action, double param)>;
    static void setInternalMediaProvider(InternalMediaProvider provider);
    static void setInternalMediaCommander(InternalMediaCommander commander);

    void syncSystemMediaState();
    void stopMediaPoll();
    static std::string querySystemMprisJson();
    static void executeMprisCommand(const std::string& action, double param);
    void handleMediaScriptMessage(const std::string& messageJson);

private:
    int  m_id = 0;
    bool m_isEphemeral = false;
    bool m_isErrorPage = false;
    std::string m_failedUri;
    std::string m_url;
    std::string m_title;
    float m_loadProgress = 1.f;
    bool  m_isLoading    = false;
    bool  m_loadFailed   = false;

    uint64_t m_siteDataBytes = 0;
    bool     m_siteDataKnown = false;

    guint m_mediaPollSourceId = 0;

    bool  m_closing = false;
    float m_closeProgress = 0.f;

    GtkWidget* m_webView = nullptr;
    WebKitUserContentManager* m_ucm = nullptr;

    std::function<void(const std::string&)> m_onTitleChange;
    std::function<void(const std::string&)> m_onUrlChange;
    std::function<void(float)>              m_onProgressChange;
    std::function<void(uint64_t)>           m_onSiteDataChanged;
    std::function<void(const std::string&, bool)> m_onNewTabRequested;
    std::function<void(bool)>               m_onFullscreenToggled;

    void setupWebKitSignals();
    void loadNewTabHtml();
    void loadNullTabHtml();
};

} // namespace Blueprint::Engine
