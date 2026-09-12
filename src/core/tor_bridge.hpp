#pragma once

#include <string>
#if !defined(LUMEN_NO_WEBKIT)
#include <webkit2/webkit2.h>
#endif

namespace Blueprint::Core::TorBridge {

struct OnionV3Parts {
    std::string proto;  // "http://" or "https://"
    std::string head;   // first 6 chars of onion hostname
    std::string body;   // middle dots/chars (or 46 dots)
    std::string tail;   // last 4 chars before .onion
    std::string tld;    // ".onion"
    std::string path;   // "/path?query"
    bool isV3 = false;
};

// Check whether a given URL or host targets a .onion hidden service
bool isOnionUrl(const std::string& url, std::string* outHost = nullptr);

// Parse and format a v3 onion address for structured omnibox presentation
OnionV3Parts parseOnionV3(const std::string& url);

// Non-blocking TCP connect probe to 127.0.0.1:port with strict timeout (default: 300ms)
bool probeTorDaemon(int port = 9050, int timeoutMs = 300);

#if !defined(LUMEN_NO_WEBKIT)
// Get or initialize the isolated ephemeral Tor WebKitWebContext
WebKitWebContext* getTorWebContext(int port = 9050);

// Invalidate / reset the cached context (e.g. if port changed or new session requested)
void resetTorWebContext();
#endif

// Persistent settings accessors (backed by Storage::Database)
bool isTorRoutingEnabled();
void setTorRoutingEnabled(bool enabled);

int getTorPort();
void setTorPort(int port);

} // namespace Blueprint::Core::TorBridge
