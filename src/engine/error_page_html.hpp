#pragma once
#include "theme/colors.hpp"
#include <string>
#include <ctime>

namespace Blueprint::Engine {

inline const char* ERROR_PAGE_HTML_TEMPLATE = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Connection Failure - lumen</title>
<style>
  :root {
    --bg-base: #0E1116;
    --bg-surface: #151921;
    --border: rgba(255, 255, 255, 0.08);
    --border-hover: rgba(255, 255, 255, 0.18);
    --fg-primary: #F1F5F9;
    --fg-muted: #94A3B8;
    --fg-dim: #64748B;
    --accent: #38BDF8;
    --accent-glow: rgba(56, 189, 248, 0.25);
    --accent-tint: rgba(56, 189, 248, 0.08);
    --danger: #E05060;
  }

  * { box-sizing: border-box; margin: 0; padding: 0; }

  html, body {
    width: 100%;
    min-height: 100vh;
    background-color: var(--bg-base);
    color: var(--fg-primary);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    user-select: none;
    overflow-x: hidden;
    transition: background-color 0.3s ease, color 0.3s ease;
  }

  /* Split composition wrapper */
  .error-wrapper {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 64px;
    min-height: 100vh;
    padding: 32px;
    box-sizing: border-box;
  }

  /* Left column: Circular Portal */
  .bulb-portal {
    width: 320px;
    height: 320px;
    border-radius: 50%;
    overflow: hidden;
    position: relative;
    flex-shrink: 0;
    background: radial-gradient(circle at center, var(--bg-surface) 0%, var(--bg-base) 100%);
    border: 1px solid var(--border);
    box-shadow: 0 16px 40px rgba(0, 0, 0, 0.4);
  }

  /* Canvas element inside portal */
  #view {
    position: absolute;
    left: 50%;
    top: 50%;
    transform: translate(-50%, calc(-50% + 10px));
    width: 360px;
    height: 480px;
    pointer-events: none;
  }

  /* Right column: Error Details Column (max-width: 440px) */
  .error-details {
    max-width: 440px;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  /* Status badge: var(--danger) with 10% opacity bg fill and 25% opacity border */
  .status-badge {
    color: var(--danger);
    background: rgba(224, 80, 96, 0.10);
    border: 1px solid rgba(224, 80, 96, 0.25);
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.05em;
    font-family: 'JetBrains Mono', Consolas, monospace;
    text-transform: uppercase;
    display: inline-flex;
    align-items: center;
    gap: 6px;
    width: fit-content;
  }
  .status-badge::before {
    content: "";
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--danger);
  }

  /* Headings: var(--fg-primary) */
  .error-title {
    font-size: 26px;
    font-weight: 700;
    letter-spacing: -0.02em;
    line-height: 1.25;
    color: var(--fg-primary);
  }

  /* Secondary & descriptive text: var(--fg-muted) */
  .error-desc {
    font-size: 13px;
    line-height: 1.6;
    color: var(--fg-muted);
  }

  /* Diagnostic panel: var(--bg-surface) bg, var(--fg-dim) labels, var(--fg-muted) values */
  .diagnostic-panel {
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 14px 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .diagnostic-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 11.5px;
    gap: 16px;
  }
  .diagnostic-label {
    color: var(--fg-dim);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    font-size: 10.5px;
    flex-shrink: 0;
  }
  .diagnostic-value {
    color: var(--fg-muted);
    font-family: 'JetBrains Mono', Consolas, monospace;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    text-align: right;
  }

  /* Action bar & buttons */
  .action-bar {
    display: flex;
    align-items: center;
    gap: 14px;
    margin-top: 4px;
  }
  /* Primary action button: var(--fg-primary) bg, var(--bg-base) text */
  .btn-retry {
    background: var(--fg-primary);
    color: var(--bg-base);
    font-size: 12px;
    font-weight: 700;
    border-radius: 8px;
    padding: 10px 22px;
    cursor: pointer;
    border: none;
    font-family: inherit;
    transition: opacity 0.15s, transform 0.15s;
    display: inline-flex;
    align-items: center;
    gap: 8px;
  }
  .btn-retry:hover {
    opacity: 0.92;
    transform: translateY(-1px);
  }
  .btn-retry:active {
    transform: scale(0.98);
  }

  .btn-home {
    background: transparent;
    color: var(--fg-muted);
    border: 1px solid var(--border);
    font-size: 12px;
    font-weight: 600;
    border-radius: 8px;
    padding: 9px 18px;
    cursor: pointer;
    font-family: inherit;
    transition: all 0.15s;
    text-decoration: none;
  }
  .btn-home:hover {
    color: var(--fg-primary);
    border-color: var(--border-hover);
    background: var(--bg-surface);
  }

  .keyboard-hints {
    font-size: 11px;
    color: var(--fg-dim);
    display: flex;
    align-items: center;
    gap: 6px;
    margin-top: 2px;
  }
  .keyboard-hints kbd {
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 2px 6px;
    font-size: 10px;
    color: var(--fg-muted);
    font-family: inherit;
  }
