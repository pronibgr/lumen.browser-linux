#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "ui/omnibox.hpp"
#include "theme/colors.hpp"
#include "core/config.hpp"
#include "omnibox/calc_parser.hpp"
#include "omnibox/converter.hpp"
#include "storage/database.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Blueprint::UI {

namespace {

void sc(cairo_t* cr, const Theme::Color& c, float a = 1.f) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * a);
}
void rr(cairo_t* cr, double x, double y, double w, double h, double r) {
    r = std::min(r, std::min(w,h)/2.0);
    cairo_new_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,      M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2, M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,   3*M_PI/2);
    cairo_close_path(cr);
}
std::string cleanUtf8(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20 || c == '\t' || c >= 0x80) o += s[i];
    }
    return o;
}

} // anonymous namespace

OmniboxWidget::OmniboxWidget() {}

void OmniboxWidget::selectAll() {
    m_selStart = 0;
    m_selEnd   = static_cast<int>(m_text.length());
    m_cursorPos = m_selEnd;
}

std::string OmniboxWidget::getSelectedText() const {
    if (!hasSelection()) return "";
    int mn = getSelMin(), mx = getSelMax();
    return m_text.substr(mn, mx - mn);
}

void OmniboxWidget::deleteSelection() {
    if (!hasSelection()) return;
    int mn = getSelMin(), mx = getSelMax();
    m_text.erase(mn, mx - mn);
    m_cursorPos = mn;
    clearSelection();
}

int OmniboxWidget::xToCharIndex(double mouseX) {
    if (m_text.empty()) return 0;
    double textX = m_lastX + 32;
    double relX = mouseX - textX;
    if (relX <= 0) return 0;

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* cr = cairo_create(surf);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter 10");
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    std::string dt = cleanUtf8(m_text);
    pango_layout_set_text(layout, dt.c_str(), -1);

    int index = 0, trailing = 0;
    pango_layout_xy_to_index(layout, static_cast<int>(relX * PANGO_SCALE), 10 * PANGO_SCALE, &index, &trailing);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    return std::clamp(index + trailing, 0, static_cast<int>(m_text.length()));
}

void OmniboxWidget::setText(const std::string& url) {
    m_displayUrl = url;
    // only update text when unfocused so we dont overwrite what user is typing
    if (!m_focused) {
        // show clean url in edit field
        bool isBlank = (url.empty() || url == "about:blank");
        m_text = isBlank ? "" : url;
        m_cursorPos = static_cast<int>(m_text.length());
        clearSelection();
    }
}

void OmniboxWidget::setFocused(bool focused) {
    if (m_focused == focused) return;
    m_focused = focused;
    if (m_focused) {
        // on focus: pre-fill with current url and select everything
        bool isBlank = (m_displayUrl.empty() || m_displayUrl == "about:blank");
        m_text = isBlank ? "" : m_displayUrl;
        m_cursorPos = static_cast<int>(m_text.length());
        selectAll();
        m_selectedSuggestion = -1;
        m_hoveredSuggestion  = -1;
        updateSuggestions();
        m_showPopup = true;
    } else {
        m_showPopup = false;
        m_selectedSuggestion = -1;
        m_hoveredSuggestion  = -1;
        clearSelection();
        m_isSelecting = false;
    }
}

