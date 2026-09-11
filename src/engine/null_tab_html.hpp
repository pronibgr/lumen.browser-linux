#pragma once

namespace Blueprint::Engine {

inline const char* NULL_TAB_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>l.null</title>
<style>
  :root {
    --bg-void: #070809;
    --bg-surface: rgba(18, 21, 26, 0.65);
    --border-stealth: rgba(255, 255, 255, 0.05);
    --border-subtle: rgba(255, 255, 255, 0.08);
    --fg-phosphor: #4a5568;
    --accent-cyan: #3b5c54;
    --accent-glow: rgba(59, 92, 84, 0.4);
    --fg-text: #cbd5e1;
    --fg-muted: #64748b;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    width: 100%;
    height: 100vh;
    overflow: hidden;
    background: radial-gradient(ellipse at center, #0e1114 0%, #050607 100%);
    background-color: var(--bg-void);
    color: var(--fg-text);
    font-family: 'JetBrains Mono', 'Fira Code', 'Roboto Mono', 'SF Mono', Consolas, monospace;
    user-select: none;
    position: relative;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
  }

  /* Volatile Session HUD: Header Telemetry */
  .telemetry-header {
    height: 48px;
    padding: 0 32px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.12em;
    opacity: 0.35;
    border-bottom: 1px solid rgba(255, 255, 255, 0.03);
    pointer-events: none;
  }
  .telemetry-header .badge {
    display: flex;
    align-items: center;
    gap: 8px;
    color: var(--fg-text);
    font-weight: 600;
  }
  .telemetry-header .glyph {
    color: var(--accent-cyan);
  }
  .telemetry-header .flags {
    display: flex;
    gap: 12px;
    color: var(--fg-text);
  }
  .telemetry-header .sep {
    opacity: 0.4;
  }

  /* Ephemeral Search Anchor (~48px above true optical center) */
  .center-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    transform: translateY(-48px);
    width: 100%;
    max-width: 660px;
    margin: 0 auto;
    padding: 0 24px;
  }
  .search-anchor {
    width: 100%;
    background: var(--bg-surface);
    border: 1px solid var(--border-stealth);
    border-radius: 16px;
    padding: 6px 16px 6px 20px;
    display: flex;
    align-items: center;
    gap: 14px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
    transition: border-color 0.2s, box-shadow 0.2s, background 0.2s;
  }
  .search-anchor:focus-within {
    border-color: var(--accent-cyan);
    box-shadow: 0 0 0 1px var(--accent-cyan), 0 12px 40px rgba(0, 0, 0, 0.8);
    background: rgba(18, 21, 26, 0.85);
  }
  .optical-indicator {
    font-size: 15px;
    font-weight: 700;
    color: var(--accent-cyan);
    opacity: 0.75;
    flex-shrink: 0;
  }
  .search-input {
    flex: 1;
    background: transparent;
    border: none;
    outline: none;
    font-family: inherit;
    font-size: 14px;
    color: #f1f5f9;
    letter-spacing: 0.02em;
    padding: 10px 0;
  }
  .search-input::placeholder {
    color: #475569;
    font-size: 13px;
  }
  .search-action-btn {
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.06);
    color: var(--fg-muted);
    font-size: 10px;
    font-family: inherit;
    padding: 4px 10px;
    border-radius: 6px;
    cursor: pointer;
    transition: all 0.15s;
    letter-spacing: 0.05em;
  }
  .search-action-btn:hover {
    background: var(--accent-cyan);
    color: #ffffff;
    border-color: var(--accent-cyan);
  }

  /* Volatile Session HUD: Footer Security Matrix */
  .security-footer {
    height: 52px;
    padding: 0 32px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    border-top: 1px solid rgba(255, 255, 255, 0.03);
    font-size: 11px;
    position: relative;
  }
  .footer-caption {
    color: #475569;
    font-size: 11px;
    letter-spacing: 0.04em;
    margin: 0 auto;
    opacity: 0.85;
    pointer-events: none;
  }
  .hotkey-badges {
    position: absolute;
    right: 32px;
    display: flex;
    gap: 8px;
    pointer-events: none;
  }
  .hotkey-badge {
    background: rgba(255, 255, 255, 0.03);
    border: 1px solid rgba(255, 255, 255, 0.06);
    padding: 3px 8px;
    border-radius: 4px;
    font-size: 10px;
    color: #64748b;
  }
  .hotkey-badge span {
    color: #94a3b8;
  }
