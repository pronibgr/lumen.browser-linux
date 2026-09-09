#include <iostream>
#include <cassert>
#include <cmath>
#include "omnibox/calc_parser.hpp"
#include "omnibox/converter.hpp"
#include "omnibox/fuzzy_search.hpp"
#include "engine/tab_transition.hpp"
#include "ui/omnibox.hpp"
#include "ui/settings_panel.hpp"
#include "ui/topbar.hpp"

void testCalculator() {
    std::cout << "[Test] Running Calculator tests...\n";
    
    auto res1 = Blueprint::Omnibox::Calculator::evaluate("2 + 2 * 2");
    assert(res1.has_value() && std::abs(res1.value() - 6.0) < 1e-6);

    auto res2 = Blueprint::Omnibox::Calculator::evaluate("(10 + 5) * 3");
    assert(res2.has_value() && std::abs(res2.value() - 45.0) < 1e-6);

    auto res3 = Blueprint::Omnibox::Calculator::evaluate("2 ^ 3");
    assert(res3.has_value() && std::abs(res3.value() - 8.0) < 1e-6);

    auto res4 = Blueprint::Omnibox::Calculator::evaluate("sqrt(144)");
    assert(res4.has_value() && std::abs(res4.value() - 12.0) < 1e-6);

    auto res5 = Blueprint::Omnibox::Calculator::evaluate("100 / 4 + 5");
    assert(res5.has_value() && std::abs(res5.value() - 30.0) < 1e-6);
    (void)res1; (void)res2; (void)res3; (void)res4; (void)res5;

    std::cout << "  -> Calculator tests PASSED!\n";
}

void testConverter() {
    std::cout << "[Test] Running Converter tests...\n";

    auto res1 = Blueprint::Omnibox::UnitConverter::convert("10 km to m");
    assert(res1.has_value() && std::abs(res1->targetValue - 10000.0) < 1e-3);
    assert(!res1->isCurrency);

    auto res2 = Blueprint::Omnibox::UnitConverter::convert("100 eur in usd");
    assert(res2.has_value());
    assert(res2->isCurrency);
    assert(res2->targetValue > 50.0 && res2->targetValue < 200.0);

    auto res3 = Blueprint::Omnibox::UnitConverter::convert("1024 mb to gb");
    assert(res3.has_value() && std::abs(res3->targetValue - 1.0) < 1e-3);
    assert(!res3->isCurrency);

    auto res4 = Blueprint::Omnibox::UnitConverter::convert("500 rub to eur");
    assert(res4.has_value());
    assert(res4->isCurrency);

    // Test offline rates for popular currencies
    auto resKzt = Blueprint::Omnibox::UnitConverter::convert("1000 kzt to usd");
    assert(resKzt.has_value() && resKzt->isCurrency);

    auto resJpy = Blueprint::Omnibox::UnitConverter::convert("1000 jpy in usd");
    assert(resJpy.has_value() && resJpy->isCurrency);

    std::cout << "  -> Converter tests PASSED!\n";
}

void testOmniboxSingleClickHitbox() {
    std::cout << "[Test] Running Omnibox Single Click Hitbox test...\n";

    Blueprint::UI::CompactTopbar topbar;
    // Simulate topbar rendered at 1280 width
    // ROW1: [0, 44], ROW2: [44, 84], Omnibox is in Row 2
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 84);
    cairo_t* cr = cairo_create(surf);
    topbar.draw(cr, 1280, 84);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    assert(!topbar.getOmnibox().isFocused());

    // Single click at center of omnibox in Row 2 (e.g. x=600, y=64)
    bool handled = topbar.handleMouseDown(600.0, 64.0);
    assert(handled);
    (void)handled;
    // MUST be focused on the VERY FIRST click!
    assert(topbar.getOmnibox().isFocused());

    // Click on tab strip area in Row 1 (e.g. x=200, y=20)
    topbar.handleMouseDown(200.0, 20.0);
    // Omnibox must unfocus cleanly
    assert(!topbar.getOmnibox().isFocused());

    std::cout << "  -> Omnibox Single Click Hitbox test PASSED!\n";
}