void OmniboxWidget::updateSuggestions() {
    m_suggestions.clear();
    m_selectedSuggestion = -1;

    if (m_text.empty() || m_text == "lumen://newtab" || m_text == "lampa://newtab" || m_text == "blueprint://newtab") {
        auto recent = Storage::Database::instance().getRecentHistory(6);
        for (const auto& h : recent)
            m_suggestions.push_back({h.title, h.url, "history", 0});
        return;
    }

    // quick math solver
    auto calc = Omnibox::Calculator::evaluate(m_text);
    if (calc.has_value()) {
        std::string s = "= " + Omnibox::Calculator::formatResult(calc.value());
        m_suggestions.push_back({s, m_text, "calc", 2000});
    }
    // unit & currency converter
    auto conv = Omnibox::UnitConverter::convert(m_text);
    if (conv.has_value()) {
        Omnibox::SearchItem item{conv->formatted, m_text, "conv", 1900};
        item.isCurrency = conv->isCurrency;
        item.isLive = conv->isLive;
        m_suggestions.push_back(item);
    }

    // local history lookup
    auto hist = Storage::Database::instance().searchHistory(m_text, 5);
    for (const auto& h : hist)
        m_suggestions.push_back(h);

    // url or search fallback
    bool looksUrl = (m_text.find('.') != std::string::npos && m_text.find(' ') == std::string::npos)
                 || m_text.find("http") == 0 || m_text.find("localhost") == 0
                 || m_text.find("lumen://") == 0 || m_text.find("lampa://") == 0 || m_text.find("://") != std::string::npos
                 || m_text.find("about:") == 0;
    if (looksUrl) {
        std::string full = (m_text.find("://") == std::string::npos && m_text.find("about:") != 0) ? "https://" + m_text : m_text;
        m_suggestions.push_back({"Go to: " + full, full, "url", 500});
    } else if (!calc.has_value() && !conv.has_value()) {
        std::string searchUrl = Core::BrowserConfig::formatSearchUrl(m_searchTemplate, m_text);
        m_suggestions.push_back({"Search: " + m_text, searchUrl, "web", 100});
    }
}

void OmniboxWidget::draw(cairo_t* cr, double x, double y, double w, double h) {
    m_lastX = x; m_lastY = y; m_lastW = w; m_lastH = h;

    // background box
    rr(cr, x, y, w, h, 6);
    sc(cr, m_focused ? Theme::BG_ACTIVE : Theme::BG_SUBTLE);
    cairo_fill_preserve(cr);

    // border (stroke only, no dangling paths)
    if (m_focused) { sc(cr, Theme::ACCENT_CALM, 0.5f); cairo_set_line_width(cr, 1.5); }
    else           { sc(cr, Theme::BORDER_SOFT, 0.6f); cairo_set_line_width(cr, 1.0); }
    cairo_stroke(cr);

    // lock icon drawn separately in topbar row 2

    // decide what text to show
    std::string rawText;
    bool isPlaceholder = false;
    if (m_focused) {
        rawText = m_text; // always show raw editable text when focused
    } else {
        bool isBlank = (m_displayUrl.empty() || m_displayUrl == "about:blank");
        if (isBlank) {
            rawText = "Search or enter URL...";
            isPlaceholder = true;
        } else {
            rawText = m_displayUrl;
        }
    }
    std::string displayText = cleanUtf8(rawText);

    // Text layout
    double textX = x + 32;
    double textW = w - 44;

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* fd = pango_font_description_from_string("Inter 10");
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_width(layout, static_cast<int>(textW * PANGO_SCALE));
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    pango_layout_set_text(layout, displayText.c_str(), -1);

    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);
    double textY = y + (h - th) / 2.0;

    // Selection highlight
    if (m_focused && hasSelection()) {
        int sMin = std::clamp(getSelMin(), 0, static_cast<int>(displayText.length()));
        int sMax = std::clamp(getSelMax(), 0, static_cast<int>(displayText.length()));
        PangoRectangle rMin, rMax;
        pango_layout_index_to_pos(layout, sMin, &rMin);
        pango_layout_index_to_pos(layout, sMax, &rMax);
        double sx1 = textX + rMin.x / static_cast<double>(PANGO_SCALE);
        double sx2 = textX + rMax.x / static_cast<double>(PANGO_SCALE);
        if (sx1 > sx2) std::swap(sx1, sx2);
        double selW = std::max(2.0, sx2 - sx1);
        double selH = th + 4.0;
        double selY = textY - 2.0;
        rr(cr, sx1, selY, selW, selH, 3);
        sc(cr, Theme::ACCENT_CALM, 0.40f);
        cairo_fill(cr);
    }

    if (isPlaceholder) sc(cr, Theme::TEXT_MUTED, 0.5f);
    else               sc(cr, Theme::TEXT_MAIN);

    cairo_new_path(cr);
    cairo_move_to(cr, textX, textY);
    pango_cairo_show_layout(cr, layout);

    // Caret
    if (m_focused && !hasSelection()) {
        int byteIdx = std::clamp(m_cursorPos, 0, static_cast<int>(displayText.length()));
        PangoRectangle rect;
        pango_layout_index_to_pos(layout, byteIdx, &rect);
        double cx = textX + rect.x / static_cast<double>(PANGO_SCALE);
        sc(cr, Theme::ACCENT_CALM);
        cairo_set_line_width(cr, 1.5);
        cairo_new_path(cr);
        cairo_move_to(cr, cx, y + 5);
        cairo_line_to(cr, cx, y + h - 5);
        cairo_stroke(cr);
    }

    g_object_unref(layout);
}

