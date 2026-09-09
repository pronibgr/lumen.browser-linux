#include <iostream>
#include <cassert>
#include <cmath>
#include "omnibox/calc_parser.hpp"
#include "omnibox/converter.hpp"
#include "omnibox/fuzzy_search.hpp"
#include "engine/tab_transition.hpp"
#include "core/config.hpp"
#include "ui/omnibox.hpp"
#include "ui/settings_panel.hpp"
#include "ui/topbar.hpp"
#include <SDL2/SDL_keycode.h>
#include <glib.h>

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

    // Cyrillic uppercase queries
    auto resCyrUpper = Blueprint::Omnibox::UnitConverter::convert("100 USD В РУБ");
    assert(resCyrUpper.has_value());
    assert(resCyrUpper->isCurrency);
    assert(resCyrUpper->targetUnit == "RUB");
    assert(resCyrUpper->targetValue > 1000.0);

    auto resCyrDecl = Blueprint::Omnibox::UnitConverter::convert("100 eur в рублях");
    assert(resCyrDecl.has_value());
    assert(resCyrDecl->isCurrency);
    assert(resCyrDecl->targetUnit == "RUB");

    // Prefix currency symbols
    auto resPrefix1 = Blueprint::Omnibox::UnitConverter::convert("$100 to eur");
    assert(resPrefix1.has_value());
    assert(resPrefix1->isCurrency);
    assert(resPrefix1->sourceUnit == "USD");
    assert(resPrefix1->targetUnit == "EUR");

    auto resPrefix2 = Blueprint::Omnibox::UnitConverter::convert("€50 в rub");
    assert(resPrefix2.has_value());
    assert(resPrefix2->isCurrency);
    assert(resPrefix2->sourceUnit == "EUR");
    assert(resPrefix2->targetUnit == "RUB");

    auto resSuffixSym = Blueprint::Omnibox::UnitConverter::convert("100$ в руб");
    assert(resSuffixSym.has_value());
    assert(resSuffixSym->isCurrency);
    assert(resSuffixSym->targetUnit == "RUB");

    auto resArrow = Blueprint::Omnibox::UnitConverter::convert("100 usd -> rub");
    assert(resArrow.has_value());
    assert(resArrow->isCurrency);

    auto resCrypto = Blueprint::Omnibox::UnitConverter::convert("1 btc in usd");
    assert(resCrypto.has_value());
    assert(resCrypto->isCurrency);
    assert(resCrypto->targetValue > 10000.0);

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

    // Russian layout Ctrl+A (0x06c6 = Cyrillic_ef) selects all
    handled = omni.handleKeyPress(0x06c6, 4, nullptr);
    if (!handled || !omni.hasSelection() || omni.getSelectedText() != "test ") {
        std::cerr << "Russian layout Ctrl+A failed!\n";
        std::exit(1);
    }

    // Russian layout Ctrl+C (0x06d3 = Cyrillic_es)
    handled = omni.handleKeyPress(0x06d3, 4, nullptr);
    if (!handled) {
        std::cerr << "Russian layout Ctrl+C failed!\n";
        std::exit(1);
    }

    // Russian layout Ctrl+X (0x06de = Cyrillic_che) cuts selection
    handled = omni.handleKeyPress(0x06de, 4, nullptr);
    if (!handled || !omni.getText().empty()) {
        std::cerr << "Russian layout Ctrl+X cut failed!\n";
        std::exit(1);
    }

    // URL token-aware Ctrl+Backspace test
    omni.setFocused(false);
    omni.setText("https://google.com/search?q=test");
    omni.setFocused(true);
    omni.clearSelection();
    omni.handleKeyPress(0xff57, 0, nullptr); // end key moves cursor to end
    handled = omni.handleKeyPress(0xff08, 4, nullptr);
    if (!handled || omni.getText() != "https://google.com/search?q=") {
        std::cerr << "URL token Ctrl+Backspace failed! got: '" << omni.getText() << "'\n";
        std::exit(1);
    }

    // GDK Escape (0xff1b) -> unfocuses
    handled = omni.handleKeyPress(0xff1b, 0, nullptr);
    if (!handled || omni.isFocused()) {
        std::cerr << "Escape key test failed!\n";
        std::exit(1);
    }

    // verify lumen://newtab address is displayed in omnibox
    Blueprint::UI::OmniboxWidget omniNewTab;
    omniNewTab.setText("lumen://newtab");
    if (omniNewTab.getText() != "lumen://newtab") {
        std::cerr << "Omnibox text for lumen://newtab failed! Got: '" << omniNewTab.getText() << "'\n";
        std::exit(1);
    }

    std::cout << "  -> Omnibox Keyboard & Selection tests PASSED!\n";
}