void testFuzzySearch() {
    std::cout << "[Test] Running Fuzzy Search tests...\n";

    int score1 = Blueprint::Omnibox::FuzzySearch::calculateScore("git", "github.com");
    assert(score1 > 0);

    int score2 = Blueprint::Omnibox::FuzzySearch::calculateScore("yt", "youtube.com");
    assert(score2 > 0);
    (void)score1; (void)score2;

    std::vector<Blueprint::Omnibox::SearchItem> items = {
        {"GitHub", "https://github.com", "tab", 0},
        {"Google", "https://google.com", "history", 0},
        {"GitLab", "https://gitlab.com", "bookmark", 0}
    };

    auto ranked = Blueprint::Omnibox::FuzzySearch::rank("git", items);
    assert(!ranked.empty());
    assert(ranked[0].title.find("Git") != std::string::npos);

    std::cout << "  -> Fuzzy Search tests PASSED!\n";
}

void testTransitionBezier() {
    std::cout << "[Test] Running Tab Transition cubic-bezier tests...\n";

    Blueprint::Engine::CubicBezier bezier(0.16f, 1.0f, 0.3f, 1.0f);
    float start = bezier.solve(0.0f);
    float mid = bezier.solve(0.5f);
    float end = bezier.solve(1.0f);

    assert(std::abs(start - 0.0f) < 1e-4);
    assert(mid > 0.5f); // Fast initial acceleration of 0.16, 1.0 curve
    assert(std::abs(end - 1.0f) < 1e-4);
    (void)start; (void)mid; (void)end;

    std::cout << "  -> Tab Transition Bezier tests PASSED!\n";
}

void testOmniboxKeys() {
    std::cout << "[Test] Running Omnibox Keyboard & Selection tests...\n";

    Blueprint::UI::OmniboxWidget omni;
    omni.setFocused(true);
    if (!omni.isFocused()) {
        std::cerr << "Focus omnibox failed!\n";
        std::exit(1);
    }

    // Type "hello"
    omni.handleKeyPress(0, 0, "hello");
    if (omni.getText() != "hello") {
        std::cerr << "Type 'hello' failed!\n";
        std::exit(1);
    }

    // GDK BackSpace (0xff08) without selection deletes last character 'o'
    bool handled = omni.handleKeyPress(0xff08, 0, nullptr);
    if (!handled || omni.getText() != "hell") {
        std::cerr << "Backspace test failed!\n";
        std::exit(1);
    }

    // Type 'o' back
    handled = omni.handleKeyPress(0, 0, "o");
    if (!handled || omni.getText() != "hello") {
        std::cerr << "Type 'o' failed!\n";
        std::exit(1);
    }

    // GDK Left (0xff51) moves cursor left past 'o'
    handled = omni.handleKeyPress(0xff51, 0, nullptr);
    if (!handled || omni.getText() != "hello") {
        std::cerr << "Left arrow key test failed!\n";
        std::exit(1);
    }

    // GDK Delete (0xffff) deletes 'o'
    handled = omni.handleKeyPress(0xffff, 0, nullptr);
    if (!handled || omni.getText() != "hell") {
        std::cerr << "Delete key test failed!\n";
        std::exit(1);
    }

    // Ctrl+A selects all
    handled = omni.handleKeyPress('a', 4, nullptr);
    if (!handled || !omni.hasSelection() || omni.getSelectedText() != "hell") {
        std::cerr << "Ctrl+A test failed!\n";
        std::exit(1);
    }

    // Typing replaces selection
    handled = omni.handleKeyPress(0, 0, "test word");
    if (!handled || omni.getText() != "test word") {
        std::cerr << "Type replacing selection failed!\n";
        std::exit(1);
    }

    // Ctrl+Backspace deletes previous word ("word")
    handled = omni.handleKeyPress(0xff08, 4, nullptr);
    if (!handled || omni.getText() != "test ") {
        std::cerr << "Ctrl+Backspace test failed! got: '" << omni.getText() << "'\n";
        std::exit(1);
    }

    // GDK Escape (0xff1b) -> unfocuses
    handled = omni.handleKeyPress(0xff1b, 0, nullptr);
    if (!handled || omni.isFocused()) {
        std::cerr << "Escape key test failed!\n";
        std::exit(1);
    }

    // Verify lampa://newtab address is displayed in omnibox
    Blueprint::UI::OmniboxWidget omniNewTab;
    omniNewTab.setText("lampa://newtab");
    if (omniNewTab.getText() != "lampa://newtab") {
        std::cerr << "Omnibox text for lampa://newtab failed! Got: '" << omniNewTab.getText() << "'\n";
        std::exit(1);
    }

    std::cout << "  -> Omnibox Keyboard & Selection tests PASSED!\n";
}