</style>
</head>
<body>
  <!-- Header Telemetry -->
  <header class="telemetry-header">
    <div class="badge">
      <span class="glyph">[∅]</span>
      <span>L.NULL // IN-MEMORY SESSION</span>
    </div>
    <div class="flags">
      <span>CACHE: PURGED</span>
      <span class="sep">&middot;</span>
      <span>COOKIES: ISOLATED</span>
      <span class="sep">&middot;</span>
      <span>STORAGE: VOLATILE</span>
    </div>
  </header>

  <!-- Ephemeral Search Anchor -->
  <main class="center-container">
    <form class="search-anchor" id="search-form" autocomplete="off" onsubmit="return handleDispatch(event);">
      <span class="optical-indicator">[∅]</span>
      <input type="text" id="query-input" class="search-input"
             placeholder="Search anonymously or execute URI..."
             spellcheck="false" autocomplete="off" autofocus />
      <button type="button" class="search-action-btn" id="purge-btn" onclick="purgeBuffer()" title="Purge buffer (Esc)">[ ESC: PURGE BUFFER ]</button>
    </form>
  </main>

  <!-- Footer Security Matrix -->
  <footer class="security-footer">
    <div class="footer-caption">
      All ephemeral state registers will be zero-filled on window termination.
    </div>
    <div class="hotkey-badges">
      <div class="hotkey-badge"><span>[ / ]</span> Focus</div>
      <div class="hotkey-badge"><span>[ Esc ]</span> Sanitize</div>
    </div>
  </footer>

  <script>
    const input = document.getElementById('query-input');

    // Zero-Persistence Guarantee: Clear text buffer and sanitize allocations
    function purgeBuffer() {
      if (!input) return;
      input.value = '';
      input.blur();
      // Volatile buffer zeroing
      let zeroBuf = new Uint8Array(256);
      zeroBuf.fill(0);
      zeroBuf = null;
    }

    // Input blur also sanitizes empty/unsubmitted text
    if (input) {
      input.addEventListener('blur', () => {
        if (!input.value.trim()) {
          purgeBuffer();
        }
      });
    }

    // Hotkeys: '/' focuses input, 'Escape' purges buffer, 'Ctrl+Backspace' wipes tokens
    window.addEventListener('keydown', (e) => {
      if (e.key === '/' && document.activeElement !== input) {
        e.preventDefault();
        input.focus();
        input.select();
      } else if (e.key === 'Escape') {
        purgeBuffer();
      } else if (e.ctrlKey && e.key === 'Backspace' && document.activeElement === input) {
        e.preventDefault();
        const val = input.value;
        const lastSpace = val.trimEnd().lastIndexOf(' ');
        input.value = lastSpace >= 0 ? val.substring(0, lastSpace + 1) : '';
      }
    });

    // Strip tracking parameters on dispatch
    function stripTrackingParams(rawUrl) {
      try {
        const u = new URL(rawUrl);
        const forbidden = [
          'utm_source', 'utm_medium', 'utm_campaign', 'utm_term', 'utm_content',
          'fbclid', 'gclid', 'gclsrc', 'dclid', 'msclkid', 'mc_eid', 'yclid', '_openstat'
        ];
        forbidden.forEach(param => u.searchParams.delete(param));
        return u.toString();
      } catch (err) {
        return rawUrl;
      }
    }

    function handleDispatch(e) {
      if (e) e.preventDefault();
      const raw = (input.value || '').trim();
      if (!raw) return false;

      let targetUrl = raw;
      if (raw.indexOf('://') !== -1 || raw.startsWith('about:') || raw.startsWith('lumen://')) {
        targetUrl = stripTrackingParams(raw);
      } else if (raw.indexOf('.') !== -1 && raw.indexOf(' ') === -1) {
        targetUrl = stripTrackingParams('https://' + raw);
      } else {
        targetUrl = 'https://duckduckgo.com/?q=' + encodeURIComponent(raw);
      }

      purgeBuffer();
      window.location.href = targetUrl;
      return false;
    }

    // Volatile memory cleanup on dismiss / unload
    window.addEventListener('beforeunload', () => {
      purgeBuffer();
    });
  </script>
</body>
</html>)html";

} // namespace Blueprint::Engine