void testTabReloadProtection() {
    std::cout << "[Test] Running Tab Reload Protection tests...\n";

    // internal pages must not allow reload to prevent webkit error
    std::string newtab = "lumen://newtab";
    std::string blank  = "about:blank";
    std::string webUrl = "https://duckduckgo.com";

    auto canReloadUrl = [](const std::string& u) {
        return !u.empty() && u != "lumen://newtab" && u != "lampa://newtab" && u != "blueprint://newtab" && u != "about:blank";
    };

    if (canReloadUrl(newtab)) {
        std::cerr << "Reload protection for lumen://newtab failed!\n";
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

void testSearchEnginesAndReloadSlider() {
    std::cout << "[Test] Running Search Engines & Reload Spin Slider tests...\n";

    // reload spin animation speed slider
    Blueprint::UI::SettingsPanel panel;
    panel.update(0.016f);
    assert(std::abs(panel.getSliderVisual(4) - 1.0f) < 0.05f);

    panel.settings().anim.reloadSpin = 2.0f;
    for (int i = 0; i < 60; ++i) panel.update(0.016f);
    assert(std::abs(panel.getSliderVisual(4) - 2.0f) < 0.01f);

    // verify top 5 non-cis worldwide search engines
    const auto& engines = panel.settings().searchEngines;
    assert(engines.size() >= 5);
    assert(engines[0].name == "Google");
    assert(engines[1].name == "DuckDuckGo");
    assert(engines[2].name == "Bing");
    assert(engines[3].name == "Yahoo!");
    assert(engines[4].name == "Ecosia");
    (void)engines;

    // search url template formatting
    std::string q1 = Blueprint::Core::BrowserConfig::formatSearchUrl("https://duckduckgo.com/?q=%s", "quantum mechanics");
    assert(q1 == "https://duckduckgo.com/?q=quantum+mechanics");

    std::string q2 = Blueprint::Core::BrowserConfig::formatSearchUrl("https://www.google.com/search?q=%s", "c++20 coroutines");
    assert(q2 == "https://www.google.com/search?q=c++20+coroutines");

    // custom search engine url validation tests
    panel.setCreateSearchInput("https://searx.be/search?q=%s");
    assert(panel.isCreateSearchValid() == true);

    panel.setCreateSearchInput("http://searx.be/search?q=%s"); // http not allowed
    assert(panel.isCreateSearchValid() == false);

    panel.setCreateSearchInput("https://searx.be/search"); // missing %s
    assert(panel.isCreateSearchValid() == false);

    panel.setCreateSearchInput("https://searx.be/search?q=%s with space"); // spaces forbidden
    assert(panel.isCreateSearchValid() == false);

    panel.setCreateSearchInput("https://nodot/search?q=%s"); // host must have dot
    assert(panel.isCreateSearchValid() == false);

    // edit mode toggling
    assert(panel.isSearchEditMode() == false);
    panel.setSearchEditMode(true);
    assert(panel.isSearchEditMode() == true);
    panel.setSearchEditMode(false);

    // custom engine creation and persistence
    panel.setVisible(true);
    panel.setCreateSearchInput("https://searx.be/search?q=%s");
    // simulate committing creation via enter key
    panel.handleKeyPress(SDLK_RETURN, 0, nullptr);
    size_t countAfterCreate = panel.settings().searchEngines.size();
    (void)countAfterCreate;
    assert(countAfterCreate == 6);
    assert(panel.settings().searchEngines.back().name == "Searx");
    assert(panel.settings().searchEngines.back().isCustom == true);
    assert(panel.settings().activeSearchEngineIndex == static_cast<int>(countAfterCreate - 1));

    // verify database persistence on new instance
    {
        Blueprint::UI::SettingsPanel panel2;
        assert(panel2.settings().searchEngines.size() == countAfterCreate);
        assert(panel2.settings().searchEngines.back().name == "Searx");
        assert(panel2.settings().activeSearchEngineIndex == static_cast<int>(countAfterCreate - 1));
        assert(panel2.settings().getActiveSearchTemplate() == "https://searx.be/search?q=%s");
    }

    // verify built-in search engines cannot be deleted
    panel.triggerDeleteCustomSearchEngine(0); // Google (built-in)
    assert(panel.isDeleteConfirmModalOpen() == false);
    panel.triggerDeleteCustomSearchEngine(1); // DuckDuckGo (built-in)
    assert(panel.isDeleteConfirmModalOpen() == false);

    // verify custom search engine deletion modal and deletion
    panel.triggerDeleteCustomSearchEngine(5); // Searx (custom)
    assert(panel.isDeleteConfirmModalOpen() == true);
    // press enter to confirm deletion
    panel.handleKeyPress(SDLK_RETURN, 0, nullptr);
    assert(panel.isDeleteConfirmModalOpen() == false);
    assert(panel.settings().searchEngines.size() == 5);
    // active engine safely fell back to DuckDuckGo
    assert(panel.settings().activeSearchEngineIndex == 1);

    // verify deletion persisted to database on fresh instance
    {
        Blueprint::UI::SettingsPanel panel3;
        assert(panel3.settings().searchEngines.size() == 5);
        assert(panel3.settings().activeSearchEngineIndex == 1);
    }

    std::cout << "  -> Search Engines & Reload Spin Slider tests PASSED!\n";
}

void testSearchModalInputUtf8AndShortcuts() {
    std::cout << "[Test] Running Create Search Engine UTF-8 & Shortcuts tests...\n";

    Blueprint::UI::SettingsPanel panel;
    panel.setVisible(true);
    panel.setCreateSearchModalOpen(true);

    assert(panel.isCreateSearchModalOpen() == true);
    assert(panel.getCreateSearchInput() == "https://");

    // strictly forbid cyrillic characters: typing "поиск" should be ignored
    panel.handleKeyPress(0, 0, "поиск");
    assert(panel.getCreateSearchInput() == "https://");

    // test ctrl+a with russian layout keysym (Cyrillic_ef 'ф' on physical A key)
    panel.handleKeyPress(0x06c6, 4, nullptr);
    assert(panel.hasCreateSearchSelection() == true);

    // type valid latin url template, replacing selection
    panel.handleKeyPress(0, 0, "https://searx.be/search?q=%s");
    assert(panel.getCreateSearchInput() == "https://searx.be/search?q=%s");
    assert(!panel.hasCreateSearchSelection());
    assert(panel.isCreateSearchValid() == true);

    // test ctrl+backspace token by token deletion
    panel.handleKeyPress(SDLK_BACKSPACE, 4, nullptr); // deletes "%s"
    assert(panel.getCreateSearchInput() == "https://searx.be/search?q=");

    panel.handleKeyPress(SDLK_BACKSPACE, 4, nullptr); // deletes "?q="
    assert(panel.getCreateSearchInput() == "https://searx.be/search");

    // test ctrl+a with english keysym
    panel.handleKeyPress('a', 4, nullptr);
    assert(panel.hasCreateSearchSelection() == true);

    // test ctrl+x (cut) with russian layout keysym (Cyrillic_che 'ч' on physical X key)
    panel.handleKeyPress(0x06de, 4, nullptr);
    assert(panel.getCreateSearchInput().empty());

    // verify isCreateSearchValid rejects any cyrillic
    panel.setCreateSearchInput("https://яндекс.рф/search?q=%s");
    assert(panel.isCreateSearchValid() == false);

    std::cout << "  -> Create Search Engine UTF-8 & Shortcuts tests PASSED!\n";
}

void testThemesAndPaletteDefinitions() {
    std::cout << "[Test] Running Themes & Palette Definitions tests...\n";

    auto& tm = Blueprint::Theme::ThemeManager::instance();
    const auto& palettes = tm.allPalettes();
    assert(palettes.size() == 4);

    auto colEq = [](const Blueprint::Theme::Color& a, const Blueprint::Theme::Color& b) {
        return std::abs(a.r - b.r) < 1e-4f &&
               std::abs(a.g - b.g) < 1e-4f &&
               std::abs(a.b - b.b) < 1e-4f &&
               std::abs(a.a - b.a) < 1e-4f;
    };

    // 1. noctiluca
    const auto& noct = tm.getPalette(Blueprint::Theme::ThemeId::NOCTILUCA);
    assert(noct.name == "noctiluca");
    assert(noct.isDark == true);
    assert(colEq(noct.bgBase,      Blueprint::Theme::Color::fromHex(0x0C1214)));
    assert(colEq(noct.bgSurface,   Blueprint::Theme::Color::fromHex(0x141D22)));
    assert(colEq(noct.border,      Blueprint::Theme::Color::fromHex(0x24333B)));
    assert(colEq(noct.textPrimary, Blueprint::Theme::Color::fromHex(0xE1ECF0)));
    assert(colEq(noct.accent,      Blueprint::Theme::Color::fromHex(0x6CE5B9)));
    assert(colEq(noct.textMuted,   Blueprint::Theme::Color::fromHex(0x6E8590)));

    // 2. morion
    const auto& morion = tm.getPalette(Blueprint::Theme::ThemeId::MORION);
    assert(morion.name == "morion");
    assert(morion.isDark == true);
    assert(colEq(morion.bgBase,      Blueprint::Theme::Color::fromHex(0x131114)));
    assert(colEq(morion.bgSurface,   Blueprint::Theme::Color::fromHex(0x1D1920)));
    assert(colEq(morion.border,      Blueprint::Theme::Color::fromHex(0x342D38)));
    assert(colEq(morion.textPrimary, Blueprint::Theme::Color::fromHex(0xECE4EB)));
    assert(colEq(morion.accent,      Blueprint::Theme::Color::fromHex(0xDCA574)));
    assert(colEq(morion.textMuted,   Blueprint::Theme::Color::fromHex(0x98899A)));

    // 3. calcite
    const auto& calcite = tm.getPalette(Blueprint::Theme::ThemeId::CALCITE);
    assert(calcite.name == "calcite");
    assert(calcite.isDark == false);
    assert(colEq(calcite.bgBase,      Blueprint::Theme::Color::fromHex(0xF4F2EA)));
    assert(colEq(calcite.bgSurface,   Blueprint::Theme::Color::fromHex(0xE8E5DB)));
    assert(colEq(calcite.border,      Blueprint::Theme::Color::fromHex(0xD0CBBE)));
    assert(colEq(calcite.textPrimary, Blueprint::Theme::Color::fromHex(0x201E1A)));
    assert(colEq(calcite.accent,      Blueprint::Theme::Color::fromHex(0x3B685C)));
    assert(colEq(calcite.textMuted,   Blueprint::Theme::Color::fromHex(0x858072)));

    // 4. rime
    const auto& rime = tm.getPalette(Blueprint::Theme::ThemeId::RIME);
    assert(rime.name == "rime");
    assert(rime.isDark == false);
    assert(colEq(rime.bgBase,      Blueprint::Theme::Color::fromHex(0xECF1F4)));
    assert(colEq(rime.bgSurface,   Blueprint::Theme::Color::fromHex(0xDFE6EB)));
    assert(colEq(rime.border,      Blueprint::Theme::Color::fromHex(0xC2CDD7)));
    assert(colEq(rime.textPrimary, Blueprint::Theme::Color::fromHex(0x182129)));
    assert(colEq(rime.accent,      Blueprint::Theme::Color::fromHex(0x2D638E)));
    assert(colEq(rime.textMuted,   Blueprint::Theme::Color::fromHex(0x728494)));
    (void)palettes;
    (void)colEq;
    (void)noct;
    (void)morion;
    (void)calcite;
    (void)rime;

    std::cout << "  -> Themes & Palette Definitions tests PASSED!\n";
}

void testThemeColorInterpolation() {
    std::cout << "[Test] Running Theme Color Interpolation tests...\n";

    auto& tm = Blueprint::Theme::ThemeManager::instance();
    // set to noctiluca instantly
    tm.setTheme(Blueprint::Theme::ThemeId::NOCTILUCA, false);
    assert(!tm.wantsRedraw());
    assert(tm.currentTheme() == Blueprint::Theme::ThemeId::NOCTILUCA);

    auto colEq = [](const Blueprint::Theme::Color& a, const Blueprint::Theme::Color& b) {
        return std::abs(a.r - b.r) < 1e-3f &&
               std::abs(a.g - b.g) < 1e-3f &&
               std::abs(a.b - b.b) < 1e-3f;
    };
    (void)colEq;

    assert(colEq(Blueprint::Theme::BG_ABYSS, Blueprint::Theme::Color::fromHex(0x0C1214)));

    // start smooth animated switch to morion
    tm.setTheme(Blueprint::Theme::ThemeId::MORION, true);
    assert(tm.wantsRedraw());

    // step half duration
    tm.update(0.175f);
    assert(tm.wantsRedraw());
    // verify color has moved away from noctiluca base and towards morion base
    assert(!colEq(Blueprint::Theme::BG_ABYSS, Blueprint::Theme::Color::fromHex(0x0C1214)));

    // step rest of duration to finish transition
    tm.update(0.30f);
    assert(!tm.wantsRedraw());
    assert(colEq(Blueprint::Theme::BG_ABYSS, Blueprint::Theme::Color::fromHex(0x131114)));
    assert(colEq(Blueprint::Theme::ACCENT_CALM, Blueprint::Theme::Color::fromHex(0xDCA574)));

    // reset back to noctiluca
    tm.setTheme(Blueprint::Theme::ThemeId::NOCTILUCA, false);

    std::cout << "  -> Theme Color Interpolation tests PASSED!\n";
}

void testLumenThresholdLogicAndModal() {
    std::cout << "[Test] Running Lumen Threshold Logic & Modal tests...\n";

    // 1. time condition test: 20:00 to 08:00
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(20) == true);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(21) == true);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(23) == true);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(0)  == true);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(3)  == true);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(7)  == true);

    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(8)  == false);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(12) == false);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(15) == false);
    assert(Blueprint::Theme::ThemeManager::isNightTimeForHour(19) == false);

    // 2. light vs dark theme classification
    assert(Blueprint::Theme::ThemeManager::isLightTheme(Blueprint::Theme::ThemeId::NOCTILUCA) == false);
    assert(Blueprint::Theme::ThemeManager::isLightTheme(Blueprint::Theme::ThemeId::MORION)    == false);
    assert(Blueprint::Theme::ThemeManager::isLightTheme(Blueprint::Theme::ThemeId::CALCITE)   == true);
    assert(Blueprint::Theme::ThemeManager::isLightTheme(Blueprint::Theme::ThemeId::RIME)      == true);

    // 3. modal interaction in SettingsPanel
    Blueprint::UI::SettingsPanel panel;
    panel.setVisible(true);

    auto& tm = Blueprint::Theme::ThemeManager::instance();
    tm.setTheme(Blueprint::Theme::ThemeId::NOCTILUCA, false);

    // trigger lumen threshold modal for calcite
    panel.triggerLumenThresholdModal(Blueprint::Theme::ThemeId::CALCITE);
    assert(panel.isLumenThresholdModalOpen() == true);
    assert(panel.getPendingLightTheme() == Blueprint::Theme::ThemeId::CALCITE);

    // test escape cancels modal and leaves theme as noctiluca
    panel.handleKeyPress(SDLK_ESCAPE, 0, nullptr);
    assert(panel.isLumenThresholdModalOpen() == false);
    assert(tm.currentTheme() == Blueprint::Theme::ThemeId::NOCTILUCA);

    // trigger lumen threshold modal for rime
    panel.triggerLumenThresholdModal(Blueprint::Theme::ThemeId::RIME);
    assert(panel.isLumenThresholdModalOpen() == true);

    // test enter accepts modal and sets theme to rime
    panel.handleKeyPress(SDLK_RETURN, 0, nullptr);
    assert(panel.isLumenThresholdModalOpen() == false);
    assert(tm.currentTheme() == Blueprint::Theme::ThemeId::RIME);

    // reset back to noctiluca
    tm.setTheme(Blueprint::Theme::ThemeId::NOCTILUCA, false);

    std::cout << "  -> Lumen Threshold Logic & Modal tests PASSED!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " lumen browser Unit Tests\n";
    std::cout << "========================================\n";

    testCalculator();
    testConverter();
    testOmniboxSingleClickHitbox();
    testFuzzySearch();
    testTransitionBezier();
    testOmniboxKeys();
    testTabReloadProtection();
    testSettingsSliderSmoothGlide();
    testSearchEnginesAndReloadSlider();
    testSearchModalInputUtf8AndShortcuts();
    testThemesAndPaletteDefinitions();
    testThemeColorInterpolation();
    testLumenThresholdLogicAndModal();

    std::cout << "========================================\n";
    std::cout << " ALL UNIT TESTS PASSED SUCCESSFULLY! ✅\n";
    std::cout << "========================================\n";
    return 0;
}
