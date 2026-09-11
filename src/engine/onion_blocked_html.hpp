#pragma once

#include "theme/colors.hpp"
#include <string>

namespace Blueprint::Engine {

inline std::string getOnionBlockedHtml(const Theme::Palette& pal, const std::string& targetUrl) {
    std::string safeUrl = targetUrl.empty() ? "hidden-service.onion" : targetUrl;

    std::string html = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Onion Routing Disabled - lumen</title>
<style>
  :root {
    --bg-base: )html" + pal.bgBase.toCssHex() + R"html(;
    --bg-surface: )html" + pal.bgSurface.toCssHex() + R"html(;
    --border: rgba(255, 255, 255, 0.08);
    --border-hover: rgba(255, 255, 255, 0.18);
    --fg-primary: #F1F5F9;
    --fg-muted: #94A3B8;
    --fg-dim: #64748B;
    --onion-accent: #7AA2F7;
    --onion-glow: rgba(122, 162, 247, 0.25);
    --onion-dim: rgba(122, 162, 247, 0.12);
    --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    --font-mono: 'JetBrains Mono', Consolas, monospace;
  }

  * { box-sizing: border-box; margin: 0; padding: 0; }

  html, body {
    width: 100%;
    min-height: 100vh;
    background-color: var(--bg-base);
    color: var(--fg-primary);
    font-family: var(--font-sans);
    user-select: none;
    overflow-x: hidden;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .container {
    max-width: 580px;
    width: 90%;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 40px;
    box-shadow: 0 24px 60px rgba(0, 0, 0, 0.55), 0 0 0 1px rgba(255, 255, 255, 0.03);
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    animation: fadeIn 0.4s cubic-bezier(0.16, 1, 0.3, 1);
  }

  @keyframes fadeIn {
    from { opacity: 0; transform: translateY(12px) scale(0.98); }
    to { opacity: 1; transform: translateY(0) scale(1); }
  }

  /* Animated Glowing Onion Cut Icon */
  .icon-wrapper {
    width: 76px;
    height: 76px;
    border-radius: 24px;
    background: var(--onion-dim);
    border: 1px solid rgba(122, 162, 247, 0.3);
    display: flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 24px;
    position: relative;
    box-shadow: 0 0 30px var(--onion-glow);
    animation: pulse 3s infinite ease-in-out;
  }

  @keyframes pulse {
    0%, 100% { box-shadow: 0 0 24px var(--onion-glow); transform: scale(1); }
    50% { box-shadow: 0 0 40px rgba(122, 162, 247, 0.45); transform: scale(1.03); }
  }

  .onion-glyph {
    width: 42px;
    height: 42px;
    color: var(--onion-accent);
  }

  .status-badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    background: rgba(122, 162, 247, 0.12);
    border: 1px solid rgba(122, 162, 247, 0.28);
    color: var(--onion-accent);
    padding: 4px 12px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.06em;
    font-family: var(--font-mono);
    text-transform: uppercase;
    margin-bottom: 16px;
  }

  .status-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--onion-accent);
    box-shadow: 0 0 8px var(--onion-accent);
  }

  h1 {
    font-size: 22px;
    font-weight: 700;
    letter-spacing: -0.02em;
    margin-bottom: 12px;
    color: #FFFFFF;
  }

  p.description {
    font-size: 13px;
    line-height: 1.6;
    color: var(--fg-muted);
    margin-bottom: 24px;
    max-width: 480px;
  }

  .target-box {
    width: 100%;
    background: rgba(0, 0, 0, 0.25);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px 14px;
    font-family: var(--font-mono);
    font-size: 12px;
    color: #93C5FD;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    margin-bottom: 24px;
    text-align: left;
  }

  .actions {
    display: flex;
    gap: 12px;
    width: 100%;
    justify-content: center;
  }

  .btn-primary {
    background: var(--onion-accent);
    color: #07090E;
    font-weight: 600;
    font-size: 13px;
    padding: 10px 22px;
    border-radius: 8px;
    border: none;
    cursor: pointer;
    transition: all 0.18s ease;
    box-shadow: 0 4px 14px rgba(122, 162, 247, 0.35);
  }

  .btn-primary:hover {
    background: #90B4FC;
    transform: translateY(-1px);
    box-shadow: 0 6px 18px rgba(122, 162, 247, 0.45);
  }

  .btn-primary:active {
    transform: translateY(0);
  }

  .hint-box {
    margin-top: 24px;
    padding-top: 18px;
    border-top: 1px solid var(--border);
    width: 100%;
    font-size: 11px;
    color: var(--fg-dim);
    line-height: 1.5;
  }
</style>
</head>
<body>

<div class="container">
  <div class="icon-wrapper">
    <svg class="onion-glyph" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
      <path d="M8 3.5 C4.2 3.5 2 6.5 2 10.2 C2 13.2 4.5 15 8 15 C11.5 15 14 13.2 14 10.2 C14 6.5 11.8 3.5 8 3.5 Z" 
            stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"/>
      <path d="M8 6 C5.8 6 4.2 7.8 4.2 10.5 C4.2 12.5 5.8 13.6 8 13.6 C10.2 13.6 11.8 12.5 11.8 10.5 C11.8 7.8 10.2 6 8 6 Z" 
            stroke="currentColor" stroke-width="1.1" stroke-dasharray="2.5 1.2"/>
      <ellipse cx="8" cy="10.8" rx="1.6" ry="1.4" fill="currentColor"/>
      <path d="M8 3.5 V1 M6.5 1.8 L8 1 L9.5 1.8" 
            stroke="currentColor" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/>
    </svg>
  </div>

  <div class="status-badge">
    <span class="status-dot"></span>
    Tor Bridge Inactive
  </div>

  <h1>Onion Routing Disabled</h1>
  <p class="description">
    This destination belongs to the .onion hidden network. Direct clearnet access is blocked by default to prevent DNS leaks and preserve anonymity. Enable Tor routing in settings to establish an isolated circuit.
  </p>

  <div class="target-box" title=")html" + safeUrl + R"html(">
    )html" + safeUrl + R"html(
  </div>

  <div class="actions">
    <button class="btn-primary" onclick="openTorSettings()">Enable in Settings</button>
  </div>

  <div class="hint-box">
    Ensure the Tor daemon is running on port 9050 (<code>sudo systemctl start tor</code>) or Tor Browser on port 9150.
  </div>
</div>

<script>
  function openTorSettings() {
    try {
      if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.lumenMedia) {
        window.webkit.messageHandlers.lumenMedia.postMessage(JSON.stringify({
          action: "openSettings",
          section: "tor"
        }));
      }
    } catch (e) {
      console.error(e);
    }
  }
</script>

</body>
</html>)html";

    return html;
}

} // namespace Blueprint::Engine