void OmniboxWidget::drawPopup(cairo_t* cr, double x, double y, double w) {
    if (!m_focused || !m_showPopup || m_suggestions.empty()) return;

    size_t maxItems = std::min(m_suggestions.size(), size_t(7));
    constexpr double itemH = 34.0;
    double popupH = static_cast<double>(maxItems) * itemH + 8.0;
    m_popupY = y; m_popupH = popupH;

    // popup shadow
    rr(cr, x + 3, y + 3, w, popupH, 8);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_fill(cr);

    // panel card
    rr(cr, x, y, w, popupH, 8);
    sc(cr, Theme::BG_POPUP);
    cairo_fill_preserve(cr);
    sc(cr, Theme::BORDER_SOFT, 0.6f);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    PangoLayout* lText = pango_cairo_create_layout(cr);
    PangoFontDescription* fdText = pango_font_description_from_string("Inter 10");
    pango_layout_set_font_description(lText, fdText);
    pango_font_description_free(fdText);
    pango_layout_set_width(lText, static_cast<int>((w - 56) * PANGO_SCALE));
    pango_layout_set_ellipsize(lText, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(lText, TRUE);

    double curY = y + 4.0;
    for (size_t i = 0; i < maxItems; ++i) {
        const auto& item = m_suggestions[i];
        bool sel = (static_cast<int>(i) == m_selectedSuggestion ||
                    static_cast<int>(i) == m_hoveredSuggestion);

        if (sel) {
            rr(cr, x + 4, curY, w - 8, itemH, 5);
            sc(cr, Theme::BG_ACTIVE);
            cairo_fill(cr);
        }

        // draw crisp vector icons for suggestions
        double ix = x + 24.0;
        double iy = curY + itemH / 2.0;

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        if (item.category == "web") {
            // search glass icon
            sc(cr, sel ? Theme::ACCENT_CALM : Theme::TEXT_MUTED, 0.9f);
            cairo_set_line_width(cr, 1.4);
            cairo_new_path(cr);
            cairo_arc(cr, ix - 1.5, iy - 1.5, 4.5, 0, 2 * M_PI);
            cairo_stroke(cr);
            cairo_move_to(cr, ix + 1.8, iy + 1.8);
            cairo_line_to(cr, ix + 5.5, iy + 5.5);
            cairo_stroke(cr);
        } else if (item.category == "url") {
            // world globe for web links
            sc(cr, sel ? Theme::ACCENT_CALM : Theme::TEXT_MUTED, 0.9f);
            cairo_set_line_width(cr, 1.3);
            cairo_new_path(cr);
            cairo_arc(cr, ix, iy, 5.5, 0, 2 * M_PI);
            cairo_stroke(cr);
            // equator
            cairo_move_to(cr, ix - 5.5, iy);
            cairo_line_to(cr, ix + 5.5, iy);
            cairo_stroke(cr);
            // meridians
            cairo_move_to(cr, ix, iy - 5.5);
            cairo_curve_to(cr, ix - 2.8, iy - 2.0, ix - 2.8, iy + 2.0, ix, iy + 5.5);
            cairo_stroke(cr);
            cairo_move_to(cr, ix, iy - 5.5);
            cairo_curve_to(cr, ix + 2.8, iy - 2.0, ix + 2.8, iy + 2.0, ix, iy + 5.5);
            cairo_stroke(cr);
        } else if (item.category == "history") {
            // clock face for history items
            sc(cr, sel ? Theme::ACCENT_CALM : Theme::TEXT_MUTED, 0.9f);
            cairo_set_line_width(cr, 1.3);
            cairo_new_path(cr);
            cairo_arc(cr, ix, iy, 5.5, 0, 2 * M_PI);
            cairo_stroke(cr);
            // clock hands
            cairo_move_to(cr, ix, iy - 3.2);
            cairo_line_to(cr, ix, iy);
            cairo_line_to(cr, ix + 2.6, iy);
            cairo_stroke(cr);
        } else {
            // dollar badge for calculations and currency results
            sc(cr, sel ? Theme::ACCENT_CALM : Theme::TEXT_MUTED, 0.95f);
            cairo_set_line_width(cr, 1.4);
            cairo_new_path(cr);
            // S curves of dollar sign
            cairo_arc(cr, ix, iy - 2.2, 2.5, M_PI * 0.75, M_PI * 2.0);
            cairo_arc_negative(cr, ix, iy + 2.2, 2.5, M_PI * 1.0, -M_PI * 0.25);
            cairo_stroke(cr);
            // Vertical stroke through $
            cairo_move_to(cr, ix, iy - 5.5);
            cairo_line_to(cr, ix, iy + 5.5);
            cairo_stroke(cr);
        }

        // Title text
        std::string t = cleanUtf8(item.title);
        pango_layout_set_text(lText, t.c_str(), -1);
        int tw = 0, th = 0;
        pango_layout_get_pixel_size(lText, &tw, &th);
        sc(cr, sel ? Theme::TEXT_MAIN : Theme::TEXT_MUTED);
        cairo_new_path(cr);
        double textDrawX = x + 44.0;
        double textDrawY = curY + (itemH - th) / 2.0;
        cairo_move_to(cr, textDrawX, textDrawY);
        pango_cairo_show_layout(cr, lText);

        // currency indicator: crossed-out globe if offline, vibrant emerald badge if live
        if (item.isCurrency) {
            double gx = textDrawX + tw + 14.0;
            double gy = curY + itemH / 2.0;

            if (!item.isLive) {
                // globe outline & meridians (muted warning tone)
                sc(cr, Theme::TEXT_MUTED, 0.75f);
                cairo_set_line_width(cr, 1.2);
                cairo_new_path(cr);
                cairo_arc(cr, gx, gy, 5.2, 0, 2 * M_PI);
                cairo_stroke(cr);
                // equator
                cairo_move_to(cr, gx - 5.2, gy);
                cairo_line_to(cr, gx + 5.2, gy);
                cairo_stroke(cr);
                // longitude curves
                cairo_move_to(cr, gx, gy - 5.2);
                cairo_curve_to(cr, gx - 2.5, gy - 1.8, gx - 2.5, gy + 1.8, gx, gy + 5.2);
                cairo_stroke(cr);
                cairo_move_to(cr, gx, gy - 5.2);
                cairo_curve_to(cr, gx + 2.5, gy - 1.8, gx + 2.5, gy + 1.8, gx, gy + 5.2);
                cairo_stroke(cr);

                // clean diagonal cross-out line through the globe
                Theme::Color strikeCol{0.92f, 0.40f, 0.35f, 0.9f}; // soft coral/red
                sc(cr, strikeCol);
                cairo_set_line_width(cr, 1.5);
                cairo_new_path(cr);
                cairo_move_to(cr, gx - 5.8, gy - 5.8);
                cairo_line_to(cr, gx + 5.8, gy + 5.8);
                cairo_stroke(cr);
            } else {
                // live rate indicator: emerald green dot with subtle glow aura
                Theme::Color liveCol{0.20f, 0.85f, 0.50f, 0.95f};
                sc(cr, liveCol);
                cairo_new_path(cr);
                cairo_arc(cr, gx, gy, 3.2, 0, 2 * M_PI);
                cairo_fill(cr);

                sc(cr, liveCol, 0.28f);
                cairo_set_line_width(cr, 1.4);
                cairo_new_path(cr);
                cairo_arc(cr, gx, gy, 5.4, 0, 2 * M_PI);
                cairo_stroke(cr);
            }
        }

        curY += itemH;
    }

    g_object_unref(lText);
}

void OmniboxWidget::executeSelection() {
    std::string target;
    if (m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_suggestions.size())) {
        target = m_suggestions[m_selectedSuggestion].url;
    } else {
        target = m_text;
    }
    setFocused(false);
    if (!target.empty() && m_onNavigate) {
        if (target.find("://") == std::string::npos && target.find("about:") != 0) {
            if (target.find('.') != std::string::npos && target.find(' ') == std::string::npos)
                target = "https://" + target;
            else
                target = Core::BrowserConfig::formatSearchUrl(m_searchTemplate, target);
        }
        m_onNavigate(target);
    }
}

