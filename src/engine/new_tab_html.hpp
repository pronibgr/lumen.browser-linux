#pragma once
#include "theme/colors.hpp"
#include <string>

namespace Blueprint::Engine {

inline const char* NEW_TAB_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>New Tab</title>
<style>
  :root {
    --bg-base: #0E1116;
    --bg-surface: #151921;
    --bg-subtle: #1C212B;
    --bg-active: #262D3A;
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
    height: 100vh;
    overflow: hidden;
    background-color: var(--bg-base);
    color: var(--fg-primary);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    user-select: none;
    position: relative;
    transition: background-color 0.3s ease, color 0.3s ease;
  }

  /* Scalable CSS background layer bound to theme variables */
  .canvas-backdrop {
    position: absolute;
    inset: 0;
    background: radial-gradient(circle at 50% 30%, var(--bg-surface) 0%, var(--bg-base) 80%);
    pointer-events: none;
    z-index: 0;
    transition: background 0.35s ease;
  }

  /* Dimmed peripheral frame during Edit Mode */
  .peripheral-frame {
    position: fixed;
    inset: 0;
    pointer-events: none;
    z-index: 40;
    opacity: 0;
    transition: opacity 300ms cubic-bezier(0.16, 1, 0.3, 1);
    box-shadow: inset 0 0 100px rgba(0, 0, 0, 0.85);
  }
  body.edit-mode .peripheral-frame {
    opacity: 1;
  }