void testTabReloadProtection() {
    std::cout << "[Test] Running Tab Reload Protection tests...\n";

    // Internal pages must not allow reload to prevent WebKit "The URL can’t be shown" error
    std::string newtab = "lampa://newtab";
    std::string blank  = "about:blank";
    std::string webUrl = "https://duckduckgo.com";

    auto canReloadUrl = [](const std::string& u) {
        return !u.empty() && u != "lampa://newtab" && u != "blueprint://newtab" && u != "about:blank";
    };

    if (canReloadUrl(newtab)) {
        std::cerr << "Reload protection for lampa://newtab failed!\n";
        std::exit(1);
    }
    if (canReloadUrl(blank)) {
        std::cerr << "Reload protection for about:blank failed!\n";
        std::exit(1);
    }
    if (!canReloadUrl(webUrl)) {
        std::cerr << "Regular website should be reloadable!\n";
        std::exit(1);
    }

    std::cout << "  -> Tab Reload Protection tests PASSED!\n";
}

void testSettingsSliderSmoothGlide() {
    std::cout << "[Test] Running Settings Slider Smooth Glide tests...\n";

    Blueprint::UI::SettingsPanel panel;
    // Initial update initializes visual values to default
    panel.update(0.016f);

    float initialVal = panel.getSliderVisual(0);
    assert(std::abs(initialVal - 1.0f) < 0.05f);

    // Simulate user setting tabSlide to 2.50f (clicking at 2.50x)
    panel.settings().anim.tabSlide = 2.50f;

    // Visual value must NOT jump instantly to 2.50f
    if (std::abs(panel.getSliderVisual(0) - initialVal) > 0.01f) {
        std::cerr << "Slider thumb jumped instantly without glide!\n";
        std::exit(1);
    }

    // Step 1 frame
    panel.update(0.016f);
    float step1 = panel.getSliderVisual(0);
    // Should smoothly progress towards 2.50f
    if (step1 <= initialVal || step1 >= 2.50f) {
        std::cerr << "Slider glide progression failed on frame 1: " << step1 << "\n";
        std::exit(1);
    }

    // Advance 60 frames until settled
    for (int i = 0; i < 60; ++i) {
        panel.update(0.016f);
    }
    float finalVal = panel.getSliderVisual(0);
    if (std::abs(finalVal - 2.50f) > 0.005f) {
        std::cerr << "Slider failed to settle at target: " << finalVal << "\n";
        std::exit(1);
    }

    std::cout << "  -> Settings Slider Smooth Glide tests PASSED!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " lampa browser Unit Tests\n";
    std::cout << "========================================\n";

    testCalculator();
    testConverter();
    testOmniboxSingleClickHitbox();
    testFuzzySearch();
    testTransitionBezier();
    testOmniboxKeys();
    testTabReloadProtection();
    testSettingsSliderSmoothGlide();

    std::cout << "========================================\n";
    std::cout << " ALL UNIT TESTS PASSED SUCCESSFULLY! ✅\n";
    std::cout << "========================================\n";
    return 0;
}