bool OmniboxWidget::handleKeyPress(uint32_t sym, uint32_t mod, const char* textInput) {
    if (!m_focused) return false;

    bool isCtrl = (mod & 4) != 0 || (mod & 0x40) != 0 || (SDL_GetModState() & KMOD_CTRL);

    // enter to navigate or run search
    if (sym == 0xff0d || sym == 0xff8d || sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == 13) {
        executeSelection();
        return true;
    }
    // escape to dismiss
    if (sym == 0xff1b || sym == SDLK_ESCAPE || sym == 27) {
        setFocused(false);
        return true;
    }
    // down arrow through suggestions
    if ((sym == 0xff54 || sym == 0xff99 || sym == SDLK_DOWN) && !m_suggestions.empty()) {
        m_selectedSuggestion = (m_selectedSuggestion + 1) % static_cast<int>(m_suggestions.size());
        return true;
    }
    // up arrow
    if ((sym == 0xff52 || sym == 0xff97 || sym == SDLK_UP) && !m_suggestions.empty()) {
        m_selectedSuggestion = (m_selectedSuggestion - 1 + static_cast<int>(m_suggestions.size()))
                                % static_cast<int>(m_suggestions.size());
        return true;
    }

    // support shortcuts under both standard latin and cyrillic / russian keymaps
    bool isKeyA = (sym == 'a' || sym == 'A' || sym == 0x0061 || sym == 0x0041 ||
                   sym == 0x06c6 || sym == 0x06e6 || sym == 0x0444 || sym == 0x0424);
    bool isKeyC = (sym == 'c' || sym == 'C' || sym == 0x0063 || sym == 0x0043 ||
                   sym == 0x06d3 || sym == 0x06f3 || sym == 0x0441 || sym == 0x0421);
    bool isKeyV = (sym == 'v' || sym == 'V' || sym == 0x0076 || sym == 0x0056 ||
                   sym == 0x06cd || sym == 0x06ed || sym == 0x043c || sym == 0x041c);
    bool isKeyX = (sym == 'x' || sym == 'X' || sym == 0x0078 || sym == 0x0058 ||
                   sym == 0x06de || sym == 0x06fe || sym == 0x0447 || sym == 0x0427);
    bool isKeyBksp = (sym == 0xff08 || sym == 0xff9f || sym == SDLK_BACKSPACE || sym == 8 || sym == 127);

    // ctrl+a: select all text
    if (isKeyA && isCtrl) {
        selectAll();
        return true;
    }

    // ctrl+c: copy
    if (isKeyC && isCtrl) {
        if (hasSelection()) {
            GdkDisplay* disp = gdk_display_get_default();
            if (disp) {
                GtkClipboard* clip = gtk_clipboard_get_for_display(disp, GDK_SELECTION_CLIPBOARD);
                if (clip) {
                    std::string sel = getSelectedText();
                    gtk_clipboard_set_text(clip, sel.c_str(), static_cast<gint>(sel.length()));
                }
            }
        }
        return true;
    }

    // ctrl+x: cut
    if (isKeyX && isCtrl) {
        if (hasSelection()) {
            GdkDisplay* disp = gdk_display_get_default();
            if (disp) {
                GtkClipboard* clip = gtk_clipboard_get_for_display(disp, GDK_SELECTION_CLIPBOARD);
                if (clip) {
                    std::string sel = getSelectedText();
                    gtk_clipboard_set_text(clip, sel.c_str(), static_cast<gint>(sel.length()));
                }
            }
            deleteSelection();
            updateSuggestions();
        }
        return true;
    }

    // ctrl+v: paste
    if (isKeyV && isCtrl) {
        GdkDisplay* disp = gdk_display_get_default();
        if (disp) {
            GtkClipboard* clip = gtk_clipboard_get_for_display(disp, GDK_SELECTION_CLIPBOARD);
            if (clip) {
                gchar* text = gtk_clipboard_wait_for_text(clip);
                if (text) {
                    if (hasSelection()) deleteSelection();
                    std::string s(text);
                    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
                    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
                    m_text.insert(m_cursorPos, s);
                    m_cursorPos += static_cast<int>(s.length());
                    clearSelection();
                    updateSuggestions();
                    g_free(text);
                }
            }
        }
        return true;
    }

    // ctrl+backspace: token-aware deletion (delimiters like /?=&. vs words)
    if (isKeyBksp && isCtrl) {
        if (hasSelection()) {
            deleteSelection();
            updateSuggestions();
        } else if (m_cursorPos > 0) {
            auto isDelim = [](char c) -> bool {
                return c == ' ' || c == '\t' || c == '/' || c == '?' || c == '&' ||
                       c == '=' || c == '.' || c == ':' || c == '-' || c == '_' ||
                       c == '#' || c == '@' || c == '+' || c == '%';
            };
            int pos = m_cursorPos;
            while (pos > 0 && m_text[pos - 1] == ' ') --pos;
            if (pos > 0 && isDelim(m_text[pos - 1])) {
                while (pos > 0 && isDelim(m_text[pos - 1])) --pos;
            } else {
                while (pos > 0 && !isDelim(m_text[pos - 1])) {
                    --pos;
                    while (pos > 0 && (static_cast<unsigned char>(m_text[pos]) & 0xC0) == 0x80) {
                        --pos;
                    }
                }
            }
            m_text.erase(pos, m_cursorPos - pos);
            m_cursorPos = pos;
            updateSuggestions();
        }
        return true;
    }

    // backspace: delete single codepoint (handles utf8 multibyte correctly)
    if (isKeyBksp) {
        if (hasSelection()) {
            deleteSelection();
            updateSuggestions();
            return true;
        }
        if (!m_text.empty() && m_cursorPos > 0) {
            int prev = m_cursorPos - 1;
            while (prev > 0 && (static_cast<unsigned char>(m_text[prev]) & 0xC0) == 0x80) {
                --prev;
            }
            m_text.erase(prev, m_cursorPos - prev);
            m_cursorPos = prev;
            updateSuggestions();
        }
        return true;
    }

    // delete: forward delete
    if (sym == 0xffff || sym == 0xff9f || sym == SDLK_DELETE || sym == 127) {
        if (hasSelection()) {
            deleteSelection();
            updateSuggestions();
            return true;
        }
        if (m_cursorPos < static_cast<int>(m_text.length())) {
            int next = m_cursorPos + 1;
            while (next < static_cast<int>(m_text.length()) && (static_cast<unsigned char>(m_text[next]) & 0xC0) == 0x80) {
                ++next;
            }
            m_text.erase(m_cursorPos, next - m_cursorPos);
            updateSuggestions();
        }
        return true;
    }

    // left arrow
    if (sym == 0xff51 || sym == 0xff96 || sym == SDLK_LEFT) {
        if (hasSelection()) {
            m_cursorPos = getSelMin();
            clearSelection();
        } else if (m_cursorPos > 0) {
            int prev = m_cursorPos - 1;
            while (prev > 0 && (static_cast<unsigned char>(m_text[prev]) & 0xC0) == 0x80) {
                --prev;
            }
            m_cursorPos = prev;
        }
        return true;
    }

    // right arrow
    if (sym == 0xff53 || sym == 0xff98 || sym == SDLK_RIGHT) {
        if (hasSelection()) {
            m_cursorPos = getSelMax();
            clearSelection();
        } else if (m_cursorPos < static_cast<int>(m_text.length())) {
            int next = m_cursorPos + 1;
            while (next < static_cast<int>(m_text.length()) && (static_cast<unsigned char>(m_text[next]) & 0xC0) == 0x80) {
                ++next;
            }
            m_cursorPos = next;
        }
        return true;
    }

    // home
    if (sym == 0xff50 || sym == 0xff95 || sym == SDLK_HOME) {
        m_cursorPos = 0;
        clearSelection();
        return true;
    }

    // end
    if (sym == 0xff57 || sym == 0xff9b || sym == SDLK_END) {
        m_cursorPos = static_cast<int>(m_text.length());
        clearSelection();
        return true;
    }

    // dont insert text if ctrl or alt is held
    if (isCtrl || (mod & 8)) {
        return false;
    }

    // regular typing insert
    if (textInput && textInput[0] != '\0') {
        unsigned char first = static_cast<unsigned char>(textInput[0]);
        if (first >= 32 && first != 127) {
            if (hasSelection()) deleteSelection();
            std::string s(textInput);
            m_text.insert(m_cursorPos, s);
            m_cursorPos += static_cast<int>(s.length());
            clearSelection();
            updateSuggestions();
            return true;
        }
    }
    return false;
}