</style>
</head>
<body>
  <div class="error-wrapper">
    <!-- Left: Circular Portal -->
    <div class="bulb-portal">
      <canvas id="view" width="720" height="960"></canvas>
    </div>

    <!-- Right: Error Details Column -->
    <div class="error-details">
      <div class="status-badge" id="status-badge">{{ERROR_CODE}}</div>
      <h1 class="error-title" id="error-title">{{ERROR_TITLE}}</h1>
      <p class="error-desc" id="error-desc">{{ERROR_DESC}}</p>

      <div class="diagnostic-panel">
        <div class="diagnostic-row">
          <span class="diagnostic-label">Target Host</span>
          <span class="diagnostic-value" id="diag-host">{{TARGET_HOST}}</span>
        </div>
        <div class="diagnostic-row">
          <span class="diagnostic-label">Requested URI</span>
          <span class="diagnostic-value" id="diag-uri" title="{{FAILING_URI}}">{{FAILING_URI}}</span>
        </div>
        <div class="diagnostic-row">
          <span class="diagnostic-label">Diagnostic Code</span>
          <span class="diagnostic-value" id="diag-code">{{DIAG_CODE}}</span>
        </div>
        <div class="diagnostic-row">
          <span class="diagnostic-label">Timestamp</span>
          <span class="diagnostic-value" id="diag-time">{{TIMESTAMP}}</span>
        </div>
      </div>

      <div class="action-bar">
        <button type="button" class="btn-retry" id="retry-btn" onclick="retryNavigation()">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <path d="M21 2v6h-6"></path>
            <path d="M3 12a9 9 0 0 1 15.5-6.36L21 8"></path>
            <path d="M3 22v-6h6"></path>
            <path d="M21 12a9 9 0 0 1-15.5 6.36L3 16"></path>
          </svg>
          Try Again
        </button>
        <a href="lumen://newtab" class="btn-home">New Tab</a>
      </div>

      <div class="keyboard-hints">
        Press <kbd>Enter</kbd> or <kbd>Space</kbd> to retry
      </div>
    </div>
  </div>

  <script>
    window.__failedUri = "{{FAILING_URI_ESCAPED}}";

    // Dynamic Theme Reactivity Bridge
    window.__setLumenTheme = function(theme) {
      if (!theme) return;
      const root = document.documentElement;
      if (theme.bgBase) root.style.setProperty('--bg-base', theme.bgBase);
      if (theme.bgSurface) root.style.setProperty('--bg-surface', theme.bgSurface);
      if (theme.border) root.style.setProperty('--border', theme.border);
      if (theme.borderHover) root.style.setProperty('--border-hover', theme.borderHover);
      if (theme.fgPrimary) root.style.setProperty('--fg-primary', theme.fgPrimary);
      if (theme.fgMuted) root.style.setProperty('--fg-muted', theme.fgMuted);
      if (theme.fgDim) root.style.setProperty('--fg-dim', theme.fgDim);
      if (theme.danger) root.style.setProperty('--danger', theme.danger);
    };

    // Retry Navigation
    function retryNavigation() {
      const target = window.__failedUri;
      if (target && target.startsWith('http')) {
        window.location.href = target;
      } else {
        window.location.reload();
      }
    }

    // Keyboard handlers (Enter / Space)
    document.addEventListener('keydown', (e) => {
      if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA')) return;
      if (e.key === 'Enter' || e.key === ' ' || e.code === 'Space' || e.code === 'Enter') {
        e.preventDefault();
        retryNavigation();
      }
    });

    // ──────────────── Physics Filament Meltdown Engine ────────────────
    const canvas = document.getElementById("view");
    const ctx = canvas.getContext("2d");

    const CX = 360;
    const FILAMENT_Y = 380;
    const PIN_LEFT_X = 280;
    const PIN_RIGHT_X = 440;
    const STEM_BOTTOM_Y = 620;

    const SEGMENTS = 40;
    const BREAK_INDEX = Math.floor(SEGMENTS * 0.46);

    let nodes = [];
    let restLengths = [];
    let snapped = false;
    let time = 0;
    let energy = 0;
    let glow = 0;
    let flash = 0;

    function initFilament() {
      nodes = [];
      restLengths = [];
      snapped = false;
      time = 0;
      energy = 0;
      glow = 0;
      flash = 0;

      const spanX = PIN_RIGHT_X - PIN_LEFT_X;

      for (let i = 0; i <= SEGMENTS; i++) {
        const t = i / SEGMENTS;
        const x = PIN_LEFT_X + t * spanX;
        const microSag = Math.sin(t * Math.PI) * 3.5;
        const y = FILAMENT_Y + microSag;

        nodes.push({
          x: x,
          y: y,
          oldX: x,
          oldY: y,
          pinned: (i === 0 || i === SEGMENTS)
        });
      }

      for (let i = 0; i < SEGMENTS; i++) {
        const dist = Math.hypot(nodes[i + 1].x - nodes[i].x, nodes[i + 1].y - nodes[i].y);
        restLengths.push(dist);
      }
    }

    function stepPhysics(dt) {
      if (!snapped) return;

      const elapsed = Math.max(0, time - 2.0);
      const settleProgress = Math.min(1, elapsed / 1.1);
      const damping = 0.985 - settleProgress * 0.16;
      const gravity = 3400;

      for (let i = 0; i < nodes.length; i++) {
        const p = nodes[i];
        if (p.pinned) continue;

        let vx = (p.x - p.oldX) * damping;
        let vy = (p.y - p.oldY) * damping + gravity * dt * dt;

        if (elapsed > 0.8) {
          const stopRatio = Math.min(1, (elapsed - 0.8) / 0.7);
          vx *= (1 - stopRatio * 0.92);
          vy *= (1 - stopRatio * 0.92);

          if (Math.hypot(vx, vy) < 0.04) {
            vx = 0;
            vy = 0;
          }
        }

        p.oldX = p.x;
        p.oldY = p.y;
        p.x += vx;
        p.y += vy;
      }

      for (let iter = 0; iter < 14; iter++) {
        for (let i = 0; i < nodes.length - 1; i++) {
          if (i === BREAK_INDEX) continue;

          const p1 = nodes[i];
          const p2 = nodes[i + 1];
          const rest = restLengths[i];

          const dx = p2.x - p1.x;
          const dy = p2.y - p1.y;
          const dist = Math.hypot(dx, dy);
          if (dist === 0) continue;

          const diff = (dist - rest) / dist;
          const offsetX = dx * 0.5 * diff;
          const offsetY = dy * 0.5 * diff;

          if (!p1.pinned) {
            p1.x += offsetX;
            p1.y += offsetY;
          }
          if (!p2.pinned) {
            p2.x -= offsetX;
            p2.y -= offsetY;
          }
        }
      }
    }

    function getFilamentColor(temp) {
      if (temp <= 0.05) return "rgb(48, 54, 61)";
      if (temp < 0.35) {
        const t = temp / 0.35;
        return `rgb(${Math.round(48 + t * 140)}, ${Math.round(54 - t * 20)}, ${Math.round(61 - t * 45)})`;
      }
      if (temp < 0.75) {
        const t = (temp - 0.35) / 0.4;
        return `rgb(${Math.round(188 + t * 67)}, ${Math.round(34 + t * 120)}, ${Math.round(16 + t * 20)})`;
      }
      const t = (temp - 0.75) / 0.25;
      return `rgb(255, ${Math.round(154 + t * 90)}, ${Math.round(36 + t * 180)})`;
    }

    function update(dt) {
      time += dt;

      if (time < 1.0) {
        const t = time / 1.0;
        energy = Math.pow(t, 2.2);
        glow = energy * 0.9 + Math.sin(time * 65) * 0.015;
      } else if (time < 2.0) {
        energy = 1.0;
        glow = 1.0 + Math.sin(time * 80) * 0.012;
      } else {
        if (!snapped) {
          snapped = true;
          flash = 1.0;
        }
        const cooldown = time - 2.0;
        energy = Math.max(0, Math.exp(-cooldown * 4.8));
        glow = energy;
        flash = Math.max(0, flash - dt * 8.0);
      }

      stepPhysics(dt);
    }

    function drawRoundedRect(x, y, w, h, r) {
      ctx.beginPath();
      ctx.moveTo(x + r, y);
      ctx.arcTo(x + w, y, x + w, y + h, r);
      ctx.arcTo(x + w, y + h, x, y + h, r);
      ctx.arcTo(x, y + h, x, y, r);
      ctx.arcTo(x, y, x + w, y, r);
      ctx.closePath();
    }

    function render() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      if (glow > 0.02) {
        const radGrad = ctx.createRadialGradient(CX, FILAMENT_Y, 10, CX, FILAMENT_Y, 260);
        radGrad.addColorStop(0, `rgba(255, 185, 90, ${glow * 0.18})`);
        radGrad.addColorStop(0.4, `rgba(240, 110, 30, ${glow * 0.05})`);
        radGrad.addColorStop(1, "rgba(0, 0, 0, 0)");
        ctx.fillStyle = radGrad;
        ctx.beginPath();
        ctx.arc(CX, FILAMENT_Y, 260, 0, Math.PI * 2);
        ctx.fill();
      }

      const STROKE_COLOR = "rgba(180, 205, 225, 0.28)";
      const FILL_BASE_COLOR = "rgba(16, 21, 27, 0.75)";

      ctx.save();
      ctx.lineCap = "round";
      ctx.lineJoin = "round";

      // Bulb outline
      ctx.strokeStyle = STROKE_COLOR;
      ctx.lineWidth = 4;
      ctx.beginPath();
      ctx.arc(CX, 340, 160, Math.PI * 0.81, Math.PI * 0.19, false);
      ctx.bezierCurveTo(464, 520, 424, 600, 412, 656);
      ctx.moveTo(CX - 160 * Math.cos(Math.PI * 0.19), 340 + 160 * Math.sin(Math.PI * 0.19));
      ctx.bezierCurveTo(256, 520, 296, 600, 308, 656);
      ctx.stroke();

      // Glass mount
      ctx.strokeStyle = "rgba(180, 205, 225, 0.15)";
      ctx.lineWidth = 3;
      drawRoundedRect(336, 575, 48, 80, 14);
      ctx.stroke();

      // Support wires
      ctx.strokeStyle = "rgba(180, 205, 225, 0.26)";
      ctx.lineWidth = 3.5;

      ctx.beginPath();
      ctx.moveTo(346, 620);
      ctx.lineTo(346, 490);
      ctx.lineTo(PIN_LEFT_X, FILAMENT_Y);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(374, 620);
      ctx.lineTo(374, 490);
      ctx.lineTo(PIN_RIGHT_X, FILAMENT_Y);
      ctx.stroke();

      ctx.fillStyle = "rgba(215, 235, 255, 0.4)";
      ctx.beginPath();
      ctx.arc(PIN_LEFT_X, FILAMENT_Y, 3.5, 0, Math.PI * 2);
      ctx.arc(PIN_RIGHT_X, FILAMENT_Y, 3.5, 0, Math.PI * 2);
      ctx.fill();

      // Socket base
      ctx.lineWidth = 3.5;
      ctx.strokeStyle = STROKE_COLOR;
      ctx.fillStyle = FILL_BASE_COLOR;

      drawRoundedRect(304, 656, 112, 14, 7);
      ctx.fill();
      ctx.stroke();

      drawRoundedRect(308, 676, 104, 18, 9);
      ctx.fill();
      ctx.stroke();

      drawRoundedRect(308, 700, 104, 18, 9);
      ctx.fill();
      ctx.stroke();

      drawRoundedRect(326, 724, 68, 14, 7);
      ctx.fill();
      ctx.stroke();

      ctx.restore();

      // Filament line
      ctx.save();
      ctx.lineWidth = 2.2;
      ctx.lineCap = "round";
      ctx.lineJoin = "round";

      if (glow > 0.05) {
        ctx.shadowColor = `rgba(255, 170, 70, ${glow * 0.9})`;
        ctx.shadowBlur = 10 * glow;
      }

      ctx.strokeStyle = getFilamentColor(energy);

      ctx.beginPath();
      ctx.moveTo(nodes[0].x, nodes[0].y);
      const leftEnd = snapped ? BREAK_INDEX : nodes.length - 1;
      for (let i = 1; i <= leftEnd; i++) {
        ctx.lineTo(nodes[i].x, nodes[i].y);
      }
      ctx.stroke();

      if (snapped) {
        ctx.beginPath();
        ctx.moveTo(nodes[BREAK_INDEX + 1].x, nodes[BREAK_INDEX + 1].y);
        for (let i = BREAK_INDEX + 2; i < nodes.length; i++) {
          ctx.lineTo(nodes[i].x, nodes[i].y);
        }
        ctx.stroke();

        if (flash > 0.01) {
          ctx.fillStyle = `rgba(255, 250, 240, ${flash})`;
          ctx.beginPath();
          ctx.arc(nodes[BREAK_INDEX].x, nodes[BREAK_INDEX].y, 4 * flash, 0, Math.PI * 2);
          ctx.fill();
        }
      }
      ctx.restore();
    }

    let lastTime = performance.now();
    function loop(currentTime) {
      const dt = Math.min((currentTime - lastTime) / 1000, 0.033);
      lastTime = currentTime;

      update(dt);
      render();
      requestAnimationFrame(loop);
    }

    initFilament();
    requestAnimationFrame(loop);
  </script>