  /* Desktop Viewport Wrapper (Left-aligned in edit mode, mirroring KDE Plasma) */
  #desktop-wrapper {
    position: fixed;
    top: 0;
    left: 0;
    bottom: 0;
    width: 100vw;
    height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 0;
    box-sizing: border-box;
    z-index: 5;
    transition: width 320ms cubic-bezier(0.16, 1, 0.3, 1),
                padding 320ms cubic-bezier(0.16, 1, 0.3, 1);
  }

  body.edit-mode #desktop-wrapper {
    width: calc(100vw - 360px);
    padding: 16px 28px 24px 28px;
    gap: 8px;
  }

  /* KDE Plasma style edit header bar above shrunk desktop */
  #desktop-edit-bar {
    width: 100%;
    height: 0;
    opacity: 0;
    overflow: hidden;
    pointer-events: none;
    display: flex;
    align-items: center;
    justify-content: space-between;
    transition: height 320ms cubic-bezier(0.16, 1, 0.3, 1),
                opacity 320ms cubic-bezier(0.16, 1, 0.3, 1);
    flex-shrink: 0;
    user-select: none;
  }

  body.edit-mode #desktop-edit-bar {
    height: 38px;
    opacity: 1;
    pointer-events: auto;
  }

  .edit-bar-left {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .edit-bar-title {
    font-size: 13px;
    font-weight: 700;
    color: var(--fg-primary);
    background: var(--bg-surface);
    border: 1px solid var(--border);
    padding: 5px 14px;
    border-radius: 8px;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
  }

  .edit-bar-subtitle {
    font-size: 12px;
    color: var(--fg-muted);
  }

  .edit-bar-right {
    display: flex;
    align-items: center;
  }

  .edit-bar-exit-btn {
    display: flex;
    align-items: center;
    gap: 8px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 6px 14px;
    color: var(--fg-primary);
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
    transition: background 0.15s, border-color 0.15s, color 0.15s, transform 0.15s;
  }

  .edit-bar-exit-btn:hover {
    background: var(--bg-subtle);
    border-color: var(--accent);
    color: var(--accent);
    transform: translateY(-1px);
  }

  /* Plasma-Style Viewport Zoom-Out */
  #canvas {
    position: relative;
    z-index: 1;
    width: 100%;
    height: 100%;
    display: grid;
    grid-template-rows: minmax(0, 0.82fr) auto minmax(0, 1.18fr);
    grid-template-columns: minmax(0, 1fr) minmax(460px, 640px) minmax(0, 1fr);
    gap: 16px;
    padding: 24px 32px;
    box-sizing: border-box;
    transition: transform 320ms cubic-bezier(0.16, 1, 0.3, 1),
                border-radius 320ms cubic-bezier(0.16, 1, 0.3, 1),
                border-color 320ms cubic-bezier(0.16, 1, 0.3, 1),
                box-shadow 320ms cubic-bezier(0.16, 1, 0.3, 1),
                background-color 320ms ease;
    transform-origin: center center;
    border-radius: 0;
    border: 1px solid transparent;
    box-shadow: none;
  }

  body.edit-mode #canvas {
    border-radius: 18px;
    border-color: var(--border);
    background-color: var(--bg-base);
    box-shadow: 0 24px 64px rgba(0, 0, 0, 0.65), 0 0 0 1px var(--border);
    transform: scale(0.97);
    overflow: hidden;
  }

  /* 6 Grid Drop Zones (Strict Static Sizing) */
  .drop-zone {
    position: relative;
    border-radius: 14px;
    padding: 0;
    min-width: 0;
    min-height: 0;
    width: 100%;
    height: 100%;
    overflow: hidden;
    border: 2px dashed transparent;
    transition: border-color 0.2s, background-color 0.2s, box-shadow 0.2s;
    box-sizing: border-box;
  }
  body.edit-mode .drop-zone {
    border-color: var(--border-hover);
    background-image: radial-gradient(var(--border) 1.2px, transparent 1.2px);
    background-size: 16px 16px;
    background-position: 8px 8px;
  }
  body.edit-mode .drop-zone.drag-over {
    border-color: var(--accent) !important;
    background-color: var(--accent-tint) !important;
    box-shadow: inset 0 0 16px var(--accent-glow);
  }
  body.edit-mode .drop-zone.drag-error,
  body.edit-mode .drop-zone.zone-full {
    border-color: var(--danger) !important;
    background-color: rgba(239, 68, 68, 0.15) !important;
    box-shadow: inset 0 0 20px rgba(239, 68, 68, 0.35), 0 0 16px rgba(239, 68, 68, 0.25) !important;
  }

  #zone-top-left      { grid-row: 1; grid-column: 1; }
  #zone-top-center    { grid-row: 1; grid-column: 2; }
  #zone-top-right     { grid-row: 1; grid-column: 3; }
  #zone-bottom-left   { grid-row: 3; grid-column: 1; }
  #zone-bottom-center { grid-row: 3; grid-column: 2; }
  #zone-bottom-right  { grid-row: 3; grid-column: 3; }

  /* Static Search Bar (Centered, Natural Track Alignment) */
  .search-container {
    grid-row: 2;
    grid-column: 2;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    transform: none;
    margin: 4px 0 8px 0;
    z-index: 30;
    position: relative;
    pointer-events: auto;
  }
  .search-anchor {
    width: 100%;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 6px 16px 6px 20px;
    display: flex;
    align-items: center;
    gap: 12px;
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.45);
    transition: border-color 0.2s, box-shadow 0.2s, background-color 0.2s;
    position: relative;
  }
  .search-anchor:focus-within {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent), 0 16px 40px rgba(0, 0, 0, 0.6);
  }
  .search-icon {
    width: 18px;
    height: 18px;
    color: var(--fg-muted);
    flex-shrink: 0;
  }
  .search-input {
    flex: 1;
    background: transparent;
    border: none;
    outline: none;
    font-size: 15px;
    color: var(--fg-primary);
    font-family: inherit;
    padding: 8px 0;
  }
  .search-input::placeholder {
    color: var(--fg-muted);
    font-size: 14px;
  }

  /* History Popover */
  .search-history-dropdown {
    position: absolute;
    top: calc(100% + 8px);
    left: 0;
    right: 0;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 8px 0;
    box-shadow: 0 16px 36px rgba(0, 0, 0, 0.65);
    display: none;
    z-index: 50;
    backdrop-filter: blur(12px);
  }
  .search-history-dropdown.open {
    display: block;
  }
  .history-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 4px 16px 6px 16px;
    border-bottom: 1px solid var(--border);
    font-size: 11px;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }
  .clear-history-btn {
    background: transparent;
    border: none;
    color: var(--accent);
    font-size: 11px;
    cursor: pointer;
    font-family: inherit;
  }
  .clear-history-btn:hover {
    text-decoration: underline;
  }
  .history-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 16px;
    font-size: 13px;
    color: var(--fg-primary);
    cursor: pointer;
    transition: background 0.12s, color 0.12s;
  }
  .history-item:hover {
    background: var(--accent-tint);
    color: var(--accent);
  }
  .history-item svg {
    width: 14px;
    height: 14px;
    color: var(--fg-muted);
  }

  /* Widget Container */
  .widget {
    position: absolute;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 14px;
    overflow: hidden;
    contain: layout paint;
    min-width: 120px;
    min-height: 80px;
    max-width: 100% !important;
    max-height: 100% !important;
    box-sizing: border-box;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);
    transition: box-shadow 0.24s cubic-bezier(0.16, 1, 0.3, 1),
                border-color 0.2s,
                background-color 0.2s,
                transform 0.24s cubic-bezier(0.16, 1, 0.3, 1),
                opacity 0.2s ease;
    user-select: none;
  }
  body.edit-mode .widget {
    cursor: grab;
  }
  body.edit-mode .widget:active,
  .widget.is-dragging {
    cursor: grabbing !important;
  }
  /* In-zone dragging: slightly transparent, normal scale, pristine internal layout */
  .widget.is-dragging {
    cursor: grabbing !important;
    opacity: 0.8 !important;
    z-index: 999999 !important;
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.35) !important;
  }
  /* Only pointer-events: none on children so they do not capture cursor during flight.
     NEVER apply box-shadow, borders, or transforms to child elements! */
  .widget.is-dragging *,
  .widget.widget-lifted * {
    pointer-events: none !important;
  }
  /* Lifted state (Flight Elevation): ONLY when widget is outside any zone or dragging from catalog */
  .widget.widget-lifted {
    transform: scale(1.04) translateY(-4px) !important;
    box-shadow: 0 22px 48px rgba(0, 0, 0, 0.6) !important;
    opacity: 0.88 !important;
  }
  body.edit-mode .widget:hover {
    border-color: var(--accent);
    box-shadow: 0 10px 28px rgba(0, 0, 0, 0.5);
  }
  .widget.is-centering {
    transition: left 0.32s cubic-bezier(0.16, 1, 0.3, 1), transform 0.16s ease-out !important;
  }

  /* Ghost/Drop Preview for widget placement */
  .widget-drop-preview {
    position: absolute;
    pointer-events: none !important;
    border: 2px dashed var(--accent);
    background-color: var(--accent-tint);
    border-radius: 14px;
    box-shadow: 0 0 24px var(--accent-glow), inset 0 0 16px var(--accent-glow);
    z-index: 20;
    box-sizing: border-box;
    will-change: left, top, width, height;
    animation: blueprint-pulse 1.8s ease-in-out infinite alternate;
  }
  .widget-drop-preview.preview-invalid {
    border-color: var(--danger) !important;
    background-color: rgba(239, 68, 68, 0.16) !important;
    box-shadow: 0 0 24px rgba(239, 68, 68, 0.4), inset 0 0 16px rgba(239, 68, 68, 0.25) !important;
    animation: none !important;
  }
  @keyframes blueprint-pulse {
    0% {
      border-color: var(--accent);
      box-shadow: 0 0 16px var(--accent-glow), inset 0 0 12px var(--accent-glow);
      opacity: 0.85;
    }
    100% {
      border-color: var(--accent-hover);
      box-shadow: 0 0 28px var(--accent-glow), inset 0 0 22px var(--accent-glow);
      opacity: 1;
    }
  }

  /* Widget Controls in Edit Mode */
  .widget-controls {
    display: none;
    position: absolute;
    inset: 0;
    pointer-events: none;
    z-index: 25;
  }
  body.edit-mode .widget .widget-controls {
    display: block;
  }
  .widget-btn-gear {
    position: absolute;
    bottom: 8px;
    right: 8px;
    width: 26px;
    height: 26px;
    border-radius: 6px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    color: var(--fg-muted);
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    pointer-events: auto;
    transition: color 0.15s, background 0.15s, border-color 0.15s;
  }
  .widget-btn-gear:hover {
    color: var(--accent);
    background: var(--bg-subtle);
    border-color: var(--accent);
  }
  .widget-btn-center {
    position: absolute;
    bottom: 8px;
    left: 8px;
    width: 26px;
    height: 26px;
    border-radius: 6px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    color: var(--fg-muted);
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    pointer-events: auto;
    transition: color 0.15s, background 0.15s, border-color 0.15s, transform 0.15s;
  }
  .widget-btn-center:hover {
    color: var(--accent);
    background: var(--bg-subtle);
    border-color: var(--accent);
    transform: scale(1.08);
  }
  .widget-btn-center:active {
    transform: scale(0.92);
  }
  .widget-btn-remove {
    position: absolute;
    top: 8px;
    right: 8px;
    width: 24px;
    height: 24px;
    border-radius: 6px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    color: var(--danger);
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 14px;
    cursor: pointer;
    pointer-events: auto;
    transition: all 0.15s;
  }
  .widget-btn-remove:hover {
    background: var(--danger);
    color: #ffffff;
    border-color: var(--danger);
  }

  /* Transform Mode (8 Vector Anchors) */
  .widget.transform-mode {
    outline: 2px solid var(--accent);
    box-shadow: 0 0 0 4px var(--accent-glow);
  }
  /* Active resizing: smooth hardware response without sudden jerkiness */
  body.is-resizing .widget.transform-mode,
  .widget.is-resizing-active {
    transition: width 0.1s cubic-bezier(0.16, 1, 0.3, 1),
                height 0.1s cubic-bezier(0.16, 1, 0.3, 1),
                left 0.1s cubic-bezier(0.16, 1, 0.3, 1),
                top 0.1s cubic-bezier(0.16, 1, 0.3, 1) !important;
    will-change: left, top, width, height;
  }
  .transform-handle {
    display: none;
    position: absolute;
    width: 10px;
    height: 10px;
    background: var(--accent);
    border: 1px solid var(--bg-surface);
    border-radius: 2px;
    z-index: 35;
    pointer-events: auto;
  }
  .widget.transform-mode .transform-handle {
    display: block;
    animation: handle-pop 0.18s cubic-bezier(0.16, 1, 0.3, 1) forwards;
  }
  @keyframes handle-pop {
    from { transform: scale(0.3); opacity: 0; }
    to { transform: scale(1); opacity: 1; }
  }
  .handle-nw { top: -5px; left: -5px; cursor: nwse-resize; }
  .handle-n  { top: -5px; left: calc(50% - 5px); cursor: ns-resize; }
  .handle-ne { top: -5px; right: -5px; cursor: nesw-resize; }
  .handle-e  { top: calc(50% - 5px); right: -5px; cursor: ew-resize; }
  .handle-se { bottom: -5px; right: -5px; cursor: nwse-resize; }
  .handle-s  { bottom: -5px; left: calc(50% - 5px); cursor: ns-resize; }
  .handle-sw { bottom: -5px; left: -5px; cursor: nesw-resize; }
  .handle-w  { top: calc(50% - 5px); left: -5px; cursor: ew-resize; }

  /* ──────────────── Alpha Widget 1: Sleek Clock ──────────────── */
  .widget-clock {
    padding: 16px 22px;
    background: linear-gradient(135deg, var(--bg-surface) 0%, var(--bg-subtle) 100%);
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    width: 220px;
    height: 110px;
  }
  .clock-time {
    font-size: 38px;
    font-weight: 700;
    letter-spacing: -0.04em;
    font-variant-numeric: tabular-nums;
    color: var(--fg-primary);
    line-height: 1.1;
  }
  .clock-time.font-sans {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
  }
  .clock-time.font-mono {
    font-family: 'JetBrains Mono', Consolas, monospace;
  }
  .clock-time .sec {
    font-size: 20px;
    font-weight: 400;
    color: var(--accent);
    margin-left: 2px;
  }
  .clock-time .ampm {
    font-size: 14px;
    font-weight: 600;
    color: var(--fg-muted);
    margin-left: 6px;
    letter-spacing: 0.05em;
  }
  .clock-date {
    font-size: 11px;
    color: var(--fg-muted);
    letter-spacing: 0.05em;
    margin-top: 6px;
    text-transform: uppercase;
  }

  /* ──────────────── Alpha Widget 2: Live Weather Monitor ──────────── */
  .widget-weather {
    padding: 16px 20px;
    background: linear-gradient(135deg, var(--bg-surface) 0%, var(--bg-subtle) 100%);
    display: flex;
    align-items: center;
    gap: 16px;
    width: 260px;
    height: 110px;
  }
  .widget-weather.compact {
    padding: 12px 16px;
    gap: 12px;
  }
  .weather-icon-box {
    width: 44px;
    height: 44px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--accent);
    flex-shrink: 0;
  }
  .weather-info {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .weather-temp {
    font-size: 28px;
    font-weight: 700;
    color: var(--fg-primary);
    line-height: 1;
  }
  .weather-cond {
    font-size: 12px;
    color: var(--fg-muted);
    font-weight: 500;
  }
  .weather-loc {
    font-size: 10px;
    color: var(--fg-dim);
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  /* ──────────────── Alpha Widget 3: Custom HTML Embed ─────────────── */
  .widget-embed {
    width: 320px;
    height: 140px;
    display: flex;
    position: relative;
    background: var(--bg-base);
    overflow: hidden;
  }
  .widget-embed .embed-inner {
    width: 100%;
    height: 100%;
    position: relative;
    display: flex;
  }
  .widget-embed iframe {
    width: 100% !important;
    height: 100% !important;
    border: none;
    display: block;
    box-sizing: border-box;
    flex: 1;
  }
  body.edit-mode .widget-embed iframe,
  .widget.transform-mode iframe,
  body.is-resizing iframe {
    pointer-events: none !important;
  }

  /* ──────────────── Alpha Widget 4: Notes Window ─────────────────── */
  .widget-notes {
    width: 280px;
    height: 180px;
    display: flex;
    position: relative;
    background: var(--bg-surface);
    overflow: hidden;
  }
  .notes-inner {
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-sizing: border-box;
  }

  /* macOS Window Theme */
  .notes-inner.theme-macos {
    background: var(--bg-surface);
  }
  .notes-inner.theme-macos .notes-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 32px;
    padding: 0 10px;
    background: var(--bg-subtle);
    border-bottom: 1px solid var(--border);
    position: relative;
    flex-shrink: 0;
    user-select: none;
  }
  .notes-inner.theme-macos .macos-dots {
    display: flex;
    align-items: center;
    gap: 6px;
    z-index: 2;
  }
  .notes-inner.theme-macos .macos-dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    box-sizing: border-box;
    border: 0.5px solid rgba(0, 0, 0, 0.2);
  }
  .notes-inner.theme-macos .dot-red    { background: #ff5f56; }
  .notes-inner.theme-macos .dot-yellow { background: #ffbd2e; }
  .notes-inner.theme-macos .dot-green  { background: #27c93f; }
  .notes-inner.theme-macos .notes-title {
    position: absolute;
    left: 0;
    right: 0;
    text-align: center;
    font-size: 11.5px;
    font-weight: 600;
    color: var(--fg-muted);
    letter-spacing: 0.3px;
    pointer-events: none;
  }

  /* Cyber Terminal Theme */
  .notes-inner.theme-cyber {
    background: #0b0f14;
    border: 1px solid var(--accent);
    box-shadow: inset 0 0 14px rgba(56, 189, 248, 0.08);
  }
  .notes-inner.theme-cyber .notes-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 30px;
    padding: 0 10px;
    background: rgba(56, 189, 248, 0.08);
    border-bottom: 1px solid var(--accent);
    flex-shrink: 0;
  }
  .notes-inner.theme-cyber .cyber-tag {
    font-size: 10.5px;
    font-weight: 700;
    color: var(--accent);
    letter-spacing: 0.8px;
    display: flex;
    align-items: center;
    gap: 6px;
    font-family: monospace;
  }
  .notes-inner.theme-cyber .cyber-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 6px var(--accent);
    animation: cyber-glow 1.6s ease-in-out infinite alternate;
  }
  @keyframes cyber-glow {
    0% { opacity: 0.4; }
    100% { opacity: 1; }
  }
  .notes-inner.theme-cyber textarea,
  .notes-inner.theme-cyber .notes-markdown-view {
    color: var(--accent);
    font-family: monospace;
  }

  /* Amber Sticky Memo Theme */
  .notes-inner.theme-parchment {
    background: linear-gradient(180deg, rgba(245, 158, 11, 0.08) 0%, var(--bg-surface) 100%);
    border-top: 3px solid #f59e0b;
  }
  .notes-inner.theme-parchment .notes-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 28px;
    padding: 0 10px;
    background: rgba(245, 158, 11, 0.05);
    border-bottom: 1px solid rgba(245, 158, 11, 0.18);
    flex-shrink: 0;
  }
  .notes-inner.theme-parchment .notes-title {
    font-size: 11px;
    font-weight: 700;
    color: #f59e0b;
    letter-spacing: 0.3px;
  }
  .notes-inner.theme-parchment textarea {
    color: var(--fg-primary);
  }

  /* Obsidian Frosted Glass Theme */
  .notes-inner.theme-glass {
    background: rgba(255, 255, 255, 0.03);
    backdrop-filter: blur(16px);
    border: 1px solid rgba(255, 255, 255, 0.12);
  }
  .notes-inner.theme-glass .notes-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 30px;
    padding: 0 10px;
    background: rgba(255, 255, 255, 0.05);
    border-bottom: 1px solid rgba(255, 255, 255, 0.08);
    flex-shrink: 0;
  }
  .notes-inner.theme-glass .notes-title {
    font-size: 11.5px;
    font-weight: 600;
    color: var(--fg-primary);
    letter-spacing: 0.3px;
  }

  /* Notes Editor & Markdown Body */
  .notes-body {
    flex: 1;
    position: relative;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    padding: 8px 10px;
    box-sizing: border-box;
    min-height: 0;
  }
  .notes-textarea {
    width: 100%;
    height: 100%;
    border: none;
    outline: none;
    resize: none;
    background: transparent;
    color: var(--fg-primary);
    font-family: inherit;
    font-size: 13px;
    line-height: 1.5;
    padding: 0;
    margin: 0;
    box-sizing: border-box;
    overflow-y: auto;
  }
  .notes-textarea::placeholder {
    color: var(--fg-muted);
    opacity: 0.6;
  }
  .notes-md-toggle {
    z-index: 5;
    border: 1px solid var(--border);
    background: var(--bg-surface);
    color: var(--fg-muted);
    border-radius: 4px;
    font-size: 9.5px;
    font-weight: 700;
    padding: 2px 6px;
    cursor: pointer;
    transition: all 0.15s;
    line-height: 1;
    display: flex;
    align-items: center;
  }
  .notes-md-toggle:hover {
    border-color: var(--accent);
    color: var(--accent);
  }
  .notes-markdown-view {
    width: 100%;
    height: 100%;
    overflow-y: auto;
    line-height: 1.5;
    color: var(--fg-primary);
    word-break: break-word;
    box-sizing: border-box;
  }
  .notes-markdown-view h1, .notes-markdown-view h2, .notes-markdown-view h3 {
    margin: 4px 0 6px 0;
    color: var(--accent);
    font-weight: 700;
  }
  .notes-markdown-view h1 { font-size: 1.25em; }
  .notes-markdown-view h2 { font-size: 1.15em; }
  .notes-markdown-view h3 { font-size: 1.05em; }
  .notes-markdown-view p { margin: 0 0 6px 0; }
  .notes-markdown-view ul { margin: 0 0 6px 0; padding-left: 18px; }
  .notes-markdown-view code {
    background: var(--bg-subtle);
    border: 1px solid var(--border);
    padding: 1px 4px;
    border-radius: 4px;
    font-family: monospace;
    font-size: 0.9em;
  }
  .notes-markdown-view pre {
    background: var(--bg-subtle);
    border: 1px solid var(--border);
    padding: 6px 8px;
    border-radius: 6px;
    overflow-x: auto;
    margin: 4px 0 6px 0;
  }
  .notes-markdown-view pre code {
    background: transparent;
    border: none;
    padding: 0;
  }
  .notes-markdown-view blockquote {
    border-left: 3px solid var(--accent);
    margin: 4px 0 6px 0;
    padding-left: 8px;
    color: var(--fg-muted);
  }

  /* ──────────────── Media Player Widget (Now Playing) ──────────────── */
  .widget-media {
    background: var(--bg-surface);
    border-radius: 22px !important;
    box-shadow: 0 12px 36px rgba(0, 0, 0, 0.45);
    overflow: hidden;
  }
  .media-container {
    width: 100%;
    height: 100%;
    box-sizing: border-box;
    padding: 12px 14px 12px 14px;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    user-select: none;
    background: radial-gradient(circle at 15% 15%, rgba(255, 255, 255, 0.05) 0%, transparent 65%);
  }

  /* Top row */
  .media-top-row {
    display: flex;
    align-items: center;
    gap: 12px;
    position: relative;
    width: 100%;
    min-height: 48px;
  }
  .media-art-wrap {
    width: 48px;
    height: 48px;
    border-radius: 12px;
    overflow: hidden;
    flex-shrink: 0;
    position: relative;
    background: #18181b;
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.45);
    transition: opacity 0.22s cubic-bezier(0.16, 1, 0.3, 1), transform 0.22s cubic-bezier(0.16, 1, 0.3, 1);
  }
  .media-art-img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
    border-radius: 12px;
  }
  .media-art-placeholder {
    width: 100%;
    height: 100%;
    display: flex;
    align-items: center;
    justify-content: center;
    background: linear-gradient(135deg, #312e81 0%, #831843 100%);
    border-radius: 12px;
  }

  /* Title & Artist */
  .media-meta {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    justify-content: center;
    transition: opacity 0.22s cubic-bezier(0.16, 1, 0.3, 1), transform 0.22s cubic-bezier(0.16, 1, 0.3, 1);
  }
  .media-container.is-track-transitioning .media-meta,
  .media-container.is-track-transitioning .media-art-wrap {
    opacity: 0.18;
    transform: translateY(3px) scale(0.97);
  }
  .media-title {
    font-size: clamp(14px, 1.6vw, 17px);
    font-weight: 700;
    color: var(--fg-primary);
    line-height: 1.25;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    letter-spacing: -0.2px;
  }
  .media-artist-marquee-wrapper {
    position: relative;
    width: 100%;
    overflow: hidden;
    margin-top: 3px;
    height: 18px;
  }
  .media-artist {
    display: inline-block;
    font-size: 13px;
    font-weight: 400;
    color: var(--fg-muted);
    opacity: 0.82;
    white-space: nowrap;
    line-height: 18px;
    transform: translateX(0);
    will-change: transform;
  }
  /* Ping-pong marquee ticker when text overflows container */
  .media-artist.is-marquee {
    animation: pingpong-marquee var(--marquee-duration, 8s) ease-in-out infinite alternate;
  }
  @keyframes pingpong-marquee {
    0%, 20% {
      transform: translateX(0);
    }
    80%, 100% {
      transform: translateX(var(--marquee-distance, -40px));
    }
  }

  /* Sound Waveform Visualizer (Top Right) */
  .media-waveform {
    display: flex;
    align-items: flex-end;
    gap: 3px;
    height: 22px;
    flex-shrink: 0;
    padding-bottom: 2px;
    opacity: 0.92;
  }
  .wave-bar {
    width: 3.5px;
    border-radius: 3px;
    background: linear-gradient(to top, #c084fc, #f472b6);
    transition: height 0.25s ease;
  }
  .media-waveform.is-playing .wave-bar {
    animation: soundwave-pulse 1.2s ease-in-out infinite alternate;
  }
  .media-waveform.is-playing .bar-1 { animation-delay: 0.0s; animation-duration: 0.9s; height: 14px; }
  .media-waveform.is-playing .bar-2 { animation-delay: 0.2s; animation-duration: 1.3s; height: 22px; }
  .media-waveform.is-playing .bar-3 { animation-delay: 0.4s; animation-duration: 0.8s; height: 12px; }
  .media-waveform.is-playing .bar-4 { animation-delay: 0.15s; animation-duration: 1.1s; height: 18px; }
  .media-waveform.is-playing .bar-5 { animation-delay: 0.35s; animation-duration: 1.0s; height: 15px; }

  .media-waveform.is-paused .wave-bar {
    height: 4px !important;
    opacity: 0.4;
    animation: none !important;
  }

  @keyframes soundwave-pulse {
    0% { height: 4px; }
    50% { height: 20px; }
    100% { height: 8px; }
  }

  /* Progress Row */
  .media-progress-row {
    display: flex;
    align-items: center;
    width: 100%;
    gap: 8px;
    margin: 6px 0 4px 0;
  }
  .media-time {
    font-size: 11px;
    font-variant-numeric: tabular-nums;
    color: var(--fg-muted);
    opacity: 0.75;
    flex-shrink: 0;
    min-width: 32px;
  }
  .media-time-elapsed { text-align: left; }
  .media-time-remaining { text-align: right; }

  .media-progress-track {
    flex: 1;
    height: 6px;
    background: rgba(255, 255, 255, 0.18);
    border-radius: 999px;
    position: relative;
    cursor: pointer;
    transition: height 0.15s ease;
  }
  .media-progress-track:hover {
    height: 8px;
  }
  .media-progress-track::before {
    content: '';
    position: absolute;
    top: -8px;
    bottom: -8px;
    left: 0;
    right: 0;
    cursor: pointer;
  }
  .media-progress-fill {
    height: 100%;
    border-radius: 999px;
    background: #ffffff;
    width: 0%;
    pointer-events: none;
    transition: width 0.08s linear;
  }

  /* Controls Row */
  .media-controls-row {
    display: flex;
    align-items: center;
    justify-content: center;
    position: relative;
    width: 100%;
    margin-top: 2px;
  }
  .media-btns-group {
    display: flex;
    align-items: center;
    gap: 22px;
  }
  .media-btn {
    background: transparent;
    border: none;
    color: var(--fg-primary);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 6px;
    border-radius: 50%;
    transition: transform 0.12s ease, opacity 0.15s ease, background-color 0.15s ease;
  }
  .media-btn:hover {
    opacity: 1;
    transform: scale(1.12);
    background: rgba(255, 255, 255, 0.08);
  }
  .media-btn:active {
    transform: scale(0.94);
  }
  .media-btn-playpause {
    width: 38px;
    height: 38px;
    padding: 0;
  }
  .media-btn-playpause .icon-play {
    display: none;
  }
  .media-btn-playpause.is-paused .icon-play {
    display: block;
  }
  .media-btn-playpause.is-paused .icon-pause {
    display: none;
  }
  .media-btn:disabled,
  .media-btn.is-disabled {
    opacity: 0.28 !important;
    cursor: default !important;
    pointer-events: none !important;
    transform: none !important;
  }
  .media-progress-track.is-disabled {
    opacity: 0.28 !important;
    cursor: default !important;
    pointer-events: none !important;
  }
  .media-container.no-media .media-art-placeholder {
    opacity: 0.45;
  }
  .media-container.no-media .media-waveform .wave-bar {
    opacity: 0.2 !important;
    height: 4px !important;
    animation: none !important;
  }

  /* Edit Mode Toggle Icon */
  .edit-toggle-btn {
    position: fixed;
    bottom: 24px;
    right: 28px;
    width: 44px;
    height: 44px;
    border-radius: 12px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    color: var(--fg-muted);
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    opacity: 0.35;
    transition: opacity 0.2s, color 0.2s, border-color 0.2s, transform 0.2s, box-shadow 0.2s;
    z-index: 50;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
  }
  .edit-toggle-btn:hover {
    opacity: 0.9;
    color: var(--accent);
    border-color: var(--accent);
    transform: scale(1.05);
  }
  body.edit-mode .edit-toggle-btn {
    opacity: 0;
    pointer-events: none;
    transform: scale(0.85) translateX(30px);
  }

  /* Widget Tray (360px Vertical Side Drawer on the Right) */
  #widget-tray {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: 360px;
    background: var(--bg-surface);
    border-left: 1px solid var(--border);
    box-shadow: -10px 0 36px rgba(0, 0, 0, 0.75);
    transform: translateX(100%);
    transition: transform 320ms cubic-bezier(0.16, 1, 0.3, 1);
    z-index: 60;
    display: flex;
    flex-direction: column;
    padding: 24px 20px;
  }
  body.edit-mode #widget-tray {
    transform: translateX(0);
  }
  .tray-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding-bottom: 16px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 20px;
  }
  .tray-header h2 {
    font-size: 16px;
    font-weight: 700;
    letter-spacing: -0.02em;
    color: var(--fg-primary);
  }
  .tray-close-btn {
    background: transparent;
    border: none;
    color: var(--fg-muted);
    font-size: 18px;
    cursor: pointer;
  }
  .tray-close-btn:hover {
    color: var(--fg-primary);
  }
  .tray-catalog {
    flex: 1;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 14px;
  }
  .tray-item {
    background: var(--bg-subtle);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 14px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    cursor: grab;
    transition: border-color 0.15s, transform 0.15s, background-color 0.15s;
  }
  .tray-item:hover {
    border-color: var(--accent);
    transform: translateY(-2px);
  }
  .tray-item-info h3 {
    font-size: 13px;
    font-weight: 600;
    color: var(--fg-primary);
  }
  .tray-item-info p {
    font-size: 11px;
    color: var(--fg-muted);
    margin-top: 2px;
  }
  .tray-add-btn {
    background: var(--accent-tint);
    border: 1px solid var(--accent);
    color: var(--accent);
    padding: 6px 12px;
    border-radius: 8px;
    font-size: 11px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s;
  }
  .tray-add-btn:hover {
    background: var(--accent);
    color: #ffffff;
  }

  /* ──────────────── Contextual Configuration Modal (1:1 Native Browser Settings Clone) ──────────────── */
  .modal-backdrop {
    display: flex;
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.52);
    backdrop-filter: blur(8px);
    z-index: 100;
    align-items: center;
    justify-content: center;
    opacity: 0;
    visibility: hidden;
    pointer-events: none;
    transition: opacity 260ms cubic-bezier(0.16, 1, 0.3, 1),
                visibility 260ms cubic-bezier(0.16, 1, 0.3, 1);
  }
  .modal-backdrop.open {
    opacity: 1;
    visibility: visible;
    pointer-events: auto;
  }
  .modal-dialog {
    width: 680px;
    height: 480px;
    max-width: 90vw;
    max-height: 85vh;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    box-shadow: 0 24px 64px rgba(0, 0, 0, 0.7);
    overflow: hidden;
    display: flex;
    flex-direction: column;
    position: relative;
    user-select: none;
    transform: scale(0.88);
    opacity: 0;
    transition: transform 260ms cubic-bezier(0.16, 1, 0.3, 1),
                opacity 260ms cubic-bezier(0.16, 1, 0.3, 1);
    transform-origin: center center;
  }
  .modal-backdrop.open .modal-dialog {
    transform: scale(1.0);
    opacity: 1;
  }
  .modal-header {
    height: 44px;
    background: var(--bg-subtle);
    border-bottom: 1px solid var(--border);
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0 16px;
    flex-shrink: 0;
  }
  .modal-title-text {
    font-size: 12px;
    font-weight: 700;
    color: var(--fg-primary);
    letter-spacing: 0.02em;
  }
  .modal-close-btn {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: transparent;
    border: none;
    color: var(--fg-muted);
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 18px;
    line-height: 1;
    cursor: pointer;
    transition: all 0.15s;
  }
  .modal-close-btn:hover {
    background: var(--bg-active);
    color: var(--fg-primary);
  }
  .modal-body-container {
    flex: 1;
    display: flex;
    flex-direction: row;
    min-height: 0;
    overflow: hidden;
  }
  .modal-sidebar {
    width: 140px;
    background: var(--bg-surface);
    border-right: 1px solid var(--border);
    padding: 8px 0;
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
  }
  .modal-sidebar-tab {
    height: 36px;
    margin: 3px 8px;
    border-radius: 6px;
    padding: 0 14px;
    display: flex;
    align-items: center;
    position: relative;
    background: transparent;
    border: 1px solid transparent;
    color: var(--fg-muted);
    font-size: 11px;
    font-weight: 600;
    cursor: pointer;
    font-family: inherit;
    transition: all 0.15s;
    text-align: left;
  }
  .modal-sidebar-tab:hover:not(.active) {
    background: var(--bg-subtle);
    color: var(--fg-primary);
  }
  .modal-sidebar-tab.active {
    background: var(--bg-active);
    border-color: var(--accent);
    color: var(--fg-primary);
    font-weight: 700;
  }
  .modal-sidebar-tab .tab-accent-indicator {
    display: none;
    position: absolute;
    left: 0;
    top: 6px;
    bottom: 6px;
    width: 3px;
    background: var(--accent);
    border-radius: 1.5px;
  }
  .modal-sidebar-tab.active .tab-accent-indicator {
    display: block;
  }
  .modal-content-col {
    flex: 1;
    display: flex;
    flex-direction: column;
    min-width: 0;
    overflow: hidden;
  }
  .modal-content-scroll {
    flex: 1;
    overflow-y: auto;
    padding: 20px 24px;
  }
  .tab-pane {
    display: none;
    flex-direction: column;
    gap: 16px;
  }
  .tab-pane.active {
    display: flex;
  }
  .form-group {
    display: flex;
    flex-direction: column;
    gap: 6px;
    position: relative;
  }
  .form-group label {
    font-size: 11px;
    font-weight: 600;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }
  .form-control {
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 8px 14px;
    font-size: 12px;
    color: var(--fg-primary);
    outline: none;
    font-family: inherit;
    transition: border-color 0.15s, box-shadow 0.15s;
  }
  .form-control:focus {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  textarea.form-control {
    font-family: 'JetBrains Mono', 'Fira Code', Consolas, monospace;
    font-size: 12px;
    resize: vertical;
    min-height: 150px;
    line-height: 1.5;
    background: var(--bg-base);
  }

  /* ──────────────── Standardized Custom Dropdown Component ──────────────── */
  .custom-dropdown {
    position: relative;
    width: 100%;
  }
  .custom-dropdown-trigger {
    display: flex;
    align-items: center;
    justify-content: space-between;
    width: 100%;
    height: 36px;
    padding: 0 14px;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    color: var(--fg-primary);
    font-size: 11.5px;
    font-weight: 600;
    cursor: pointer;
    font-family: inherit;
    transition: background 0.15s, border-color 0.15s, box-shadow 0.15s;
  }
  .custom-dropdown-trigger:hover {
    background: var(--bg-subtle);
    border-color: var(--border-hover);
  }
  .custom-dropdown.open .custom-dropdown-trigger {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  .custom-dropdown-label {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .custom-dropdown-chevron {
    width: 14px;
    height: 14px;
    color: var(--fg-muted);
    transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1), color 0.2s ease;
    flex-shrink: 0;
    margin-left: 8px;
  }
  .custom-dropdown.open .custom-dropdown-chevron {
    transform: rotate(180deg);
    color: var(--accent);
  }
  .custom-dropdown-menu {
    position: absolute;
    top: calc(100% + 6px);
    left: 0;
    right: 0;
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    box-shadow: 0 12px 28px rgba(0, 0, 0, 0.55);
    padding: 4px;
    display: none;
    z-index: 150;
    max-height: 200px;
    overflow-y: auto;
    backdrop-filter: blur(8px);
  }
  .custom-dropdown.open .custom-dropdown-menu {
    display: flex;
    flex-direction: column;
    gap: 2px;
    animation: dropdownSlideDown 0.15s cubic-bezier(0.16, 1, 0.3, 1);
  }
  @keyframes dropdownSlideDown {
    from { opacity: 0; transform: translateY(-4px); }
    to   { opacity: 1; transform: translateY(0); }
  }
  .custom-dropdown-item {
    display: flex;
    align-items: center;
    gap: 8px;
    height: 32px;
    padding: 0 10px;
    border-radius: 5px;
    font-size: 11px;
    color: var(--fg-muted);
    cursor: pointer;
    transition: all 0.12s;
  }
  .custom-dropdown-item:hover {
    background: var(--bg-subtle);
    color: var(--fg-primary);
  }
  .custom-dropdown-item.active {
    background: var(--bg-active);
    color: var(--fg-primary);
    font-weight: 700;
  }
  .dropdown-item-dot {
    width: 5px;
    height: 5px;
    border-radius: 50%;
    background: transparent;
    flex-shrink: 0;
  }
  .custom-dropdown-item.active .dropdown-item-dot {
    background: var(--accent);
  }
  .dropdown-item-text {
    flex: 1;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .dropdown-item-hint {
    font-size: 10px;
    color: var(--fg-dim);
    margin-left: auto;
  }

  /* Slider Styling */
  .form-slider-container {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .form-slider {
    flex: 1;
    accent-color: var(--accent);
    cursor: pointer;
  }
  .form-slider-val {
    font-size: 11px;
    font-weight: 700;
    color: var(--accent);
    min-width: 32px;
    text-align: right;
  }

  /* Modal Footer Action Bar */
  .modal-action-bar {
    height: 48px;
    background: var(--bg-subtle);
    border-top: 1px solid var(--border);
    padding: 0 20px;
    display: flex;
    justify-content: flex-end;
    align-items: center;
    gap: 10px;
    flex-shrink: 0;
  }
  .btn-settings {
    padding: 7px 18px;
    border-radius: 6px;
    font-size: 11.5px;
    font-family: inherit;
    cursor: pointer;
    transition: all 0.15s;
  }
  .btn-cancel {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--fg-muted);
    font-weight: 600;
  }
  .btn-cancel:hover {
    background: var(--bg-surface);
    color: var(--fg-primary);
    border-color: var(--border-hover);
  }
  .btn-save {
    background: var(--accent);
    border: 1px solid var(--accent);
    color: #ffffff;
    font-weight: 700;
    padding: 7px 22px;
  }
  .btn-save:hover {
    box-shadow: 0 0 14px var(--accent-glow);
    filter: brightness(1.08);
  }
</style>
</head>
<body>
  <div class="canvas-backdrop"></div>
  <div class="peripheral-frame"></div>

  <!-- Desktop Viewport Wrapper (Mirrored KDE Plasma Style) -->
  <div id="desktop-wrapper">
    <!-- KDE Plasma Top Edit Header Bar -->
    <div id="desktop-edit-bar">
      <div class="edit-bar-left">
        <span class="edit-bar-title">Widgets</span>
        <span class="edit-bar-subtitle">Drag widgets to arrange or use the gear icon to configure</span>
      </div>
      <div class="edit-bar-right">
        <button type="button" class="edit-bar-exit-btn" onclick="toggleEditMode()" title="Exit Edit Mode">
          <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
          <span>Exit Edit Mode</span>
        </button>
      </div>
    </div>

    <!-- Main Viewport Canvas -->
    <main id="canvas">
      <!-- Row 1: Top Drop Zones -->
      <div class="drop-zone" id="zone-top-left" data-zone="zone-top-left"></div>
      <div class="drop-zone" id="zone-top-center" data-zone="zone-top-center"></div>
      <div class="drop-zone" id="zone-top-right" data-zone="zone-top-right"></div>

      <!-- Row 2: Center Search Anchor -->
      <div class="search-container">
        <form class="search-anchor" id="search-form" autocomplete="off" onsubmit="return handleSearchSubmit(event);">
          <svg class="search-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="11" cy="11" r="8"></circle>
            <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
          </svg>
          <input type="text" id="search-input" class="search-input"
                 placeholder="Search the web or enter URL..."
                 spellcheck="false" autocomplete="off" autofocus />
        </form>
        <!-- History Dropdown (Last 5 Local Queries) -->
        <div class="search-history-dropdown" id="search-history">
          <div class="history-header">
            <span>Recent Searches</span>
            <button type="button" class="clear-history-btn" onclick="clearSearchHistory()">Clear</button>
          </div>
          <div id="history-items-container"></div>
        </div>
      </div>

      <!-- Row 3: Bottom Drop Zones -->
      <div class="drop-zone" id="zone-bottom-left" data-zone="zone-bottom-left"></div>
      <div class="drop-zone" id="zone-bottom-center" data-zone="zone-bottom-center"></div>
      <div class="drop-zone" id="zone-bottom-right" data-zone="zone-bottom-right"></div>
    </main>
  </div>

  <!-- Edit Mode Toggle Button -->
  <button class="edit-toggle-btn" id="edit-toggle-btn" title="Toggle Edit Mode" onclick="toggleEditMode()">
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M12 20h9"></path>
      <path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"></path>
    </svg>
  </button>

  <!-- Widget Catalog Side Drawer -->
  <aside id="widget-tray">
    <div class="tray-header">
      <h2>Widgets Catalog</h2>
      <button class="tray-close-btn" onclick="toggleEditMode()" title="Close Tray">&times;</button>
    </div>
    <div class="tray-catalog">
      <div class="tray-item" draggable="true" ondragstart="handleTrayDragStart(event, 'l.clock')" onmousedown="initTrayItemDrag(event, 'l.clock')">
        <div class="tray-item-info">
          <h3>Sleek Clock</h3>
          <p>Minimalist Clock digital tabular display</p>
        </div>
        <button class="tray-add-btn" onclick="addWidget('l.clock', 'zone-top-left')">+ Add</button>
      </div>

      <div class="tray-item" draggable="true" ondragstart="handleTrayDragStart(event, 'l.weather')" onmousedown="initTrayItemDrag(event, 'l.weather')">
        <div class="tray-item-info">
          <h3>Live Weather Monitor</h3>
          <p>Keyless Open-Meteo REST radar</p>
        </div>
        <button class="tray-add-btn" onclick="addWidget('l.weather', 'zone-top-right')">+ Add</button>
      </div>

      <div class="tray-item" draggable="true" ondragstart="handleTrayDragStart(event, 'l.embed')" onmousedown="initTrayItemDrag(event, 'l.embed')">
        <div class="tray-item-info">
          <h3>Custom HTML Embed</h3>
          <p>Sandboxed iframe payload</p>
        </div>
        <button class="tray-add-btn" onclick="addWidget('l.embed', 'zone-bottom-center')">+ Add</button>
      </div>

      <div class="tray-item" draggable="true" ondragstart="handleTrayDragStart(event, 'l.notes')" onmousedown="initTrayItemDrag(event, 'l.notes')">
        <div class="tray-item-info">
          <h3>Notes Window</h3>
          <p>macOS styled memo with themes &amp; Markdown</p>
        </div>
        <button class="tray-add-btn" onclick="addWidget('l.notes', 'zone-bottom-left')">+ Add</button>
      </div>

      <div class="tray-item" draggable="true" ondragstart="handleTrayDragStart(event, 'l.media')" onmousedown="initTrayItemDrag(event, 'l.media')">
        <div class="tray-item-info">
          <h3>Media Player</h3>
          <p>System audio playback with waveforms &amp; seek bar</p>
        </div>
        <button class="tray-add-btn" onclick="addWidget('l.media', 'zone-bottom-center')">+ Add</button>
      </div>
    </div>
  </aside>

  <!-- Contextual Settings Modal (1:1 Native Browser Settings Dialog) -->
  <div class="modal-backdrop" id="settings-modal" onclick="if(event.target===this)closeSettingsModal()">
    <div class="modal-dialog">
      <!-- 44px Titlebar -->
      <div class="modal-header">
        <span class="modal-title-text" id="modal-title">Settings</span>
        <button type="button" class="modal-close-btn" onclick="closeSettingsModal()" title="Close">&times;</button>
      </div>
      <!-- 2-Column Body: Sidebar (140px) + Content -->
      <div class="modal-body-container">
        <!-- Sidebar -->
        <div class="modal-sidebar" id="modal-sidebar"></div>
        <!-- Content Column -->
        <div class="modal-content-col">
          <div class="modal-content-scroll" id="modal-panes-container"></div>
          <!-- Action Bar -->
          <div class="modal-action-bar">
            <button type="button" class="btn-settings btn-cancel" onclick="closeSettingsModal()">Cancel</button>
            <button type="button" class="btn-settings btn-save" onclick="saveSettingsModal()">Save Changes</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    // ──────────────── State & Storage ────────────────
    const STORAGE_KEY = 'lumen_newtab_state_v1';
    const SEARCH_HIST_KEY = 'lumen_search_history';

    let appState = {
      editMode: false,
      activeModalWidgetId: null,
      widgets: []
    };

    const DEFAULT_WIDGETS = [
      {
        id: 'clock_1',
        type: 'l.clock',
        zone: 'zone-top-left',
        x: 16,
        y: 16,
        width: '220px',
        height: '110px',
        config: { timeFormat: '24h', showSeconds: true, dateFormat: 'short', fontFamily: 'mono', fontSize: 'standard' }
      },
      {
        id: 'weather_1',
        type: 'l.weather',
        zone: 'zone-top-right',
        x: 16,
        y: 16,
        width: '260px',
        height: '110px',
        config: { city: 'Moscow', lat: 55.7558, lon: 37.6173, viewMode: 'badge', showIcons: true, units: 'metric', windUnits: 'kmh' }
      },
      {
        id: 'embed_1',
        type: 'l.embed',
        zone: 'zone-bottom-center',
        x: 16,
        y: 16,
        width: '360px',
        height: '130px',
        config: {
          html: '<div style="display:flex;flex-direction:column;align-items:center;justify-content:center;height:100%;font-family:sans-serif;color:var(--accent,#38bdf8);text-align:center;"><div style="font-weight:700;font-size:16px;">Lumen Modular Surface</div><div style="font-size:12px;color:var(--fg-muted,#94a3b8);margin-top:4px;">Drag, drop and configure widgets</div></div>',
          transparent: true,
          padding: 8,
          border: 'subtle',
          allowScripts: true
        }
      }
    ];

    function loadState() {
      try {
        const raw = localStorage.getItem(STORAGE_KEY);
        if (raw) {
          const parsed = JSON.parse(raw);
          appState.widgets = (parsed.widgets && parsed.widgets.length) ? parsed.widgets : DEFAULT_WIDGETS;
        } else {
          appState.widgets = DEFAULT_WIDGETS;
        }
      } catch (e) {
        appState.widgets = DEFAULT_WIDGETS;
      }

      // Ensure every widget has defined x and y pixel coordinates within its zone
      const zoneOcc = {};
      appState.widgets.forEach(w => {
        if (!zoneOcc[w.zone]) zoneOcc[w.zone] = 0;
        if (typeof w.x !== 'number') w.x = 16;
        if (typeof w.y !== 'number') {
          w.y = zoneOcc[w.zone] === 0 ? 16 : 140;
        }
        zoneOcc[w.zone]++;
      });
    }

    function saveState() {
      try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify({ widgets: appState.widgets }));
      } catch (e) {}
    }

    // ──────────────── Dynamic Theme Reactivity Bridge ────────────────
    window.__setLumenTheme = function(theme) {
      if (!theme) return;
      const root = document.documentElement;
      if (theme.bgBase) root.style.setProperty('--bg-base', theme.bgBase);
      if (theme.bgSurface) root.style.setProperty('--bg-surface', theme.bgSurface);
      if (theme.bgSubtle) root.style.setProperty('--bg-subtle', theme.bgSubtle);
      if (theme.bgActive) root.style.setProperty('--bg-active', theme.bgActive);
      if (theme.border) root.style.setProperty('--border', theme.border);
      if (theme.borderHover) root.style.setProperty('--border-hover', theme.borderHover);
      if (theme.fgPrimary) root.style.setProperty('--fg-primary', theme.fgPrimary);
      if (theme.fgMuted) root.style.setProperty('--fg-muted', theme.fgMuted);
      if (theme.fgDim) root.style.setProperty('--fg-dim', theme.fgDim);
      if (theme.accent) {
        root.style.setProperty('--accent', theme.accent);
        root.style.setProperty('--accent-glow', theme.accentGlow || (theme.accent + '40'));
        root.style.setProperty('--accent-tint', theme.accentTint || (theme.accent + '14'));
      }
      if (theme.danger) root.style.setProperty('--danger', theme.danger);
    };

    // ──────────────── Search History Engine ────────────────
    function loadSearchHistory() {
      try {
        const r = localStorage.getItem(SEARCH_HIST_KEY);
        return r ? JSON.parse(r) : [];
      } catch (e) { return []; }
    }

    function addSearchHistory(query) {
      if (!query || !query.trim()) return;
      query = query.trim();
      let hist = loadSearchHistory().filter(q => q.toLowerCase() !== query.toLowerCase());
      hist.unshift(query);
      if (hist.length > 5) hist = hist.slice(0, 5);
      try { localStorage.setItem(SEARCH_HIST_KEY, JSON.stringify(hist)); } catch (e) {}
    }

    function clearSearchHistory() {
      localStorage.removeItem(SEARCH_HIST_KEY);
      renderSearchHistory();
    }

    function renderSearchHistory() {
      const container = document.getElementById('history-items-container');
      const dropdown = document.getElementById('search-history');
      const hist = loadSearchHistory();
      if (hist.length === 0) {
        dropdown.classList.remove('open');
        container.innerHTML = '';
        return;
      }
      container.innerHTML = hist.map(item => `
        <div class="history-item" onmousedown="executeSearch('${escapeAttr(item)}')">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <polyline points="12 6 12 12 16 14"></polyline>
          </svg>
          <span>${escapeHtml(item)}</span>
        </div>
      `).join('');
    }

    function handleSearchSubmit(e) {
      e.preventDefault();
      const val = document.getElementById('search-input').value.trim();
      if (!val) return false;
      addSearchHistory(val);
      executeSearch(val);
      return false;
    }

    function executeSearch(val) {
      if (!val) return;
      document.getElementById('search-input').value = val;
      if (val.includes('.') && !val.includes(' ')) {
        window.location.href = val.startsWith('http') ? val : 'https://' + val;
      } else {
        window.location.href = 'https://duckduckgo.com/?q=' + encodeURIComponent(val);
      }
    }

    // ──────────────── Custom Dropdown Component ────────────────
    function renderCustomDropdownHtml(id, label, value, options) {
      const activeOpt = options.find(o => String(o.val) === String(value)) || options[0] || { val: value, label: value };
      const itemsHtml = options.map(opt => {
        const isActive = String(opt.val) === String(activeOpt.val);
        const hintHtml = opt.hint ? `<span class="dropdown-item-hint">${escapeHtml(opt.hint)}</span>` : '';
        return `
          <div class="custom-dropdown-item ${isActive ? 'active' : ''}" data-val="${escapeAttr(String(opt.val))}" onclick="selectCustomDropdownItem('${id}', '${escapeAttr(String(opt.val))}')">
            <span class="dropdown-item-dot"></span>
            <span class="dropdown-item-text">${escapeHtml(opt.label)}</span>
            ${hintHtml}
          </div>
        `;
      }).join('');

      return `
        <div class="custom-dropdown" id="dropdown-${id}" data-id="${id}" data-value="${escapeAttr(String(activeOpt.val))}">
          <button type="button" class="custom-dropdown-trigger" onclick="toggleCustomDropdown('${id}')">
            <span class="custom-dropdown-label" id="dropdown-lbl-${id}">${escapeHtml(activeOpt.label)}</span>
            <svg class="custom-dropdown-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M6 9l6 6 6-6"></path>
            </svg>
          </button>
          <div class="custom-dropdown-menu">
            ${itemsHtml}
          </div>
        </div>
      `;
    }

    function toggleCustomDropdown(id) {
      const el = document.getElementById(`dropdown-${id}`);
      if (!el) return;
      const isOpen = el.classList.contains('open');
      closeAllCustomDropdowns();
      if (!isOpen) el.classList.add('open');
    }

    function closeAllCustomDropdowns() {
      document.querySelectorAll('.custom-dropdown.open').forEach(d => d.classList.remove('open'));
    }

    function selectCustomDropdownItem(id, val) {
      const el = document.getElementById(`dropdown-${id}`);
      if (!el) return;
      el.dataset.value = val;
      const items = el.querySelectorAll('.custom-dropdown-item');
      let foundText = val;
      items.forEach(it => {
        const isMatch = it.dataset.val === String(val);
        it.classList.toggle('active', isMatch);
        if (isMatch) {
          const t = it.querySelector('.dropdown-item-text');
          if (t) foundText = t.textContent;
        }
      });
      const lbl = document.getElementById(`dropdown-lbl-${id}`);
      if (lbl) lbl.textContent = foundText;
      el.classList.remove('open');

      if (typeof window[`onDropdownChanged_${id}`] === 'function') {
        window[`onDropdownChanged_${id}`](val);
      }
    }

    function getCustomDropdownValue(id) {
      const el = document.getElementById(`dropdown-${id}`);
      return el ? el.dataset.value : null;
    }

    // ──────────────── Edit Mode & Tray ────────────────
    function toggleEditMode() {
      appState.editMode = !appState.editMode;
      document.body.classList.toggle('edit-mode', appState.editMode);
      if (!appState.editMode) {
        document.querySelectorAll('.widget.transform-mode').forEach(w => w.classList.remove('transform-mode'));
      }
      renderWidgets();
    }

    let activeTrayType = null;
    function handleTrayDragStart(e, type) {
      activeTrayType = type;
      e.dataTransfer.setData('widget_type', type);
    }

    function getDefaultWidgetConfig(type) {
      if (type === 'l.clock') {
        return { timeFormat: '24h', showSeconds: true, dateFormat: 'short', fontFamily: 'mono', fontSize: 'standard' };
      } else if (type === 'l.weather') {
        return { city: 'Moscow', lat: 55.7558, lon: 37.6173, viewMode: 'badge', showIcons: true, units: 'metric', windUnits: 'kmh' };
      } else if (type === 'l.embed') {
        return {
          html: '<div style="color:var(--accent,#38bdf8);font-family:sans-serif;padding:12px;text-align:center;">Custom HTML Widget</div>',
          transparent: true,
          padding: 8,
          border: 'subtle',
          allowScripts: true
        };
      } else if (type === 'l.notes') {
        return {
          theme: 'macos',
          title: 'Notes',
          markdown: true,
          fontSize: '13px',
          wrap: 'wrap',
          text: '# Quick Notes\n- Add your reminders here\n- Supports **bold** and `code`'
        };
      } else if (type === 'l.media') {
        return {
          waveform: true,
          marquee: true,
          showArt: true
        };
      }
      return {};
    }

    function initTrayItemDrag(startEvent, type) {
      if (startEvent.button !== 0 || startEvent.target.closest('.tray-add-btn')) return;

      const startX = startEvent.clientX;
      const startY = startEvent.clientY;
      let dragStarted = false;
      let ghostEl = null;
      let activeHoverZone = null;

      const defW = type === 'l.media' ? 336 : (type === 'l.embed' ? 360 : (type === 'l.notes' ? 280 : (type === 'l.weather' ? 260 : 220)));
      const defH = type === 'l.media' ? 144 : (type === 'l.embed' ? 140 : (type === 'l.notes' ? 180 : 110));
      const grabOffsetX = Math.round(defW / 2);
      const grabOffsetY = 36;

      function onMouseMove(moveEvent) {
        if (!dragStarted) {
          if (Math.hypot(moveEvent.clientX - startX, moveEvent.clientY - startY) < 5) return;
          dragStarted = true;

          document.body.classList.add('is-widget-dragging');

          ghostEl = document.createElement('div');
          ghostEl.className = `widget widget-${type.replace('l.', '')} is-dragging widget-lifted`;
          ghostEl.style.position = 'fixed';
          ghostEl.style.width = `${defW}px`;
          ghostEl.style.height = `${defH}px`;
          ghostEl.style.zIndex = '999999';

          const dummyW = {
            id: 'tray_preview',
            type: type,
            zone: '',
            x: 0,
            y: 0,
            width: `${defW}px`,
            height: `${defH}px`,
            config: getDefaultWidgetConfig(type)
          };

          if (type === 'l.clock') {
            ghostEl.innerHTML = renderClockHtml(dummyW);
          } else if (type === 'l.weather') {
            ghostEl.innerHTML = renderWeatherHtml(dummyW);
          } else if (type === 'l.embed') {
            ghostEl.innerHTML = renderEmbedHtml(dummyW);
          } else if (type === 'l.notes') {
            ghostEl.innerHTML = renderNotesHtml(dummyW);
          } else if (type === 'l.media') {
            ghostEl.innerHTML = renderMediaHtml(dummyW);
          }

          document.body.appendChild(ghostEl);
        }

        if (ghostEl) {
          ghostEl.style.left = `${moveEvent.clientX - grabOffsetX}px`;
          ghostEl.style.top = `${moveEvent.clientY - grabOffsetY}px`;

          const hoveredZone = getDropZoneAtPoint(moveEvent.clientX, moveEvent.clientY);
          if (hoveredZone) {
            activeHoverZone = hoveredZone;
            ghostEl.classList.remove('widget-lifted');

            const hzRect = hoveredZone.getBoundingClientRect();
            const maxGridX = Math.max(GRID_OFFSET, Math.floor((hoveredZone.clientWidth - defW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            const maxGridY = Math.max(GRID_OFFSET, Math.floor((hoveredZone.clientHeight - defH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            let landX = snapToGrid(moveEvent.clientX - grabOffsetX - hzRect.left);
            let landY = snapToGrid(moveEvent.clientY - grabOffsetY - hzRect.top);
            landX = Math.max(GRID_OFFSET, Math.min(landX, maxGridX));
            landY = Math.max(GRID_OFFSET, Math.min(landY, maxGridY));

            const collides = getZoneOverlaps(hoveredZone.id, landX, landY, defW, defH);
            const isFull = getZoneWidgetCount(hoveredZone.id) >= MAX_ZONE_WIDGETS;

            document.querySelectorAll('.drop-zone').forEach(z => {
              if (z !== hoveredZone) z.classList.remove('drag-over', 'drag-error');
            });

            if (isFull || collides) {
              hoveredZone.classList.remove('drag-over');
              hoveredZone.classList.add('drag-error');
              showDropPreview(hoveredZone, landX, landY, defW, defH, true);
            } else {
              hoveredZone.classList.remove('drag-error');
              hoveredZone.classList.add('drag-over');
              showDropPreview(hoveredZone, landX, landY, defW, defH, false);
            }
          } else {
            activeHoverZone = null;
            ghostEl.classList.add('widget-lifted');
            removeDropPreview();
            document.querySelectorAll('.drop-zone').forEach(z => z.classList.remove('drag-over', 'drag-error'));
          }
        }
      }

      function onMouseUp(upEvent) {
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);

        if (dragStarted) {
          document.body.classList.remove('is-widget-dragging');
          if (ghostEl && ghostEl.parentElement) ghostEl.remove();
          removeDropPreview();
          document.querySelectorAll('.drop-zone').forEach(z => z.classList.remove('drag-over', 'drag-error'));

          if (activeHoverZone) {
            const hzRect = activeHoverZone.getBoundingClientRect();
            let rawX = snapToGrid(upEvent.clientX - grabOffsetX - hzRect.left);
            let rawY = snapToGrid(upEvent.clientY - grabOffsetY - hzRect.top);
            const maxGridX = Math.max(GRID_OFFSET, Math.floor((activeHoverZone.clientWidth - defW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            const maxGridY = Math.max(GRID_OFFSET, Math.floor((activeHoverZone.clientHeight - defH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            rawX = Math.max(GRID_OFFSET, Math.min(rawX, maxGridX));
            rawY = Math.max(GRID_OFFSET, Math.min(rawY, maxGridY));

            if (getZoneWidgetCount(activeHoverZone.id) < MAX_ZONE_WIDGETS) {
              const freePos = findFreePositionInZone(activeHoverZone, rawX, rawY, defW, defH);
              if (freePos) {
                addWidget(type, activeHoverZone.id, freePos.x, freePos.y);
              } else {
                flashZoneFull(activeHoverZone);
              }
            } else {
              flashZoneFull(activeHoverZone);
            }
          }
        }
      }

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    }

    const ALL_ZONES = [
      'zone-top-left', 'zone-top-center', 'zone-top-right',
      'zone-bottom-left', 'zone-bottom-center', 'zone-bottom-right'
    ];

    const MAX_ZONE_WIDGETS = 5;
    const GRID_STEP = 16;
    const GRID_OFFSET = 8;

    function snapToGrid(val) {
      return Math.round((val - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET;
    }

    function snapDimension(val) {
      return Math.max(80, Math.round(val / GRID_STEP) * GRID_STEP);
    }

    function doBoxesOverlap(b1, b2, margin = 4) {
      return !(
        b1.x + b1.w <= b2.x + margin ||
        b2.x + b2.w <= b1.x + margin ||
        b1.y + b1.h <= b2.y + margin ||
        b2.y + b2.h <= b1.y + margin
      );
    }

    function getZoneOverlaps(zoneId, x, y, w, h, excludeWidgetId = null) {
      const box = { x, y, w, h };
      const existing = appState.widgets.filter(ow => ow.zone === zoneId && ow.id !== excludeWidgetId);
      for (const other of existing) {
        const otherEl = document.getElementById(other.id);
        const ow = otherEl ? otherEl.offsetWidth : (parseInt(other.width, 10) || 220);
        const oh = otherEl ? otherEl.offsetHeight : (parseInt(other.height, 10) || 110);
        const ox = typeof other.x === 'number' ? other.x : 8;
        const oy = typeof other.y === 'number' ? other.y : 8;
        if (doBoxesOverlap(box, { x: ox, y: oy, w: ow, h: oh }, 4)) {
          return other;
        }
      }
      return null;
    }

    function findFreePositionInZone(zoneEl, prefX, prefY, w, h, excludeId = null) {
      if (!zoneEl) return null;
      const zoneW = zoneEl.clientWidth;
      const zoneH = zoneEl.clientHeight;
      const maxGridX = Math.max(GRID_OFFSET, Math.floor((zoneW - w - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
      const maxGridY = Math.max(GRID_OFFSET, Math.floor((zoneH - h - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
      if (maxGridX < GRID_OFFSET || maxGridY < GRID_OFFSET) return null;

      const clampedPrefX = Math.max(GRID_OFFSET, Math.min(snapToGrid(prefX), maxGridX));
      const clampedPrefY = Math.max(GRID_OFFSET, Math.min(snapToGrid(prefY), maxGridY));

      if (!getZoneOverlaps(zoneEl.id, clampedPrefX, clampedPrefY, w, h, excludeId)) {
        return { x: clampedPrefX, y: clampedPrefY };
      }

      // Search grid outward in 16px steps from preferred position
      let bestPos = null;
      let minDistance = Infinity;

      for (let y = GRID_OFFSET; y <= maxGridY; y += GRID_STEP) {
        for (let x = GRID_OFFSET; x <= maxGridX; x += GRID_STEP) {
          if (!getZoneOverlaps(zoneEl.id, x, y, w, h, excludeId)) {
            const dist = Math.hypot(x - clampedPrefX, y - clampedPrefY);
            if (dist < minDistance) {
              minDistance = dist;
              bestPos = { x, y };
            }
          }
        }
      }
      return bestPos;
    }

    function getDropZoneAtPoint(clientX, clientY) {
      for (const zoneId of ALL_ZONES) {
        const z = document.getElementById(zoneId);
        if (!z) continue;
        const r = z.getBoundingClientRect();
        if (clientX >= r.left && clientX <= r.right && clientY >= r.top && clientY <= r.bottom) {
          return z;
        }
      }
      return null;
    }

    function getZoneWidgetCount(zoneId, excludeWidgetId = null) {
      return appState.widgets.filter(w => w.zone === zoneId && w.id !== excludeWidgetId).length;
    }

    function flashZoneFull(zoneEl) {
      if (!zoneEl) return;
      zoneEl.classList.remove('drag-over');
      zoneEl.classList.add('zone-full');
      setTimeout(() => {
        zoneEl.classList.remove('zone-full', 'drag-error');
      }, 650);
    }

    let activePreviewEl = null;

    function showDropPreview(zoneEl, x, y, width, height, isInvalid = false) {
      if (!zoneEl) return;
      if (!activePreviewEl) {
        activePreviewEl = document.createElement('div');
        activePreviewEl.className = 'widget-drop-preview';
      }
      if (activePreviewEl.parentElement !== zoneEl) {
        zoneEl.appendChild(activePreviewEl);
      }
      activePreviewEl.classList.toggle('preview-invalid', isInvalid);
      activePreviewEl.style.left = `${x}px`;
      activePreviewEl.style.top = `${y}px`;
      activePreviewEl.style.width = `${width}px`;
      activePreviewEl.style.height = `${height}px`;
      activePreviewEl.style.display = 'block';
    }

    function removeDropPreview() {
      if (activePreviewEl) {
        if (activePreviewEl.parentElement) {
          activePreviewEl.parentElement.removeChild(activePreviewEl);
        }
        activePreviewEl = null;
      }
      document.querySelectorAll('.widget-drop-preview').forEach(p => p.remove());
    }

    function centerWidgetHorizontally(widgetId) {
      const w = appState.widgets.find(x => x.id === widgetId);
      if (!w) return;
      const zone = document.getElementById(w.zone);
      const el = document.getElementById(widgetId);
      if (!zone || !el) return;

      const zoneW = zone.clientWidth;
      const widgetW = el.offsetWidth;
      let newX = Math.round((zoneW - widgetW) / 2);
      newX = snapToGrid(newX);
      newX = Math.max(8, Math.min(newX, zoneW - widgetW - 8));

      const collides = getZoneOverlaps(w.zone, newX, w.y, widgetW, el.offsetHeight, w.id);
      if (collides) {
        const freePos = findFreePositionInZone(zone, newX, w.y, widgetW, el.offsetHeight, w.id);
        if (freePos) newX = freePos.x;
      }

      // Smooth horizontal glide animation
      el.classList.add('is-centering');
      el.style.left = `${newX}px`;
      w.x = newX;
      saveState();

      setTimeout(() => {
        el.classList.remove('is-centering');
      }, 340);
    }

    function addWidget(type, zoneId, customX = null, customY = null) {
      let targetZone = zoneId;
      const zoneEl = document.getElementById(targetZone);
      if (!zoneEl) return;

      const defW = type === 'l.media' ? 336 : (type === 'l.embed' ? 360 : (type === 'l.notes' ? 280 : (type === 'l.weather' ? 260 : 220)));
      const defH = type === 'l.media' ? 144 : (type === 'l.embed' ? 140 : (type === 'l.notes' ? 180 : 110));

      let posX = customX !== null ? snapToGrid(customX) : GRID_OFFSET;
      let posY = customY !== null ? snapToGrid(customY) : GRID_OFFSET;

      const maxGridX = Math.max(GRID_OFFSET, Math.floor((zoneEl.clientWidth - defW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
      const maxGridY = Math.max(GRID_OFFSET, Math.floor((zoneEl.clientHeight - defH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
      posX = Math.max(GRID_OFFSET, Math.min(posX, maxGridX));
      posY = Math.max(GRID_OFFSET, Math.min(posY, maxGridY));

      let pos = findFreePositionInZone(zoneEl, posX, posY, defW, defH);
      if (!pos) {
        const freeZoneId = ALL_ZONES.find(zid => {
          const z = document.getElementById(zid);
          return z && findFreePositionInZone(z, 8, 8, defW, defH);
        });
        if (freeZoneId) {
          targetZone = freeZoneId;
          const fzEl = document.getElementById(targetZone);
          pos = findFreePositionInZone(fzEl, 8, 8, defW, defH);
        } else {
          flashZoneFull(zoneEl);
          return;
        }
      }

      const newId = `${type.replace('l.', '')}_${Date.now()}`;
      let newWidget = {
        id: newId,
        type: type,
        zone: targetZone,
        x: pos.x,
        y: pos.y,
        width: `${defW}px`,
        height: `${defH}px`,
        config: getDefaultWidgetConfig(type)
      };
      appState.widgets.push(newWidget);
      saveState();
      renderWidgets();
    }

    function removeWidget(id) {
      appState.widgets = appState.widgets.filter(w => w.id !== id);
      saveState();
      renderWidgets();
    }

    // ──────────────── Widget Rendering & Handles ────────────────
    function renderWidgets() {
      document.querySelectorAll('.drop-zone').forEach(z => z.innerHTML = '');

      appState.widgets.forEach(w => {
        const zone = document.getElementById(w.zone);
        if (!zone) return;

        const el = document.createElement('div');
        el.className = `widget widget-${w.type.replace('l.', '')}`;
        el.id = w.id;
        if (w.width) el.style.width = w.width;
        if (w.height) el.style.height = w.height;

        el.style.left = `${typeof w.x === 'number' ? w.x : 16}px`;
        el.style.top = `${typeof w.y === 'number' ? w.y : 16}px`;

        if (w.type === 'l.clock') {
          el.innerHTML = renderClockHtml(w);
        } else if (w.type === 'l.weather') {
          el.innerHTML = renderWeatherHtml(w);
          fetchWeatherForWidget(w, el);
        } else if (w.type === 'l.embed') {
          el.innerHTML = renderEmbedHtml(w);
        } else if (w.type === 'l.notes') {
          el.innerHTML = renderNotesHtml(w);
        } else if (w.type === 'l.media') {
          el.innerHTML = renderMediaHtml(w);
          initMediaWidgetEvents(w, el);
        }

        // Overlay controls in Edit Mode
        const controls = document.createElement('div');
        controls.className = 'widget-controls';
        controls.innerHTML = `
          <button type="button" class="widget-btn-remove" title="Remove Widget" onclick="removeWidget('${w.id}')">&times;</button>
          <button type="button" class="widget-btn-center" title="Center Horizontally" onclick="centerWidgetHorizontally('${w.id}')">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <line x1="12" y1="2" x2="12" y2="22"></line>
              <path d="M4 8h16"></path>
              <path d="M6 16h12"></path>
            </svg>
          </button>
          <button type="button" class="widget-btn-gear" title="Configure Widget" onclick="openSettingsModal('${w.id}')">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <circle cx="12" cy="12" r="3"></circle>
              <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
            </svg>
          </button>
        `;
        el.appendChild(controls);

        // 8 Vector Anchor Handles for Transform Mode
        ['nw', 'n', 'ne', 'e', 'se', 's', 'sw', 'w'].forEach(h => {
          const handle = document.createElement('div');
          handle.className = `transform-handle handle-${h}`;
          handle.dataset.direction = h;
          handle.addEventListener('mousedown', (e) => startTransformResize(e, w.id, h));
          el.appendChild(handle);
        });

        // Middle mouse wheel click -> Strict Single Click Toggle for Transform/Resize Mode
        el.addEventListener('mousedown', (e) => {
          if (e.button === 1) {
            e.preventDefault();
            e.stopPropagation();
            return;
          }
          if (!appState.editMode || el.classList.contains('transform-mode')) return;
          if (e.target.closest('.widget-controls') || e.target.closest('.transform-handle')) return;
          if (e.target.closest('textarea') || e.target.closest('input') || e.target.closest('.notes-md-toggle') || e.target.closest('.media-btn') || e.target.closest('.media-progress-track')) return;
          if (e.button !== 0) return;
          startWidgetFreeformDrag(e, w, el);
        });

        el.addEventListener('auxclick', (e) => {
          if (e.button === 1) {
            e.preventDefault();
            e.stopPropagation();
            if (!appState.editMode) {
              document.body.classList.add('edit-mode');
              appState.editMode = true;
              const toggleBtn = document.getElementById('btn-edit-mode');
              if (toggleBtn) toggleBtn.classList.add('active');
            }
            const wasActive = el.classList.contains('transform-mode');
            document.querySelectorAll('.widget.transform-mode').forEach(wm => {
              wm.classList.remove('transform-mode');
            });
            if (!wasActive) {
              el.classList.add('transform-mode');
            }
          }
        });

        el.addEventListener('dblclick', (e) => {
          e.stopPropagation();
          if (!appState.editMode) {
            document.body.classList.add('edit-mode');
            appState.editMode = true;
            const toggleBtn = document.getElementById('btn-edit-mode');
            if (toggleBtn) toggleBtn.classList.add('active');
          }
          const wasActive = el.classList.contains('transform-mode');
          document.querySelectorAll('.widget.transform-mode').forEach(wm => {
            wm.classList.remove('transform-mode');
          });
          if (!wasActive) {
            el.classList.add('transform-mode');
          }
        });

        zone.appendChild(el);
      });
    }

    // Dismiss Transform Mode on outside click
    document.addEventListener('click', (e) => {
      if (!e.target.closest('.widget')) {
        document.querySelectorAll('.widget.transform-mode').forEach(w => w.classList.remove('transform-mode'));
      }
      if (!e.target.closest('.custom-dropdown')) {
        closeAllCustomDropdowns();
      }
    });

    // ──────────────── Freeform Micro-Grid Dragging with Flight Elevation ────────────────
    function startWidgetFreeformDrag(startEvent, w, el) {
      startEvent.preventDefault();
      startEvent.stopPropagation();

      document.body.classList.add('is-widget-dragging');
      el.classList.add('is-dragging');

      const elRect = el.getBoundingClientRect();
      const startMouseX = startEvent.clientX;
      const startMouseY = startEvent.clientY;
      const grabOffsetX = startMouseX - elRect.left;
      const grabOffsetY = startMouseY - elRect.top;
      const widgetW = el.offsetWidth;
      const widgetH = el.offsetHeight;

      const originalZone = document.getElementById(w.zone);
      let activeHoverZone = originalZone;

      // Move element directly to document.body during flight so it is NEVER clipped
      // by parent zone overflow:hidden or trapped underneath adjacent zones / search bar!
      document.body.appendChild(el);
      el.style.position = 'fixed';
      el.style.width = `${widgetW}px`;
      el.style.height = `${widgetH}px`;
      el.style.left = `${elRect.left}px`;
      el.style.top = `${elRect.top}px`;
      el.style.zIndex = '999999';

      function onMouseMove(moveEvent) {
        // Float element under cursor
        const floatLeft = moveEvent.clientX - grabOffsetX;
        const floatTop = moveEvent.clientY - grabOffsetY;
        el.style.left = `${floatLeft}px`;
        el.style.top = `${floatTop}px`;

        // Direct bounding box zone detection
        const hoveredZone = getDropZoneAtPoint(moveEvent.clientX, moveEvent.clientY);

        if (!hoveredZone) {
          // Outside any zone: smooth flight elevation
          activeHoverZone = null;
          el.classList.add('widget-lifted');
          removeDropPreview();
          document.querySelectorAll('.drop-zone').forEach(z => {
            z.classList.remove('drag-over', 'drag-error');
          });
        } else if (hoveredZone.id === w.zone) {
          // Inside original zone: NOT lifted, NO drop preview underneath
          activeHoverZone = hoveredZone;
          el.classList.remove('widget-lifted');
          removeDropPreview();
          document.querySelectorAll('.drop-zone').forEach(z => {
            z.classList.remove('drag-over', 'drag-error');
          });
        } else {
          // Hovering over a DIFFERENT zone: un-lift smoothly, show landing preview
          activeHoverZone = hoveredZone;
          el.classList.remove('widget-lifted');

          const hzRect = hoveredZone.getBoundingClientRect();
          const maxGridX = Math.max(GRID_OFFSET, Math.floor((hoveredZone.clientWidth - widgetW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
          const maxGridY = Math.max(GRID_OFFSET, Math.floor((hoveredZone.clientHeight - widgetH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
          let landX = snapToGrid(moveEvent.clientX - grabOffsetX - hzRect.left);
          let landY = snapToGrid(moveEvent.clientY - grabOffsetY - hzRect.top);
          landX = Math.max(GRID_OFFSET, Math.min(landX, maxGridX));
          landY = Math.max(GRID_OFFSET, Math.min(landY, maxGridY));

          const collides = getZoneOverlaps(hoveredZone.id, landX, landY, widgetW, widgetH, w.id);
          const isFull = getZoneWidgetCount(hoveredZone.id, w.id) >= MAX_ZONE_WIDGETS;

          document.querySelectorAll('.drop-zone').forEach(z => {
            if (z !== hoveredZone) z.classList.remove('drag-over', 'drag-error');
          });

          if (isFull || collides) {
            hoveredZone.classList.remove('drag-over');
            hoveredZone.classList.add('drag-error');
            showDropPreview(hoveredZone, landX, landY, widgetW, widgetH, true);
          } else {
            hoveredZone.classList.remove('drag-error');
            hoveredZone.classList.add('drag-over');
            showDropPreview(hoveredZone, landX, landY, widgetW, widgetH, false);
          }
        }
      }

      function onMouseUp(upEvent) {
        document.body.classList.remove('is-widget-dragging');
        el.classList.remove('is-dragging', 'widget-lifted');
        removeDropPreview();
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);

        document.querySelectorAll('.drop-zone').forEach(z => {
          z.classList.remove('drag-over', 'drag-error');
        });

        if (activeHoverZone && activeHoverZone.id !== w.zone) {
          const isFull = getZoneWidgetCount(activeHoverZone.id, w.id) >= MAX_ZONE_WIDGETS;
          if (!isFull) {
            const hzRect = activeHoverZone.getBoundingClientRect();
            let rawX = snapToGrid(upEvent.clientX - grabOffsetX - hzRect.left);
            let rawY = snapToGrid(upEvent.clientY - grabOffsetY - hzRect.top);
            const maxGridX = Math.max(GRID_OFFSET, Math.floor((activeHoverZone.clientWidth - widgetW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            const maxGridY = Math.max(GRID_OFFSET, Math.floor((activeHoverZone.clientHeight - widgetH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
            rawX = Math.max(GRID_OFFSET, Math.min(rawX, maxGridX));
            rawY = Math.max(GRID_OFFSET, Math.min(rawY, maxGridY));

            const freePos = findFreePositionInZone(activeHoverZone, rawX, rawY, widgetW, widgetH, w.id);
            if (freePos) {
              w.zone = activeHoverZone.id;
              w.x = freePos.x;
              w.y = freePos.y;
            } else {
              flashZoneFull(activeHoverZone);
            }
          } else {
            flashZoneFull(activeHoverZone);
          }
        } else if (originalZone) {
          const ozRect = originalZone.getBoundingClientRect();
          let rawX = snapToGrid(upEvent.clientX - grabOffsetX - ozRect.left);
          let rawY = snapToGrid(upEvent.clientY - grabOffsetY - ozRect.top);
          const maxGridX = Math.max(GRID_OFFSET, Math.floor((originalZone.clientWidth - widgetW - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
          const maxGridY = Math.max(GRID_OFFSET, Math.floor((originalZone.clientHeight - widgetH - GRID_OFFSET) / GRID_STEP) * GRID_STEP + GRID_OFFSET);
          rawX = Math.max(GRID_OFFSET, Math.min(rawX, maxGridX));
          rawY = Math.max(GRID_OFFSET, Math.min(rawY, maxGridY));

          const freePos = findFreePositionInZone(originalZone, rawX, rawY, widgetW, widgetH, w.id);
          if (freePos) {
            w.x = freePos.x;
            w.y = freePos.y;
          }
        }

        // Cleanly remove the floating clone from document.body before rerender
        if (el.parentElement) {
          el.remove();
        }
        saveState();
        renderWidgets();
      }

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    }

    // ──────────────── 8-Anchor Resize Transformation ────────────────
    function startTransformResize(e, widgetId, dir) {
      e.preventDefault();
      e.stopPropagation();

      const el = document.getElementById(widgetId);
      const widgetData = appState.widgets.find(w => w.id === widgetId);
      if (!el || !widgetData) return;

      document.body.classList.add('is-resizing');
      el.classList.add('is-resizing-active');

      const startX = e.clientX;
      const startY = e.clientY;
      const startW = el.offsetWidth;
      const startH = el.offsetHeight;
      const startL = parseInt(el.style.left, 10) || widgetData.x || 8;
      const startT = parseInt(el.style.top, 10) || widgetData.y || 8;
      const startRight = startL + startW;
      const startBottom = startT + startH;

      const zoneEl = el.closest('.drop-zone');
      const zoneW = zoneEl ? zoneEl.clientWidth : 600;
      const zoneH = zoneEl ? zoneEl.clientHeight : 400;

      const minW = 120;
      const minH = 80;

      let rafId = null;

      function onMouseMove(moveEvent) {
        const dx = moveEvent.clientX - startX;
        const dy = moveEvent.clientY - startY;

        let curL = startL;
        let curT = startT;
        let curW = startW;
        let curH = startH;

        const maxZoneW = zoneW - 8;
        const maxZoneH = zoneH - 8;

        // East: drag right handle
        if (dir.includes('e')) {
          curW = snapDimension(Math.max(minW, Math.min(startW + dx, maxZoneW - startL)));
        }
        // West: drag left handle (right edge stays fixed at startRight)
        if (dir.includes('w')) {
          curL = snapToGrid(Math.max(8, Math.min(startL + dx, startRight - minW)));
          curW = startRight - curL;
        }
        // South: drag bottom handle
        if (dir.includes('s')) {
          curH = snapDimension(Math.max(minH, Math.min(startH + dy, maxZoneH - startT)));
        }
        // North: drag top handle (bottom edge stays fixed at startBottom)
        if (dir.includes('n')) {
          curT = snapToGrid(Math.max(8, Math.min(startT + dy, startBottom - minH)));
          curH = startBottom - curT;
        }

        // Prevent resizing from overlapping adjacent widgets in the same zone
        const collides = getZoneOverlaps(widgetData.zone, curL, curT, curW, curH, widgetId);
        if (collides) {
          return;
        }

        if (rafId) cancelAnimationFrame(rafId);
        rafId = requestAnimationFrame(() => {
          el.style.left = `${Math.round(curL)}px`;
          el.style.top = `${Math.round(curT)}px`;
          el.style.width = `${Math.round(curW)}px`;
          el.style.height = `${Math.round(curH)}px`;
        });
      }

      function onMouseUp() {
        if (rafId) cancelAnimationFrame(rafId);
        document.body.classList.remove('is-resizing');
        setTimeout(() => {
          el.classList.remove('is-resizing-active');
        }, 140);
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);

        widgetData.width = `${snapDimension(parseInt(el.style.width, 10) || startW)}px`;
        widgetData.height = `${snapDimension(parseInt(el.style.height, 10) || startH)}px`;
        widgetData.x = snapToGrid(parseInt(el.style.left, 10) || widgetData.x);
        widgetData.y = snapToGrid(parseInt(el.style.top, 10) || widgetData.y);
        saveState();
      }

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    }

    // ──────────────── Clock Implementation ────────────────
    function renderClockHtml(w) {
      const cfg = w.config || {};
      const fontCls = cfg.fontFamily === 'sans' ? 'font-sans' : 'font-mono';
      const sizeStyle = cfg.fontSize === 'large' ? 'font-size:46px;' : (cfg.fontSize === 'compact' ? 'font-size:28px;' : 'font-size:38px;');
      return `
        <div class="clock-time ${fontCls}" id="clock-time-${w.id}" style="${sizeStyle}">--:--</div>
        <div class="clock-date" id="clock-date-${w.id}">---, --- --</div>
      `;
    }

    function updateClocks() {
      const now = new Date();
      appState.widgets.filter(w => w.type === 'l.clock').forEach(w => {
        const timeEl = document.getElementById(`clock-time-${w.id}`);
        const dateEl = document.getElementById(`clock-date-${w.id}`);
        if (!timeEl || !dateEl) return;

        const cfg = w.config || {};
        let h = now.getHours();
        const m = String(now.getMinutes()).padStart(2, '0');
        const s = String(now.getSeconds()).padStart(2, '0');
        let ampmHtml = '';

        if (cfg.timeFormat === '12h') {
          const ampm = h >= 12 ? 'PM' : 'AM';
          h = h % 12;
          if (h === 0) h = 12;
          ampmHtml = `<span class="ampm">${ampm}</span>`;
        }
        const hStr = String(h).padStart(2, '0');

        if (cfg.showSeconds === false) {
          timeEl.innerHTML = `${hStr}:${m}${ampmHtml}`;
        } else {
          timeEl.innerHTML = `${hStr}:${m}<span class="sec">:${s}</span>${ampmHtml}`;
        }

        if (cfg.dateFormat === 'hide') {
          dateEl.style.display = 'none';
        } else {
          dateEl.style.display = 'block';
          if (cfg.dateFormat === 'long') {
            dateEl.textContent = now.toLocaleDateString('en-US', { weekday: 'long', month: 'long', day: 'numeric' });
          } else {
            dateEl.textContent = now.toLocaleDateString('en-US', { weekday: 'short', month: 'short', day: 'numeric' });
          }
        }
      });
    }
    setInterval(updateClocks, 1000);

    // ──────────────── Weather Implementation ────────────────
    function renderWeatherHtml(w) {
      const cfg = w.config || {};
      const isCompact = cfg.viewMode === 'compact';
      const showIcons = cfg.showIcons !== false;
      const iconDisplay = showIcons ? 'display:flex;' : 'display:none;';

      return `
        <div class="weather-icon-box" id="weather-icon-${w.id}" style="${iconDisplay}">
          <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="4"></circle>
            <path d="M12 2v2"></path><path d="M12 20v2"></path>
            <path d="m4.93 4.93 1.41 1.41"></path><path d="m17.66 17.66 1.41 1.41"></path>
            <path d="M2 12h2"></path><path d="M20 12h2"></path>
            <path d="m6.34 17.66-1.41 1.41"></path><path d="m19.07 4.93-1.41 1.41"></path>
          </svg>
        </div>
        <div class="weather-info">
          <div class="weather-temp" id="weather-temp-${w.id}">--&deg;</div>
          <div class="weather-cond" id="weather-cond-${w.id}">Updating...</div>
          <div class="weather-loc" id="weather-loc-${w.id}">${escapeHtml(cfg.city || 'Local')}</div>
        </div>
      `;
    }

    const WMO_CODE_MAP = {
      0: { label: 'Clear sky', icon: 'sun' },
      1: { label: 'Mainly clear', icon: 'sun-cloud' },
      2: { label: 'Partly cloudy', icon: 'cloud' },
      3: { label: 'Overcast', icon: 'cloud' },
      45: { label: 'Fog', icon: 'fog' },
      48: { label: 'Depositing rime fog', icon: 'fog' },
      51: { label: 'Light drizzle', icon: 'rain' },
      61: { label: 'Slight rain', icon: 'rain' },
      63: { label: 'Moderate rain', icon: 'rain' },
      65: { label: 'Heavy rain', icon: 'rain' },
      71: { label: 'Slight snow', icon: 'snow' },
      73: { label: 'Moderate snow', icon: 'snow' },
      75: { label: 'Heavy snow', icon: 'snow' },
      95: { label: 'Thunderstorm', icon: 'thunder' }
    };

    function getWeatherGlyphSvg(type) {
      if (type === 'sun') {
        return `<svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="4"></circle><path d="M12 2v2"></path><path d="M12 20v2"></path><path d="m4.93 4.93 1.41 1.41"></path><path d="m17.66 17.66 1.41 1.41"></path><path d="M2 12h2"></path><path d="M20 12h2"></path><path d="m6.34 17.66-1.41 1.41"></path><path d="m19.07 4.93-1.41 1.41"></path></svg>`;
      } else if (type === 'rain') {
        return `<svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 14.899A7 7 0 1 1 15.71 8h1.79a4.5 4.5 0 0 1 2.5 8.242"></path><path d="M16 14v6"></path><path d="M8 14v6"></path><path d="M12 16v6"></path></svg>`;
      } else if (type === 'snow') {
        return `<svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M20 17.58A5 5 0 0 0 18 8h-1.26A8 8 0 1 0 4 16.25"></path><line x1="8" y1="16" x2="8.01" y2="16"></line><line x1="8" y1="20" x2="8.01" y2="20"></line><line x1="12" y1="18" x2="12.01" y2="18"></line><line x1="12" y1="22" x2="12.01" y2="22"></line><line x1="16" y1="16" x2="16.01" y2="16"></line><line x1="16" y1="20" x2="16.01" y2="20"></line></svg>`;
      }
      return `<svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z"></path></svg>`;
    }

    function fetchWeatherForWidget(w, el) {
      const cfg = w.config || {};
      const lat = cfg.lat || 55.7558;
      const lon = cfg.lon || 37.6173;
      const isImperial = cfg.units === 'imperial';
      const tempUnit = isImperial ? '&temperature_unit=fahrenheit' : '';
      const windUnit = cfg.windUnits === 'mph' ? '&windspeed_unit=mph' : (cfg.windUnits === 'ms' ? '&windspeed_unit=ms' : '&windspeed_unit=kmh');

      fetch(`https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current_weather=true${tempUnit}${windUnit}`)
        .then(res => res.json())
        .then(data => {
          if (!data || !data.current_weather) return;
          const cw = data.current_weather;
          const t = Math.round(cw.temperature);
          const sign = t > 0 ? '+' : '';
          const code = cw.weathercode;
          const meta = WMO_CODE_MAP[code] || { label: 'Overcast', icon: 'cloud' };

          const tempEl = el.querySelector(`#weather-temp-${w.id}`);
          const condEl = el.querySelector(`#weather-cond-${w.id}`);
          const iconEl = el.querySelector(`#weather-icon-${w.id}`);

          if (tempEl) tempEl.textContent = `${sign}${t}°${isImperial ? 'F' : 'C'}`;
          if (condEl) condEl.textContent = meta.label;
          if (iconEl) iconEl.innerHTML = getWeatherGlyphSvg(meta.icon);
        })
        .catch(() => {
          const condEl = el.querySelector(`#weather-cond-${w.id}`);
          if (condEl) condEl.textContent = 'Offline';
        });
    }

    // ──────────────── Embed Implementation ────────────────
    function renderEmbedHtml(w) {
      const cfg = w.config || {};
      const pad = cfg.padding !== undefined ? cfg.padding : 8;
      const bg = cfg.transparent !== false ? 'transparent' : 'var(--bg-base)';
      const border = cfg.border === 'none' ? 'border:none;' : 'border:1px solid var(--border);';
      const sandbox = cfg.allowScripts !== false ? 'sandbox="allow-scripts allow-same-origin"' : 'sandbox=""';
      const payload = cfg.html || '';

      // UTF-8 metadata prepended to srcdoc ensures zero Cyrillic truncation or garbling in WebKit
      const safeSrcdoc = `<meta charset="UTF-8"><style>:root{color-scheme:dark;}body{margin:0;padding:${pad}px;font-family:sans-serif;color:var(--fg-primary,#f1f5f9);box-sizing:border-box;}</style>${payload}`;

      return `
        <div class="embed-inner" style="background:${bg}; ${border}">
          <iframe ${sandbox} srcdoc="${escapeAttr(safeSrcdoc)}"></iframe>
        </div>
      `;
    }

    // ──────────────── Notes Implementation ────────────────
    function renderMarkdownToHtml(md) {
      if (!md) return '<em style="color:var(--fg-muted)">No notes entered yet...</em>';
      let html = escapeHtml(md);

      // Code blocks
      html = html.replace(/```([\s\S]*?)```/g, (m, code) => `<pre><code>${code.trim()}</code></pre>`);
      // Inline code
      html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
      // Headers
      html = html.replace(/^### (.*$)/gim, '<h3>$1</h3>');
      html = html.replace(/^## (.*$)/gim, '<h2>$1</h2>');
      html = html.replace(/^# (.*$)/gim, '<h1>$1</h1>');
      // Bold & Italic
      html = html.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
      html = html.replace(/\*([^*]+)\*/g, '<em>$1</em>');
      // Blockquotes
      html = html.replace(/^\> (.*$)/gim, '<blockquote>$1</blockquote>');
      // Checkboxes
      html = html.replace(/- \[x\] (.*$)/gim, '<div style="display:flex;align-items:center;gap:6px;margin-bottom:3px;"><input type="checkbox" checked disabled/> <span style="text-decoration:line-through;opacity:0.7;">$1</span></div>');
      html = html.replace(/- \[ \] (.*$)/gim, '<div style="display:flex;align-items:center;gap:6px;margin-bottom:3px;"><input type="checkbox" disabled/> <span>$1</span></div>');
      // Lists
      html = html.replace(/^\- (.*$)/gim, '<li>$1</li>');
      html = html.replace(/(<li>.*<\/li>)/gim, '<ul>$1</ul>');
      html = html.replace(/<\/ul>\s*<ul>/g, '');
      // Line breaks
      html = html.replace(/\n/g, '<br>');
      return html;
    }

    function renderNotesHtml(w) {
      const cfg = w.config || {};
      const theme = cfg.theme || 'macos';
      const isMarkdown = !!cfg.markdown;
      const isMDPreview = !!w._previewMode;
      const title = cfg.title || 'Notes';
      const text = cfg.text !== undefined ? cfg.text : '# Notes\n- Type notes or ideas here\n- Supports **bold** and `code`';
      const fs = cfg.fontSize || '13px';
      const wrap = cfg.wrap || 'wrap';

      let headerHtml = '';
      if (theme === 'macos') {
        headerHtml = `
          <div class="notes-header">
            <div class="macos-dots">
              <span class="macos-dot dot-red"></span>
              <span class="macos-dot dot-yellow"></span>
              <span class="macos-dot dot-green"></span>
            </div>
            <div class="notes-title">${escapeHtml(title)}</div>
            ${isMarkdown ? `<button type="button" class="notes-md-toggle" onclick="toggleNotesMarkdownPreview('${w.id}')" title="Toggle Preview / Edit">${isMDPreview ? 'Edit' : 'MD'}</button>` : '<span style="width:24px"></span>'}
          </div>
        `;
      } else if (theme === 'cyber') {
        headerHtml = `
          <div class="notes-header">
            <div class="cyber-tag">
              <span class="cyber-dot"></span>
              <span>[BUFFER // ${escapeHtml(title.toUpperCase())}]</span>
            </div>
            ${isMarkdown ? `<button type="button" class="notes-md-toggle" onclick="toggleNotesMarkdownPreview('${w.id}')" title="Toggle Preview / Edit">${isMDPreview ? 'Edit' : 'MD'}</button>` : '<span style="width:24px"></span>'}
          </div>
        `;
      } else if (theme === 'parchment') {
        headerHtml = `
          <div class="notes-header">
            <div style="display:flex;align-items:center;gap:6px;">
              <span style="font-size:12px;">📌</span>
              <span class="notes-title" style="position:static;font-weight:700;">${escapeHtml(title)}</span>
            </div>
            ${isMarkdown ? `<button type="button" class="notes-md-toggle" onclick="toggleNotesMarkdownPreview('${w.id}')" title="Toggle Preview / Edit">${isMDPreview ? 'Edit' : 'MD'}</button>` : '<span style="width:24px"></span>'}
          </div>
        `;
      } else { // 'glass'
        headerHtml = `
          <div class="notes-header">
            <div style="display:flex;align-items:center;gap:6px;">
              <span style="width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent-glow);"></span>
              <span class="notes-title" style="position:static;font-weight:600;">${escapeHtml(title)}</span>
            </div>
            ${isMarkdown ? `<button type="button" class="notes-md-toggle" onclick="toggleNotesMarkdownPreview('${w.id}')" title="Toggle Preview / Edit">${isMDPreview ? 'Edit' : 'MD'}</button>` : '<span style="width:24px"></span>'}
          </div>
        `;
      }

      let bodyHtml = '';
      if (isMarkdown && isMDPreview) {
        bodyHtml = `<div class="notes-markdown-view" style="font-size:${fs};" ondblclick="toggleNotesMarkdownPreview('${w.id}')">${renderMarkdownToHtml(text)}</div>`;
      } else {
        bodyHtml = `<textarea class="notes-textarea" style="font-size:${fs}; white-space:${wrap === 'nowrap' ? 'pre' : 'pre-wrap'};" placeholder="Type notes here..." oninput="onNotesInput('${w.id}', this)">${escapeHtml(text)}</textarea>`;
      }

      return `
        <div class="notes-inner theme-${theme}">
          ${headerHtml}
          <div class="notes-body">
            ${bodyHtml}
          </div>
        </div>
      `;
    }

    function toggleNotesMarkdownPreview(widgetId) {
      const w = appState.widgets.find(x => x.id === widgetId);
      if (!w) return;
      w._previewMode = !w._previewMode;
      renderWidgets();
    }

    let notesSaveTimeout = null;
    function onNotesInput(widgetId, textarea) {
      const w = appState.widgets.find(x => x.id === widgetId);
      if (!w) return;
      if (!w.config) w.config = {};
      w.config.text = textarea.value;
      if (notesSaveTimeout) clearTimeout(notesSaveTimeout);
      notesSaveTimeout = setTimeout(saveState, 350);
    }

    // ──────────────── Media Player Logic & MPRIS Controller ────────────────
    let globalMediaState = {
      hasPlayer: false,
      isPlaying: false,
      title: 'Not Playing',
      artist: '',
      album: '',
      artUrl: '',
      duration: 0,
      position: 0
    };

    let mediaTimerInterval = null;

    function formatMediaTime(sec) {
      if (isNaN(sec) || sec < 0) sec = 0;
      const m = Math.floor(sec / 60);
      const s = Math.floor(sec % 60);
      return `${m}:${s < 10 ? '0' : ''}${s}`;
    }

    function formatMediaRemainingTime(pos, dur) {
      if (isNaN(pos) || isNaN(dur) || dur <= 0) return '-0:00';
      const rem = Math.max(0, dur - pos);
      const m = Math.floor(rem / 60);
      const s = Math.floor(rem % 60);
      return `-${m}:${s < 10 ? '0' : ''}${s}`;
    }

    function startLocalMediaTicker() {
      if (mediaTimerInterval) clearInterval(mediaTimerInterval);
      mediaTimerInterval = setInterval(() => {
        if (globalMediaState.hasPlayer && globalMediaState.isPlaying && globalMediaState.duration > 0) {
          globalMediaState.position = Math.min(globalMediaState.duration, globalMediaState.position + 1);
          updateAllMediaWidgetsUi();
        }
      }, 1000);
    }
    startLocalMediaTicker();

    window.__updateLumenMediaState = function(state) {
      if (!state) return;
      if (state.hasPlayer && state.title) {
        globalMediaState.hasPlayer = true;
        globalMediaState.isPlaying = (state.playbackStatus === 'Playing');
        globalMediaState.title = state.title;
        globalMediaState.artist = state.artist || '';
        globalMediaState.album = state.album || '';
        globalMediaState.artUrl = state.artUrl || '';
        globalMediaState.duration = typeof state.duration === 'number' ? Math.max(0, state.duration) : 0;
        globalMediaState.position = typeof state.position === 'number' ? Math.max(0, state.position) : 0;
      } else {
        globalMediaState.hasPlayer = false;
        globalMediaState.isPlaying = false;
        globalMediaState.title = 'Not Playing';
        globalMediaState.artist = '';
        globalMediaState.album = '';
        globalMediaState.artUrl = '';
        globalMediaState.duration = 0;
        globalMediaState.position = 0;
      }
      updateAllMediaWidgetsUi();
    };

    function sendMediaCommand(action, value = 0) {
      if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.lumenMedia) {
        try {
          window.webkit.messageHandlers.lumenMedia.postMessage(JSON.stringify({
            action: action,
            value: value
          }));
        } catch (e) {}
      }
      if (action === 'playPause') {
        globalMediaState.isPlaying = !globalMediaState.isPlaying;
      }
      updateAllMediaWidgetsUi();
    }

    function renderMediaHtml(w) {
      const cfg = w.config || {};
      const showWaveform = cfg.waveform !== false;
      const showArt = cfg.showArt !== false;

      const hasMedia = globalMediaState.hasPlayer;
      const isPlaying = hasMedia && globalMediaState.isPlaying;
      const title = globalMediaState.title || 'Not Playing';
      const artist = hasMedia ? (globalMediaState.artist || '') : '';
      const artUrl = hasMedia ? globalMediaState.artUrl : '';
      const pos = hasMedia ? globalMediaState.position : 0;
      const dur = hasMedia ? globalMediaState.duration : 0;
      const pct = (dur > 0 && hasMedia) ? Math.min(100, Math.max(0, (pos / dur) * 100)) : 0;

      const elapsedStr = formatMediaTime(pos);
      const remainStr = (dur > 0 && hasMedia) ? formatMediaRemainingTime(pos, dur) : '-0:00';

      let artHtml = '';
      if (showArt) {
        artHtml = `<div class="media-art-wrap"><img src="${escapeAttr(artUrl)}" class="media-art-img" alt="Cover" style="${hasMedia && artUrl ? '' : 'display:none;'}" onerror="this.style.display='none';if(this.nextElementSibling)this.nextElementSibling.style.display='flex';" /><div class="media-art-placeholder" style="${hasMedia && artUrl ? 'display:none;' : ''}"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18V5l12-2v13"></path><circle cx="6" cy="18" r="3"></circle><circle cx="18" cy="16" r="3"></circle></svg></div></div>`;
      }

      const disabledAttr = hasMedia ? '' : 'disabled';
      const disabledClass = hasMedia ? '' : 'is-disabled';

      return `
        <div class="media-container ${hasMedia ? 'has-media' : 'no-media'}" id="media-widget-${w.id}">
          <div class="media-top-row">
            ${artHtml}
            <div class="media-meta">
              <div class="media-title" title="${escapeAttr(title)}">${escapeHtml(title)}</div>
              <div class="media-artist-marquee-wrapper">
                <span class="media-artist" id="media-artist-${w.id}">${escapeHtml(artist)}</span>
              </div>
            </div>
            ${showWaveform ? `
            <div class="media-waveform ${isPlaying ? 'is-playing' : 'is-paused'}" id="media-waveform-${w.id}">
              <span class="wave-bar bar-1"></span>
              <span class="wave-bar bar-2"></span>
              <span class="wave-bar bar-3"></span>
              <span class="wave-bar bar-4"></span>
              <span class="wave-bar bar-5"></span>
            </div>` : ''}
          </div>

          <div class="media-progress-row">
            <span class="media-time media-time-elapsed" id="media-elapsed-${w.id}">${elapsedStr}</span>
            <div class="media-progress-track ${disabledClass}" id="media-track-${w.id}" onmousedown="handleMediaTrackMouseDown(event, '${w.id}')" onclick="handleMediaTrackClick(event, '${w.id}')" title="${hasMedia ? 'Seek' : ''}">
              <div class="media-progress-fill" id="media-fill-${w.id}" style="width: ${pct}%;"></div>
            </div>
            <span class="media-time media-time-remaining" id="media-remaining-${w.id}">${remainStr}</span>
          </div>

          <div class="media-controls-row">
            <div class="media-btns-group">
              <button type="button" class="media-btn media-btn-prev ${disabledClass}" ${disabledAttr} onclick="sendMediaCommand('prev')" title="Previous Track">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor">
                  <rect x="5" y="6" width="2.5" height="12" rx="1.25"></rect>
                  <path d="M9.8 12.87a1.05 1.05 0 0 1 0-1.74l7.63-5.08c.7-.47 1.65.03 1.65.87v10.16c0 .84-.95 1.34-1.65.87l-7.63-5.08z"></path>
                </svg>
              </button>
              <button type="button" class="media-btn media-btn-playpause ${isPlaying ? '' : 'is-paused'} ${disabledClass}" ${disabledAttr} id="media-playpause-${w.id}" onclick="sendMediaCommand('playPause')" title="${hasMedia ? (isPlaying ? 'Pause' : 'Play') : ''}">
                <svg class="icon-pause" width="28" height="28" viewBox="0 0 24 24" fill="currentColor">
                  <rect x="6.5" y="5" width="3.5" height="14" rx="1.75"></rect>
                  <rect x="14" y="5" width="3.5" height="14" rx="1.75"></rect>
                </svg>
                <svg class="icon-play" width="28" height="28" viewBox="0 0 24 24" fill="currentColor">
                  <path d="M8.5 6.35c0-.98 1.07-1.59 1.91-1.07l9.42 5.88c.8.5.8 1.66 0 2.16l-9.42 5.88c-.84.52-1.91-.09-1.91-1.07V6.35z"></path>
                </svg>
              </button>
              <button type="button" class="media-btn media-btn-next ${disabledClass}" ${disabledAttr} onclick="sendMediaCommand('next')" title="Next Track">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor">
                  <path d="M6.5 6.8c0-.84.95-1.34 1.65-.87l7.63 5.08c.63.42.63 1.32 0 1.74l-7.63 5.08c-.7.47-1.65-.03-1.65-.87V6.8z"></path>
                  <rect x="16.5" y="6" width="2.5" height="12" rx="1.25"></rect>
                </svg>
              </button>
            </div>
          </div>
        </div>
      `;
    }

    function initMediaWidgetEvents(w, el) {
      setTimeout(() => updateMarqueeForWidget(w.id), 50);
      sendMediaCommand('poll');
    }

    function updateMarqueeForWidget(widgetId) {
      const artistEl = document.getElementById(`media-artist-${widgetId}`);
      if (!artistEl) return;
      const parent = artistEl.parentElement;
      if (!parent) return;

      const overflow = artistEl.scrollWidth - parent.clientWidth;
      if (overflow > 4) {
        const dist = overflow + 12;
        const duration = Math.max(5, Math.round(dist / 14));
        artistEl.style.setProperty('--marquee-distance', `-${dist}px`);
        artistEl.style.setProperty('--marquee-duration', `${duration}s`);
        artistEl.classList.add('is-marquee');
      } else {
        artistEl.classList.remove('is-marquee');
        artistEl.style.removeProperty('--marquee-distance');
        artistEl.style.removeProperty('--marquee-duration');
      }
    }

    function seekToTrackRatio(ratio, widgetId) {
      if (!globalMediaState.hasPlayer || globalMediaState.duration <= 0) return;
      const dur = globalMediaState.duration;
      const clampedRatio = Math.max(0, Math.min(1, ratio));
      const targetSec = Math.max(0, Math.min(dur, Math.round(clampedRatio * dur)));

      globalMediaState.position = targetSec;
      const pct = (dur > 0) ? (targetSec / dur) * 100 : 0;
      const fillEl = document.getElementById(`media-fill-${widgetId}`);
      if (fillEl) fillEl.style.width = `${pct}%`;
      const elapsedEl = document.getElementById(`media-elapsed-${widgetId}`);
      if (elapsedEl) elapsedEl.textContent = formatMediaTime(targetSec);
      const remainingEl = document.getElementById(`media-remaining-${widgetId}`);
      if (remainingEl) remainingEl.textContent = formatMediaRemainingTime(targetSec, dur);

      sendMediaCommand('setPosition', targetSec);
    }

    function handleMediaTrackClick(event, widgetId) {
      event.stopPropagation();
      if (!globalMediaState.hasPlayer || globalMediaState.duration <= 0) return;
      const trackEl = document.getElementById(`media-track-${widgetId}`);
      if (!trackEl) return;
      const rect = trackEl.getBoundingClientRect();
      if (rect.width <= 0) return;
      const clickX = event.clientX - rect.left;
      seekToTrackRatio(clickX / rect.width, widgetId);
    }

    function handleMediaTrackMouseDown(event, widgetId) {
      event.stopPropagation();
      if (event.button !== 0) return;
      if (!globalMediaState.hasPlayer || globalMediaState.duration <= 0) return;
      const trackEl = document.getElementById(`media-track-${widgetId}`);
      if (!trackEl) return;

      const rect = trackEl.getBoundingClientRect();
      if (rect.width <= 0) return;
      const clickX = event.clientX - rect.left;
      seekToTrackRatio(clickX / rect.width, widgetId);

      function onMouseMove(e) {
        const r = trackEl.getBoundingClientRect();
        if (r.width <= 0) return;
        const x = e.clientX - r.left;
        seekToTrackRatio(x / r.width, widgetId);
      }

      function onMouseUp(e) {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        const r = trackEl.getBoundingClientRect();
        if (r.width > 0) {
          const x = e.clientX - r.left;
          seekToTrackRatio(x / r.width, widgetId);
        }
      }

      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
    }

    let prevTrackSignature = '';

    function updateAllMediaWidgetsUi() {
      const currentSignature = globalMediaState.hasPlayer
        ? (globalMediaState.title + ':::' + globalMediaState.artist + ':::' + globalMediaState.artUrl)
        : '__no_media__';
      const isTrackTransition = (prevTrackSignature !== '' && prevTrackSignature !== currentSignature);
      prevTrackSignature = currentSignature;

      appState.widgets.forEach(w => {
        if (w.type !== 'l.media') return;
        const container = document.getElementById(`media-widget-${w.id}`);
        if (!container) return;

        if (isTrackTransition) {
          container.classList.add('is-track-transitioning');
          setTimeout(() => {
            container.classList.remove('is-track-transitioning');
          }, 180);
        }

        const hasMedia = globalMediaState.hasPlayer;
        const isPlaying = hasMedia && globalMediaState.isPlaying;
        const dur = hasMedia ? globalMediaState.duration : 0;
        const pos = hasMedia ? globalMediaState.position : 0;
        const pct = (dur > 0 && hasMedia) ? Math.min(100, Math.max(0, (pos / dur) * 100)) : 0;

        container.classList.toggle('has-media', hasMedia);
        container.classList.toggle('no-media', !hasMedia);

        const elapsedEl = document.getElementById(`media-elapsed-${w.id}`);
        if (elapsedEl) elapsedEl.textContent = formatMediaTime(pos);

        const remainingEl = document.getElementById(`media-remaining-${w.id}`);
        if (remainingEl) remainingEl.textContent = (dur > 0 && hasMedia) ? formatMediaRemainingTime(pos, dur) : '-0:00';

        const fillEl = document.getElementById(`media-fill-${w.id}`);
        if (fillEl) fillEl.style.width = `${pct}%`;

        const trackEl = document.getElementById(`media-track-${w.id}`);
        if (trackEl) {
          trackEl.classList.toggle('is-disabled', !hasMedia);
          trackEl.title = hasMedia ? 'Seek' : '';
        }

        const btns = container.querySelectorAll('.media-btn');
        btns.forEach(b => {
          b.classList.toggle('is-disabled', !hasMedia);
          b.disabled = !hasMedia;
        });

        const playPauseBtn = document.getElementById(`media-playpause-${w.id}`);
        if (playPauseBtn) {
          playPauseBtn.classList.toggle('is-paused', !isPlaying);
          playPauseBtn.title = hasMedia ? (isPlaying ? 'Pause' : 'Play') : '';
        }

        const waveEl = document.getElementById(`media-waveform-${w.id}`);
        if (waveEl) {
          waveEl.classList.toggle('is-playing', isPlaying);
          waveEl.classList.toggle('is-paused', !isPlaying);
        }

        const titleEl = container.querySelector('.media-title');
        const dispTitle = hasMedia ? (globalMediaState.title || 'Unknown Title') : 'Not Playing';
        if (titleEl && titleEl.textContent !== dispTitle) {
          titleEl.textContent = dispTitle;
          titleEl.title = dispTitle;
        }

        const artistEl = document.getElementById(`media-artist-${w.id}`);
        const dispArtist = hasMedia ? (globalMediaState.artist || '') : '';
        if (artistEl && artistEl.textContent !== dispArtist) {
          artistEl.textContent = dispArtist;
          updateMarqueeForWidget(w.id);
        }

        const artImg = container.querySelector('.media-art-img');
        const artPlaceholder = container.querySelector('.media-art-placeholder');
        if (artImg && artPlaceholder) {
          if (hasMedia && globalMediaState.artUrl) {
            if (artImg.getAttribute('src') !== globalMediaState.artUrl) {
              artImg.src = globalMediaState.artUrl;
            }
            artImg.style.display = 'block';
            artPlaceholder.style.display = 'none';
          } else {
            artImg.style.display = 'none';
            artPlaceholder.style.display = 'flex';
          }
        }
      });
    }

    // ──────────────── Contextual Settings Modal Logic ────────────────
    function openSettingsModal(widgetId) {
      const w = appState.widgets.find(x => x.id === widgetId);
      if (!w) return;
      appState.activeModalWidgetId = widgetId;

      const widgetNames = {
        'l.clock': 'Sleek Clock',
        'l.weather': 'Live Weather Monitor',
        'l.embed': 'Custom HTML Embed',
        'l.notes': 'Notes Window',
        'l.media': 'Media Player'
      };
      document.getElementById('modal-title').textContent = `Settings: ${widgetNames[w.type] || w.type}`;

      setupModalForWidget(w);
      document.getElementById('settings-modal').classList.add('open');
    }

    function closeSettingsModal() {
      closeAllCustomDropdowns();
      document.getElementById('settings-modal').classList.remove('open');
      appState.activeModalWidgetId = null;
    }

    function switchModalTab(tabId) {
      closeAllCustomDropdowns();
      document.querySelectorAll('.modal-sidebar-tab').forEach(b => {
        b.classList.toggle('active', b.dataset.tab === tabId);
      });
      document.querySelectorAll('.tab-pane').forEach(p => {
        p.classList.toggle('active', p.id === `tab-pane-${tabId}`);
      });
    }

    function setupModalForWidget(w) {
      const sidebar = document.getElementById('modal-sidebar');
      const container = document.getElementById('modal-panes-container');
      sidebar.innerHTML = '';
      container.innerHTML = '';

      let tabs = [];
      if (w.type === 'l.embed') {
        tabs = [
          { id: 'content', label: 'Content' },
          { id: 'display', label: 'Display' },
          { id: 'sandbox', label: 'Sandbox' }
        ];
      } else if (w.type === 'l.weather') {
        tabs = [
          { id: 'location', label: 'Location' },
          { id: 'display', label: 'Display' },
          { id: 'units', label: 'Units' }
        ];
      } else if (w.type === 'l.clock') {
        tabs = [
          { id: 'format', label: 'Format' },
          { id: 'typography', label: 'Typography' }
        ];
      } else if (w.type === 'l.notes') {
        tabs = [
          { id: 'theme', label: 'Theme & Style' },
          { id: 'editor', label: 'Markdown & Editor' },
          { id: 'info', label: 'Info' }
        ];
      } else if (w.type === 'l.media') {
        tabs = [
          { id: 'display', label: 'Display' },
          { id: 'source', label: 'Audio Source' },
          { id: 'info', label: 'Info' }
        ];
      }

      tabs.forEach((t, idx) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `modal-sidebar-tab ${idx === 0 ? 'active' : ''}`;
        btn.dataset.tab = t.id;
        btn.onclick = () => switchModalTab(t.id);
        btn.innerHTML = `
          <span class="tab-accent-indicator"></span>
          <span class="tab-text">${t.label}</span>
        `;
        sidebar.appendChild(btn);

        const pane = document.createElement('div');
        pane.className = `tab-pane ${idx === 0 ? 'active' : ''}`;
        pane.id = `tab-pane-${t.id}`;
        container.appendChild(pane);
      });

      const cfg = w.config || {};

      if (w.type === 'l.embed') {
        // [Content] Tab
        document.getElementById('tab-pane-content').innerHTML = `
          <div class="form-group">
            <label>HTML / CSS Source (UTF-8 &amp; Cyrillic Native)</label>
            <textarea id="cfg-embed-html" class="form-control" spellcheck="false">${escapeHtml(cfg.html || '')}</textarea>
          </div>
        `;

        // [Display] Tab
        document.getElementById('tab-pane-display').innerHTML = `
          <div class="form-group">
            <label>Dimensions Presets</label>
            ${renderCustomDropdownHtml('embed-dim', 'Presets', w.width === '100%' ? 'full' : (w.width === '420px' ? 'wide' : 'default'), [
              { val: 'default', label: 'Standard (320 × 140)', hint: 'Default' },
              { val: 'compact', label: 'Compact (260 × 100)', hint: 'Small' },
              { val: 'wide', label: 'Wide (420 × 160)', hint: 'Banner' },
              { val: 'full', label: 'Full Width (100% × 180)', hint: 'Expanded' }
            ])}
          </div>
          <div class="form-group">
            <label>Background Opacity</label>
            ${renderCustomDropdownHtml('embed-trans', 'Background', cfg.transparent !== false ? 'trans' : 'solid', [
              { val: 'trans', label: 'Transparent Background', hint: 'Glass' },
              { val: 'solid', label: 'Solid Base Color', hint: 'Canvas' }
            ])}
          </div>
          <div class="form-group">
            <label>Border Visibility</label>
            ${renderCustomDropdownHtml('embed-border', 'Border', cfg.border === 'none' ? 'none' : 'subtle', [
              { val: 'subtle', label: 'Subtle Outline Border', hint: '1px' },
              { val: 'none', label: 'No Border', hint: 'Frameless' }
            ])}
          </div>
          <div class="form-group">
            <label>Container Padding</label>
            <div class="form-slider-container">
              <input type="range" id="cfg-embed-pad" min="0" max="24" class="form-slider" value="${cfg.padding !== undefined ? cfg.padding : 8}" oninput="document.getElementById('cfg-embed-pad-val').textContent = this.value + 'px'" />
              <span class="form-slider-val" id="cfg-embed-pad-val">${cfg.padding !== undefined ? cfg.padding : 8}px</span>
            </div>
          </div>
        `;

        // [Sandbox] Tab
        document.getElementById('tab-pane-sandbox').innerHTML = `
          <div class="form-group">
            <label>Script Execution Rules</label>
            ${renderCustomDropdownHtml('embed-scripts', 'Scripts', cfg.allowScripts !== false ? 'allow' : 'deny', [
              { val: 'allow', label: 'Allow JavaScript (sandbox="allow-scripts")', hint: 'Dynamic' },
              { val: 'deny', label: 'Strict Static Mode (No Scripts)', hint: 'Safe' }
            ])}
          </div>
          <div style="font-size:11.5px;color:var(--fg-muted);line-height:1.5;margin-top:8px;">
            Sandboxed execution isolates payload memory while allowing interactive components.
          </div>
        `;
      } else if (w.type === 'l.weather') {
        // [Location] Tab
        const cities = ['Moscow', 'London', 'New York', 'Tokyo', 'Berlin', 'Paris'];
        const isKnownCity = cities.includes(cfg.city);
        document.getElementById('tab-pane-location').innerHTML = `
          <div class="form-group">
            <label>City Quick Picker</label>
            ${renderCustomDropdownHtml('weather-city', 'City', isKnownCity ? cfg.city : 'Custom', [
              { val: 'Moscow', label: 'Moscow', hint: '55.75, 37.61' },
              { val: 'London', label: 'London', hint: '51.50, -0.12' },
              { val: 'New York', label: 'New York', hint: '40.71, -74.00' },
              { val: 'Tokyo', label: 'Tokyo', hint: '35.67, 139.65' },
              { val: 'Berlin', label: 'Berlin', hint: '52.52, 13.40' },
              { val: 'Paris', label: 'Paris', hint: '48.85, 2.35' },
              { val: 'Custom', label: 'Custom Coordinates', hint: 'Manual' }
            ])}
          </div>
          <div class="form-group">
            <label>Latitude</label>
            <input type="number" step="0.0001" id="cfg-weather-lat" class="form-control" value="${cfg.lat || 55.7558}" />
          </div>
          <div class="form-group">
            <label>Longitude</label>
            <input type="number" step="0.0001" id="cfg-weather-lon" class="form-control" value="${cfg.lon || 37.6173}" />
          </div>
        `;

        window.onDropdownChanged_weather_city = function(val) {
          const coords = {
            'Moscow': { lat: 55.7558, lon: 37.6173 },
            'London': { lat: 51.5074, lon: -0.1278 },
            'New York': { lat: 40.7128, lon: -74.0060 },
            'Tokyo': { lat: 35.6762, lon: 139.6503 },
            'Berlin': { lat: 52.5200, lon: 13.4050 },
            'Paris': { lat: 48.8566, lon: 2.3522 }
          };
          if (coords[val]) {
            document.getElementById('cfg-weather-lat').value = coords[val].lat;
            document.getElementById('cfg-weather-lon').value = coords[val].lon;
          }
        };

        // [Display] Tab
        document.getElementById('tab-pane-display').innerHTML = `
          <div class="form-group">
            <label>View Mode</label>
            ${renderCustomDropdownHtml('weather-view', 'View', cfg.viewMode === 'compact' ? 'compact' : 'badge', [
              { val: 'badge', label: 'Expanded Card', hint: 'Detailed' },
              { val: 'compact', label: 'Compact Badge', hint: 'Minimal' }
            ])}
          </div>
          <div class="form-group">
            <label>Weather Glyphs</label>
            ${renderCustomDropdownHtml('weather-icons', 'Glyphs', cfg.showIcons !== false ? 'show' : 'hide', [
              { val: 'show', label: 'Show Weather Icons', hint: 'Glyphs' },
              { val: 'hide', label: 'Hide Icons (Text Only)', hint: 'Text' }
            ])}
          </div>
        `;

        // [Units] Tab
        document.getElementById('tab-pane-units').innerHTML = `
          <div class="form-group">
            <label>Temperature Units</label>
            ${renderCustomDropdownHtml('weather-units', 'Temperature', cfg.units === 'imperial' ? 'imperial' : 'metric', [
              { val: 'metric', label: 'Celsius (°C)', hint: 'Metric' },
              { val: 'imperial', label: 'Fahrenheit (°F)', hint: 'Imperial' }
            ])}
          </div>
          <div class="form-group">
            <label>Wind Speed Units</label>
            ${renderCustomDropdownHtml('weather-wind', 'Wind', cfg.windUnits || 'kmh', [
              { val: 'kmh', label: 'Kilometers per hour (km/h)', hint: 'Default' },
              { val: 'ms', label: 'Meters per second (m/s)', hint: 'Metric' },
              { val: 'mph', label: 'Miles per hour (mph)', hint: 'Imperial' }
            ])}
          </div>
        `;
      } else if (w.type === 'l.clock') {
        // [Format] Tab
        document.getElementById('tab-pane-format').innerHTML = `
          <div class="form-group">
            <label>Time Notation</label>
            ${renderCustomDropdownHtml('clock-timefmt', 'Notation', cfg.timeFormat === '12h' ? '12h' : '24h', [
              { val: '24h', label: '24-Hour Military (14:30)', hint: '24h' },
              { val: '12h', label: '12-Hour AM/PM (2:30 PM)', hint: '12h' }
            ])}
          </div>
          <div class="form-group">
            <label>Seconds Readout</label>
            ${renderCustomDropdownHtml('clock-sec', 'Seconds', cfg.showSeconds !== false ? 'show' : 'hide', [
              { val: 'show', label: 'Show Seconds Ticker', hint: ':00' },
              { val: 'hide', label: 'Hide Seconds', hint: 'Clean' }
            ])}
          </div>
          <div class="form-group">
            <label>Date Readout</label>
            ${renderCustomDropdownHtml('clock-datefmt', 'Date', cfg.dateFormat || 'short', [
              { val: 'short', label: 'Short Date (Wed, Sep 10)', hint: 'Abbr' },
              { val: 'long', label: 'Full Date (Wednesday, September 10)', hint: 'Full' },
              { val: 'hide', label: 'Hide Date', hint: 'Off' }
            ])}
          </div>
        `;

        // [Typography] Tab
        document.getElementById('tab-pane-typography').innerHTML = `
          <div class="form-group">
            <label>Font Family</label>
            ${renderCustomDropdownHtml('clock-font', 'Font', cfg.fontFamily === 'sans' ? 'sans' : 'mono', [
              { val: 'mono', label: 'Tabular Monospace', hint: 'Monospace' },
              { val: 'sans', label: 'Modern Sans-Serif', hint: 'Inter/System' }
            ])}
          </div>
          <div class="form-group">
            <label>Font Size</label>
            ${renderCustomDropdownHtml('clock-size', 'Size', cfg.fontSize || 'standard', [
              { val: 'standard', label: 'Standard Readout (38px)', hint: 'Normal' },
              { val: 'large', label: 'Large Readout (46px)', hint: 'Hero' },
              { val: 'compact', label: 'Compact Readout (28px)', hint: 'Small' }
            ])}
          </div>
        `;
      } else if (w.type === 'l.notes') {
        // [Theme & Style] Tab
        document.getElementById('tab-pane-theme').innerHTML = `
          <div class="form-group">
            <label>Window Style Theme</label>
            ${renderCustomDropdownHtml('notes-theme', 'Style', cfg.theme || 'macos', [
              { val: 'macos', label: 'macOS Window (Traffic Lights)', hint: 'Classic' },
              { val: 'cyber', label: 'Cyber Terminal (HUD Neon)', hint: 'Hacker' },
              { val: 'parchment', label: 'Amber Sticky Memo', hint: 'Warm' },
              { val: 'glass', label: 'Obsidian Frosted Glass', hint: 'Acrylic' }
            ])}
          </div>
          <div class="form-group">
            <label>Window Title</label>
            <input type="text" id="cfg-notes-title" class="form-control" value="${escapeAttr(cfg.title || 'Notes')}" placeholder="e.g. Notes, Todo, Scratchpad" />
          </div>
        `;

        // [Markdown & Editor] Tab
        document.getElementById('tab-pane-editor').innerHTML = `
          <div class="form-group">
            <label>Markdown Formatting Support</label>
            <div style="display:flex;align-items:center;gap:10px;margin-top:4px;">
              <input type="checkbox" id="cfg-notes-md" ${cfg.markdown ? 'checked' : ''} style="width:16px;height:16px;accent-color:var(--accent);cursor:pointer;" />
              <label for="cfg-notes-md" style="cursor:pointer;font-size:12.5px;color:var(--fg-primary);margin:0;font-weight:500;">Enable Markdown features in notes</label>
            </div>
            <div style="font-size:11.5px;color:var(--fg-muted);line-height:1.4;margin-top:6px;">
              Supports # Headers, **bold**, *italic*, lists, checkboxes (- [ ]), and code blocks.
            </div>
          </div>
          <div class="form-group" style="margin-top:16px;">
            <label>Editor Font Size</label>
            ${renderCustomDropdownHtml('notes-fontsize', 'Font Size', cfg.fontSize || '13px', [
              { val: '12px', label: 'Compact (12px)', hint: 'Small' },
              { val: '13px', label: 'Standard (13px)', hint: 'Default' },
              { val: '15px', label: 'Large (15px)', hint: 'Comfortable' }
            ])}
          </div>
          <div class="form-group">
            <label>Word Wrapping</label>
            ${renderCustomDropdownHtml('notes-wrap', 'Wrapping', cfg.wrap === 'nowrap' ? 'nowrap' : 'wrap', [
              { val: 'wrap', label: 'Wrap Lines Naturally', hint: 'Standard' },
              { val: 'nowrap', label: 'No Wrap (Horizontal Scroll)', hint: 'Code' }
            ])}
          </div>
        `;

        // [Info] Tab
        const curText = cfg.text || '';
        const charCount = curText.length;
        const lineCount = curText ? curText.split('\n').length : 0;
        const wordCount = curText.trim() ? curText.trim().split(/\s+/).length : 0;
        document.getElementById('tab-pane-info').innerHTML = `
          <div class="form-group">
            <label>Note Statistics</label>
            <div style="background:var(--bg-subtle);border:1px solid var(--border);border-radius:8px;padding:12px;display:flex;justify-content:space-around;text-align:center;">
              <div>
                <div style="font-size:20px;font-weight:700;color:var(--accent);">${charCount}</div>
                <div style="font-size:11px;color:var(--fg-muted);margin-top:2px;">Characters</div>
              </div>
              <div>
                <div style="font-size:20px;font-weight:700;color:var(--accent);">${wordCount}</div>
                <div style="font-size:11px;color:var(--fg-muted);margin-top:2px;">Words</div>
              </div>
              <div>
                <div style="font-size:20px;font-weight:700;color:var(--accent);">${lineCount}</div>
                <div style="font-size:11px;color:var(--fg-muted);margin-top:2px;">Lines</div>
              </div>
            </div>
          </div>
          <div style="font-size:11.5px;color:var(--fg-muted);line-height:1.5;margin-top:12px;">
            Notes are saved instantly to local memory in real time as you type.
          </div>
        `;
      } else if (w.type === 'l.media') {
        // [Display] Tab
        document.getElementById('tab-pane-display').innerHTML = `
          <div class="form-group">
            <label>Waveform Equalizer</label>
            ${renderCustomDropdownHtml('media-wave', 'Visualizer', cfg.waveform !== false ? 'show' : 'hide', [
              { val: 'show', label: 'Animated Sound Waves', hint: 'Equalizer' },
              { val: 'hide', label: 'Hidden (Clean)', hint: 'Minimal' }
            ])}
          </div>
          <div class="form-group">
            <label>Artist Marquee Ticker</label>
            ${renderCustomDropdownHtml('media-marquee', 'Ticker', cfg.marquee !== false ? 'scroll' : 'static', [
              { val: 'scroll', label: 'Ping-Pong Ticker (Auto-scroll)', hint: 'Dynamic' },
              { val: 'static', label: 'Static Ellipsis Truncate', hint: 'Fixed' }
            ])}
          </div>
          <div class="form-group">
            <label>Album Cover Art</label>
            ${renderCustomDropdownHtml('media-art', 'Artwork', cfg.showArt !== false ? 'show' : 'hide', [
              { val: 'show', label: 'Show Cover Thumbnail', hint: 'Default' },
              { val: 'hide', label: 'Hide Artwork (Text Only)', hint: 'Compact' }
            ])}
          </div>
        `;

        // [Audio Source] Tab
        document.getElementById('tab-pane-source').innerHTML = `
          <div class="form-group">
            <label>Audio Integration Backend</label>
            ${renderCustomDropdownHtml('media-backend', 'Backend', 'mpris', [
              { val: 'mpris', label: 'Linux MPRIS D-Bus (Spotify, VLC, Browsers)', hint: 'Native' }
            ])}
          </div>
          <div style="font-size:11.5px;color:var(--fg-muted);line-height:1.5;margin-top:12px;">
            The browser continuously polls the system D-Bus session bus for active players implementing org.mpris.MediaPlayer2. When no media is playing, the widget automatically switches to an inactive idle state.
          </div>
        `;

        // [Info] Tab
        document.getElementById('tab-pane-info').innerHTML = `
          <div class="form-group">
            <label>Now Playing Diagnostics</label>
            <div style="background:var(--bg-subtle);border:1px solid var(--border);border-radius:8px;padding:12px;font-size:12px;line-height:1.6;color:var(--fg-muted);">
              <div><strong style="color:var(--fg-primary);">Active Source:</strong> ${globalMediaState.hasPlayer ? 'System MPRIS Player' : 'None (Idle)'}</div>
              <div><strong style="color:var(--fg-primary);">Status:</strong> ${globalMediaState.hasPlayer ? (globalMediaState.isPlaying ? 'Playing' : 'Paused') : 'Stopped'}</div>
              <div><strong style="color:var(--fg-primary);">Track:</strong> ${escapeHtml(globalMediaState.title)}</div>
              <div><strong style="color:var(--fg-primary);">Artist:</strong> ${escapeHtml(globalMediaState.artist || '-')}</div>
              <div><strong style="color:var(--fg-primary);">Duration:</strong> ${formatMediaTime(globalMediaState.duration)}</div>
            </div>
          </div>
        `;
      }
    }

    function saveSettingsModal() {
      const widgetId = appState.activeModalWidgetId;
      const w = appState.widgets.find(x => x.id === widgetId);
      if (!w) {
        closeSettingsModal();
        return;
      }

      if (!w.config) w.config = {};

      if (w.type === 'l.embed') {
        const txtEl = document.getElementById('cfg-embed-html');
        if (txtEl) w.config.html = txtEl.value;

        const dim = getCustomDropdownValue('embed-dim');
        if (dim === 'compact') { w.width = '260px'; w.height = '100px'; }
        else if (dim === 'wide') { w.width = '420px'; w.height = '160px'; }
        else if (dim === 'full') { w.width = '100%'; w.height = '180px'; }
        else if (dim === 'default') { w.width = '320px'; w.height = '140px'; }

        w.config.transparent = getCustomDropdownValue('embed-trans') === 'trans';
        w.config.border = getCustomDropdownValue('embed-border') === 'none' ? 'none' : 'subtle';
        const padEl = document.getElementById('cfg-embed-pad');
        if (padEl) w.config.padding = parseInt(padEl.value, 10);
        w.config.allowScripts = getCustomDropdownValue('embed-scripts') === 'allow';

      } else if (w.type === 'l.weather') {
        const cityVal = getCustomDropdownValue('weather-city');
        const latVal = parseFloat(document.getElementById('cfg-weather-lat').value) || 55.7558;
        const lonVal = parseFloat(document.getElementById('cfg-weather-lon').value) || 37.6173;

        w.config.city = cityVal || 'Local';
        w.config.lat = latVal;
        w.config.lon = lonVal;
        w.config.viewMode = getCustomDropdownValue('weather-view') || 'badge';
        w.config.showIcons = getCustomDropdownValue('weather-icons') !== 'hide';
        w.config.units = getCustomDropdownValue('weather-units') || 'metric';
        w.config.windUnits = getCustomDropdownValue('weather-wind') || 'kmh';

      } else if (w.type === 'l.clock') {
        w.config.timeFormat = getCustomDropdownValue('clock-timefmt') || '24h';
        w.config.showSeconds = getCustomDropdownValue('clock-sec') !== 'hide';
        w.config.dateFormat = getCustomDropdownValue('clock-datefmt') || 'short';
        w.config.fontFamily = getCustomDropdownValue('clock-font') || 'mono';
        w.config.fontSize = getCustomDropdownValue('clock-size') || 'standard';

      } else if (w.type === 'l.notes') {
        w.config.theme = getCustomDropdownValue('notes-theme') || 'macos';
        const titleEl = document.getElementById('cfg-notes-title');
        if (titleEl) w.config.title = titleEl.value.trim() || 'Notes';
        const mdEl = document.getElementById('cfg-notes-md');
        w.config.markdown = mdEl ? mdEl.checked : false;
        w.config.fontSize = getCustomDropdownValue('notes-fontsize') || '13px';
        w.config.wrap = getCustomDropdownValue('notes-wrap') || 'wrap';
      } else if (w.type === 'l.media') {
        w.config.waveform = getCustomDropdownValue('media-wave') !== 'hide';
        w.config.marquee = getCustomDropdownValue('media-marquee') !== 'static';
        w.config.showArt = getCustomDropdownValue('media-art') !== 'hide';
      }

      saveState();
      renderWidgets();
      updateClocks();
      closeSettingsModal();
    }

    // ──────────────── Drag & Drop Zones ────────────────
    function setupDragAndDrop() {
      const MAX_ZONE_WIDGETS = 5;
      document.querySelectorAll('.drop-zone').forEach(zone => {
        zone.addEventListener('dragover', (e) => {
          if (!appState.editMode) return;
          e.preventDefault();

          const type = activeTrayType || 'l.clock';
          const w = type === 'l.media' ? 336 : (type === 'l.embed' ? 360 : (type === 'l.notes' ? 280 : (type === 'l.weather' ? 260 : 220)));
          const h = type === 'l.media' ? 144 : (type === 'l.embed' ? 140 : (type === 'l.notes' ? 180 : 110));

          const zoneRect = zone.getBoundingClientRect();
          let dropX = snapToGrid(e.clientX - zoneRect.left - (w / 2));
          let dropY = snapToGrid(e.clientY - zoneRect.top - (h / 2));
          dropX = Math.max(8, Math.min(dropX, zone.clientWidth - w - 8));
          dropY = Math.max(8, Math.min(dropY, zone.clientHeight - h - 8));

          const count = getZoneWidgetCount(zone.id);
          const collides = getZoneOverlaps(zone.id, dropX, dropY, w, h);
          if (count >= MAX_ZONE_WIDGETS || collides) {
            zone.classList.remove('drag-over');
            zone.classList.add('drag-error');
            showDropPreview(zone, dropX, dropY, w, h, true);
          } else {
            zone.classList.remove('drag-error');
            zone.classList.add('drag-over');
            showDropPreview(zone, dropX, dropY, w, h, false);
          }
        });

        zone.addEventListener('dragleave', (e) => {
          if (!zone.contains(e.relatedTarget)) {
            zone.classList.remove('drag-over', 'drag-error');
            removeDropPreview();
          }
        });

        zone.addEventListener('drop', (e) => {
          if (!appState.editMode) return;
          e.preventDefault();
          zone.classList.remove('drag-over', 'drag-error');
          removeDropPreview();

          const widgetType = e.dataTransfer.getData('widget_type') || activeTrayType;
          const targetZoneId = zone.id;

          if (getZoneWidgetCount(targetZoneId) >= MAX_ZONE_WIDGETS) {
            flashZoneFull(zone);
            return;
          }

          if (widgetType) {
            const type = widgetType;
            const w = type === 'l.media' ? 336 : (type === 'l.embed' ? 360 : (type === 'l.notes' ? 280 : (type === 'l.weather' ? 260 : 220)));
            const h = type === 'l.media' ? 144 : (type === 'l.embed' ? 140 : (type === 'l.notes' ? 180 : 110));
            const zoneRect = zone.getBoundingClientRect();
            let dropX = snapToGrid(e.clientX - zoneRect.left - (w / 2));
            let dropY = snapToGrid(e.clientY - zoneRect.top - (h / 2));
            dropX = Math.max(8, Math.min(dropX, zone.clientWidth - w - 8));
            dropY = Math.max(8, Math.min(dropY, zone.clientHeight - h - 8));
            addWidget(widgetType, targetZoneId, dropX, dropY);
          }
          activeTrayType = null;
        });
      });

      document.addEventListener('dragend', () => {
        activeTrayType = null;
        removeDropPreview();
        document.querySelectorAll('.drop-zone').forEach(z => z.classList.remove('drag-over', 'drag-error'));
      });
    }

    // ──────────────── Helpers ────────────────
    function escapeHtml(str) {
      if (!str) return '';
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
    }

    function escapeAttr(str) {
      if (!str) return '';
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
    }

    // ──────────────── Init ────────────────
    window.addEventListener('DOMContentLoaded', () => {
      loadState();
      renderWidgets();
      setupDragAndDrop();
      updateClocks();
      sendMediaCommand('poll');

      const searchInput = document.getElementById('search-input');
      const searchHistory = document.getElementById('search-history');

      searchInput.addEventListener('focus', () => {
        renderSearchHistory();
        if (loadSearchHistory().length > 0) searchHistory.classList.add('open');
      });

      document.addEventListener('click', (e) => {
        if (!e.target.closest('.search-container')) {
          searchHistory.classList.remove('open');
        }
      });
    });
  </script>
</body>
</html>
)html";

inline std::string getNewTabHtml(const Theme::Palette& pal) {
    std::string html = NEW_TAB_HTML;
    std::string rootVars =
        "    --bg-base: " + pal.bgBase.toCssRgba() + ";\n"
        "    --bg-surface: " + pal.bgSurface.toCssRgba() + ";\n"
        "    --bg-subtle: " + pal.bgSubtle.toCssRgba() + ";\n"
        "    --bg-active: " + pal.bgActive.toCssRgba() + ";\n"
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
    return html;
}

} // namespace Blueprint::Engine
