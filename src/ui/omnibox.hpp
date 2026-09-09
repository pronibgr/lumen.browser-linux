#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cairo/cairo.h>
#include "omnibox/fuzzy_search.hpp"

namespace Blueprint::UI {

// Alias SearchItem as Suggestion for UI usage
using Suggestion = Omnibox::SearchItem;

class OmniboxWidget {
public:
    OmniboxWidget();

    void setText(const std::string& url);
    void setFocused(bool f);
    bool isFocused() const { return m_focused; }
    bool isPopupOpen() const { return m_showPopup && !m_suggestions.empty(); }
    const std::string& getText() const { return m_text; }

    void setOnNavigate(std::function<void(const std::string&)> cb) { m_onNavigate = cb; }

    void draw        (cairo_t* cr, double x, double y, double w, double h);
    void drawPopup   (cairo_t* cr, double x, double y, double w);

    bool handleMouseMove(double mx, double my);
    bool handleMouseDown(double mx, double my);
    bool handleMouseUp  (double mx, double my);
    bool handleKeyPress (uint32_t sym, uint16_t mod, const char* textInput);

    // Selection helpers
    bool hasSelection() const { return m_selStart >= 0 && m_selEnd >= 0 && m_selStart != m_selEnd; }
    int  getSelMin()    const { return std::min(m_selStart, m_selEnd); }
    int  getSelMax()    const { return std::max(m_selStart, m_selEnd); }
    void clearSelection()     { m_selStart = -1; m_selEnd = -1; }
    void selectAll();
    std::string getSelectedText() const;
    void deleteSelection();

    // For layout read-back
    double getLastX() const { return m_lastX; }
    double getLastY() const { return m_lastY; }
    double getLastW() const { return m_lastW; }
    double getLastH() const { return m_lastH; }

private:
    std::string m_displayUrl;      // shown when unfocused (current page URL)
    std::string m_text;            // editable text when focused
    int m_cursorPos = 0;

    // Selection range (byte indices in m_text)
    int  m_selStart = -1;
    int  m_selEnd   = -1;
    bool m_isSelecting = false;

    bool m_focused = false;
    bool m_showPopup = false;

    std::vector<Suggestion> m_suggestions;
    int m_selectedSuggestion = -1;
    int m_hoveredSuggestion  = -1;

    // Layout cache
    double m_lastX = 0, m_lastY = 0, m_lastW = 0, m_lastH = 0;
    double m_popupY = 0, m_popupH = 0;

    std::function<void(const std::string&)> m_onNavigate;

    int  xToCharIndex(double mouseX);
    void updateSuggestions();
    void executeSelection();
};

} // namespace Blueprint::UI