</body>
</html>
)html";

static inline std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

static inline std::string extractDomain(const std::string& uri) {
    if (uri.empty()) return "unknown";
    size_t start = uri.find("://");
    start = (start == std::string::npos) ? 0 : start + 3;
    size_t end = uri.find_first_of("/:?#", start);
    return (end == std::string::npos) ? uri.substr(start) : uri.substr(start, end - start);
}

static inline std::string getCurrentUtcTime() {
    std::time_t now = std::time(nullptr);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &now);
#else
    gmtime_r(&now, &tmUtc);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmUtc);
    return std::string(buf);
}

inline std::string getErrorPageHtml(
    const Theme::Palette& pal,
    const std::string& failingUri,
    const std::string& errorCode,
    const std::string& errorTitle,
    const std::string& errorDesc,
    const std::string& diagCode = "")
{
    std::string html = ERROR_PAGE_HTML_TEMPLATE;

    std::string rootVars =
        "    --bg-base: " + pal.bgBase.toCssRgba() + ";\n"
        "    --bg-surface: " + pal.bgSurface.toCssRgba() + ";\n"
        "    --border: " + pal.border.toCssRgba() + ";\n"
        "    --border-hover: " + pal.borderFocus.toCssRgba() + ";\n"
        "    --fg-primary: " + pal.textPrimary.toCssRgba() + ";\n"
        "    --fg-muted: " + pal.textMuted.toCssRgba() + ";\n"
        "    --fg-dim: " + pal.textDim.toCssRgba() + ";\n"
        "    --accent: " + pal.accent.toCssRgba() + ";\n"
        "    --accent-glow: " + pal.accent.toCssHex() + "40;\n"
        "    --accent-tint: " + pal.accent.toCssHex() + "15;\n"
        "    --danger: " + pal.danger.toCssRgba() + ";\n";

    size_t rootPos = html.find(":root {");
    if (rootPos != std::string::npos) {
        size_t closePos = html.find("}", rootPos);
        if (closePos != std::string::npos) {
            html.replace(rootPos + 7, closePos - (rootPos + 7), "\n" + rootVars);
        }
    }

    std::string host = extractDomain(failingUri);
    std::string timestamp = getCurrentUtcTime();
    std::string diag = diagCode.empty() ? errorCode : diagCode;

    html = replaceAll(html, "{{ERROR_CODE}}", errorCode);
    html = replaceAll(html, "{{ERROR_TITLE}}", errorTitle);
    html = replaceAll(html, "{{ERROR_DESC}}", errorDesc);
    html = replaceAll(html, "{{TARGET_HOST}}", host);
    html = replaceAll(html, "{{FAILING_URI}}", failingUri);
    html = replaceAll(html, "{{FAILING_URI_ESCAPED}}", replaceAll(failingUri, "\"", "\\\""));
    html = replaceAll(html, "{{DIAG_CODE}}", diag);
    html = replaceAll(html, "{{TIMESTAMP}}", timestamp);

    return html;
}

} // namespace Blueprint::Engine
