#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

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
    WebTab(int id, const std::string& url = "lumen://newtab", const std::string& title = "New Tab");
    ~WebTab();

    int  getId()   const { return m_id; }
    const std::string& getUrl()   const { return m_url; }
    const std::string& getTitle() const { return m_title; }
    float getLoadProgress() const { return m_loadProgress; }
    bool  isLoading()       const { return m_isLoading; }
    bool  isClosing()       const { return m_closing; }
    float getCloseProgress() const { return m_closeProgress; }

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

private:
    int  m_id = 0;
    std::string m_url;
    std::string m_title;
    float m_loadProgress = 1.f;
    bool  m_isLoading    = false;
    bool  m_loadFailed   = false;

    uint64_t m_siteDataBytes = 0;
    bool     m_siteDataKnown = false;

    bool  m_closing = false;
    float m_closeProgress = 0.f;

    GtkWidget* m_webView = nullptr;

    std::function<void(const std::string&)> m_onTitleChange;
    std::function<void(const std::string&)> m_onUrlChange;
    std::function<void(float)>              m_onProgressChange;
    std::function<void(uint64_t)>           m_onSiteDataChanged;

    void setupWebKitSignals();
    void loadNewTabHtml();
};

} // namespace Blueprint::Engine