bool OmniboxWidget::handleMouseMove(double mx, double my) {
    if (m_isSelecting && m_focused) {
        m_selEnd = xToCharIndex(mx);
        m_cursorPos = m_selEnd;
        return true;
    }

    if (!m_focused || !m_showPopup || m_suggestions.empty()) return false;

    int old = m_hoveredSuggestion;
    m_hoveredSuggestion = -1;
    if (mx >= m_lastX && mx <= m_lastX + m_lastW &&
        my >= m_popupY && my <= m_popupY + m_popupH) {
        int idx = static_cast<int>((my - m_popupY - 4.0) / 34.0);
        if (idx >= 0 && idx < static_cast<int>(m_suggestions.size()))
            m_hoveredSuggestion = idx;
    }
    return old != m_hoveredSuggestion;
}

bool OmniboxWidget::handleMouseDown(double mx, double my) {
    // click inside omnibox input area
    if (mx >= m_lastX && mx <= m_lastX + m_lastW &&
        my >= m_lastY && my <= m_lastY + m_lastH) {
        if (!m_focused) {
            setFocused(true);
        } else {
            int idx = xToCharIndex(mx);
            m_cursorPos = idx;
            m_selStart = idx;
            m_selEnd = idx;
            m_isSelecting = true;
        }
        return true;
    }
    // click inside suggestion dropdown item
    if (m_focused && m_showPopup && m_hoveredSuggestion >= 0) {
        m_selectedSuggestion = m_hoveredSuggestion;
        executeSelection();
        return true;
    }
    // click outside dismisses the popup and unfocuses
    if (m_focused) {
        setFocused(false);
    }
    return false;
}

bool OmniboxWidget::handleMouseUp(double /*mx*/, double /*my*/) {
    if (m_isSelecting) {
        m_isSelecting = false;
        if (m_selStart == m_selEnd) {
            clearSelection();
        }
        return true;
    }
    return false;
}

} // namespace Blueprint::UI
