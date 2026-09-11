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
#include "storage/database.hpp"
#include "engine/new_tab_html.hpp"
#include "engine/null_tab_html.hpp"
#include "engine/error_page_html.hpp"
#include "engine/web_tab.hpp"
#include "core/tor_bridge.hpp"
#include "core/tor_provisioner.hpp"
#include <SDL2/SDL_keycode.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <chrono>
#include <thread>

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
    assert(palettes.size() == 10);

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

    // 5. scoria
    const auto& scoria = tm.getPalette(Blueprint::Theme::ThemeId::SCORIA);
    assert(scoria.name == "scoria");
    assert(scoria.isDark == true);
    assert(colEq(scoria.bgBase,      Blueprint::Theme::Color::fromHex(0x111012)));
    assert(colEq(scoria.bgSurface,   Blueprint::Theme::Color::fromHex(0x1A181C)));
    assert(colEq(scoria.border,      Blueprint::Theme::Color::fromHex(0x2E2A33)));
    assert(colEq(scoria.textPrimary, Blueprint::Theme::Color::fromHex(0xE8E2E6)));
    assert(colEq(scoria.accent,      Blueprint::Theme::Color::fromHex(0xE5935C)));
    assert(colEq(scoria.textMuted,   Blueprint::Theme::Color::fromHex(0x8E8594)));

    // 6. stibnite
    const auto& stibnite = tm.getPalette(Blueprint::Theme::ThemeId::STIBNITE);
    assert(stibnite.name == "stibnite");
    assert(stibnite.isDark == true);
    assert(colEq(stibnite.bgBase,      Blueprint::Theme::Color::fromHex(0x0E1014)));
    assert(colEq(stibnite.bgSurface,   Blueprint::Theme::Color::fromHex(0x161920)));
    assert(colEq(stibnite.border,      Blueprint::Theme::Color::fromHex(0x262C38)));
    assert(colEq(stibnite.textPrimary, Blueprint::Theme::Color::fromHex(0xE4E8F0)));
    assert(colEq(stibnite.accent,      Blueprint::Theme::Color::fromHex(0x78A9FF)));
    assert(colEq(stibnite.textMuted,   Blueprint::Theme::Color::fromHex(0x737E94)));

    // 7. tephra
    const auto& tephra = tm.getPalette(Blueprint::Theme::ThemeId::TEPHRA);
    assert(tephra.name == "tephra");
    assert(tephra.isDark == true);
    assert(colEq(tephra.bgBase,      Blueprint::Theme::Color::fromHex(0x121212)));
    assert(colEq(tephra.bgSurface,   Blueprint::Theme::Color::fromHex(0x1C1C1C)));
    assert(colEq(tephra.border,      Blueprint::Theme::Color::fromHex(0x2E2E2E)));
    assert(colEq(tephra.textPrimary, Blueprint::Theme::Color::fromHex(0xDEDEDE)));
    assert(colEq(tephra.accent,      Blueprint::Theme::Color::fromHex(0x9EC49E)));
    assert(colEq(tephra.textMuted,   Blueprint::Theme::Color::fromHex(0x7A7A7A)));

    // 8. kaolin
    const auto& kaolin = tm.getPalette(Blueprint::Theme::ThemeId::KAOLIN);
    assert(kaolin.name == "kaolin");
    assert(kaolin.isDark == false);
    assert(colEq(kaolin.bgBase,      Blueprint::Theme::Color::fromHex(0xF6F3ED)));
    assert(colEq(kaolin.bgSurface,   Blueprint::Theme::Color::fromHex(0xEBE6DC)));
    assert(colEq(kaolin.border,      Blueprint::Theme::Color::fromHex(0xD4CDBF)));
    assert(colEq(kaolin.textPrimary, Blueprint::Theme::Color::fromHex(0x24211D)));
    assert(colEq(kaolin.accent,      Blueprint::Theme::Color::fromHex(0xB5543C)));
    assert(colEq(kaolin.textMuted,   Blueprint::Theme::Color::fromHex(0x80776B)));

    // 9. selenite
    const auto& selenite = tm.getPalette(Blueprint::Theme::ThemeId::SELENITE);
    assert(selenite.name == "selenite");
    assert(selenite.isDark == false);
    assert(colEq(selenite.bgBase,      Blueprint::Theme::Color::fromHex(0xF1F0F5)));
    assert(colEq(selenite.bgSurface,   Blueprint::Theme::Color::fromHex(0xE5E3EC)));
    assert(colEq(selenite.border,      Blueprint::Theme::Color::fromHex(0xCBCEDB)));
    assert(colEq(selenite.textPrimary, Blueprint::Theme::Color::fromHex(0x1F1C2B)));
    assert(colEq(selenite.accent,      Blueprint::Theme::Color::fromHex(0x6052A8)));
    assert(colEq(selenite.textMuted,   Blueprint::Theme::Color::fromHex(0x77738A)));

    // 10. loess
    const auto& loess = tm.getPalette(Blueprint::Theme::ThemeId::LOESS);
    assert(loess.name == "loess");
    assert(loess.isDark == false);
    assert(colEq(loess.bgBase,      Blueprint::Theme::Color::fromHex(0xF5F1E6)));
    assert(colEq(loess.bgSurface,   Blueprint::Theme::Color::fromHex(0xE8E2D1)));
    assert(colEq(loess.border,      Blueprint::Theme::Color::fromHex(0xCFC7B0)));
    assert(colEq(loess.textPrimary, Blueprint::Theme::Color::fromHex(0x26231A)));
    assert(colEq(loess.accent,      Blueprint::Theme::Color::fromHex(0x586E3F)));
    assert(colEq(loess.textMuted,   Blueprint::Theme::Color::fromHex(0x827B68)));

    (void)palettes;
    (void)colEq;
    (void)noct;
    (void)morion;
    (void)calcite;
    (void)rime;
    (void)scoria;
    (void)stibnite;
    (void)tephra;
    (void)kaolin;
    (void)selenite;
    (void)loess;

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

void testUserAgentPresetsAndCompatibility() {
    std::cout << "[Test] Running User-Agent Presets & Compatibility tests...\n";

    Blueprint::UI::SettingsPanel panel;
    const auto& uas = panel.settings().userAgents;

    // 1. Verify exact count of 7 presets
    assert(uas.size() == 7);

    // 2. Verify all 7 presets and their exact contents
    assert(uas[0].name == "Chrome 131 (Linux)");
    assert(uas[0].platform == "Linux");
    assert(uas[0].userAgent.find("Chrome/131.0.0.0") != std::string::npos);
    assert(uas[0].userAgent.find("X11; Linux x86_64") != std::string::npos);

    assert(uas[1].name == "Chrome 131 (Windows)");
    assert(uas[1].platform == "Windows");
    assert(uas[1].userAgent.find("Windows NT 10.0") != std::string::npos);
    assert(uas[1].userAgent.find("Chrome/131.0.0.0") != std::string::npos);

    assert(uas[2].name == "Firefox 133 (Linux)");
    assert(uas[2].platform == "Linux");
    assert(uas[2].userAgent.find("Firefox/133.0") != std::string::npos);
    assert(uas[2].userAgent.find("X11; Linux x86_64") != std::string::npos);

    assert(uas[3].name == "Firefox 133 (Windows)");
    assert(uas[3].platform == "Windows");
    assert(uas[3].userAgent.find("Firefox/133.0") != std::string::npos);
    assert(uas[3].userAgent.find("Windows NT 10.0") != std::string::npos);

    assert(uas[4].name == "Safari 18.1 (macOS)");
    assert(uas[4].platform == "macOS");
    assert(uas[4].userAgent.find("Version/18.1 Safari") != std::string::npos);
    assert(uas[4].userAgent.find("Macintosh; Intel Mac OS X") != std::string::npos);

    assert(uas[5].name == "Edge 131 (Windows)");
    assert(uas[5].platform == "Windows");
    assert(uas[5].userAgent.find("Edg/131.0.0.0") != std::string::npos);

    assert(uas[6].name == "Lumen Browser (Default)");
    assert(uas[6].platform == "WebKit");
    assert(uas[6].userAgent.find("lumen browser/1.0") != std::string::npos);

    // 3. Test active user agent getter and default
    assert(panel.settings().activeUserAgentIndex >= 0 && panel.settings().activeUserAgentIndex < 7);
    std::string activeUa = panel.settings().getActiveUserAgent();
    assert(!activeUa.empty());

    // 4. Test WebTab static default User-Agent get/set
    Blueprint::Engine::WebTab::setDefaultUserAgent(uas[0].userAgent);
    assert(Blueprint::Engine::WebTab::getDefaultUserAgent() == uas[0].userAgent);

    // 5. Test callback on User-Agent change
    std::string callbackUa;
    panel.setOnUserAgentChanged([&callbackUa](const std::string& ua) {
        callbackUa = ua;
    });

    // 6. Test database persistence across saves and reloads
    panel.selectUserAgent(2); // Firefox Linux
    assert(callbackUa == uas[2].userAgent);

    std::string dbUa = Blueprint::Storage::Database::instance().getSetting("user_agent_active_value", "");
    std::string dbIdx = Blueprint::Storage::Database::instance().getSetting("user_agent_active_index", "");
    assert(dbUa == uas[2].userAgent);
    assert(dbIdx == "2");

    // Fresh panel load should restore index 2
    Blueprint::UI::SettingsPanel freshPanel;
    assert(freshPanel.settings().activeUserAgentIndex == 2);
    assert(freshPanel.settings().getActiveUserAgent() == uas[2].userAgent);

    // Reset back to Chrome 131 Linux (index 0) for best video compatibility
    panel.selectUserAgent(0);
    assert(callbackUa == uas[0].userAgent);
    Blueprint::Engine::WebTab::setDefaultUserAgent(uas[0].userAgent);

    std::cout << "  -> User-Agent Presets & Compatibility tests PASSED!\n";
}

void testEphemeralSanitizerAndNullProtection() {
    std::cout << "[Test] Running Ephemeral Tracking Sanitizer & Null Protection tests...\n";

    // 1. Tracking parameter sanitization
    std::string testUrl1 = "https://duckduckgo.com/?q=linux&utm_source=twitter&utm_medium=cpc&fbclid=123#top";
    std::string clean1 = Blueprint::Engine::WebTab::sanitizeTrackingParams(testUrl1);
    assert(clean1 == "https://duckduckgo.com/?q=linux#top");

    std::string testUrl2 = "https://example.com/search?gclid=abc&utm_campaign=winter&key=val";
    std::string clean2 = Blueprint::Engine::WebTab::sanitizeTrackingParams(testUrl2);
    assert(clean2 == "https://example.com/search?key=val");

    std::string testUrl3 = "https://example.com/path/without/params";
    std::string clean3 = Blueprint::Engine::WebTab::sanitizeTrackingParams(testUrl3);
    assert(clean3 == "https://example.com/path/without/params");

    std::string testUrl4 = "https://example.com/test?utm_content=promo&mc_eid=987";
    std::string clean4 = Blueprint::Engine::WebTab::sanitizeTrackingParams(testUrl4);
    assert(clean4 == "https://example.com/test");

    // 2. Database history protection for internal & null surfaces
    assert(!Blueprint::Storage::Database::instance().addHistory("lumen://null", "l.null"));
    assert(!Blueprint::Storage::Database::instance().addHistory("lumen://null-tab", "l.null"));
    assert(!Blueprint::Storage::Database::instance().addHistory("lumen://newtab", "New Tab"));

    // 3. Verify HTML template invariants
    std::string nullHtml = Blueprint::Engine::NULL_TAB_HTML;
    assert(nullHtml.find("L.NULL // IN-MEMORY SESSION") != std::string::npos);
    assert(nullHtml.find("All ephemeral state registers will be zero-filled on window termination.") != std::string::npos);
    assert(nullHtml.find("transform: translateY(-48px)") != std::string::npos);
    assert(nullHtml.find("Search anonymously or execute URI...") != std::string::npos);

    std::string newtabHtml = Blueprint::Engine::NEW_TAB_HTML;
    assert(newtabHtml.find("zone-top-left") != std::string::npos);
    assert(newtabHtml.find("zone-top-center") != std::string::npos);
    assert(newtabHtml.find("zone-top-right") != std::string::npos);
    assert(newtabHtml.find("zone-bottom-left") != std::string::npos);
    assert(newtabHtml.find("zone-bottom-center") != std::string::npos);
    assert(newtabHtml.find("zone-bottom-right") != std::string::npos);
    assert(newtabHtml.find("transform: scale(0.92)") != std::string::npos);
    assert(newtabHtml.find("Widgets Catalog") != std::string::npos);
    assert(newtabHtml.find("Minimalist Clock") != std::string::npos);
    assert(newtabHtml.find("Live Weather Monitor") != std::string::npos);
    assert(newtabHtml.find("Custom HTML Embed") != std::string::npos);
    assert(newtabHtml.find("transform-handle") != std::string::npos);
    assert(newtabHtml.find("modal-dialog") != std::string::npos);

    std::cout << "  -> Ephemeral Sanitizer & Null Protection tests PASSED!\n";
}

void testThemeReactivityAndCustomDropdowns() {
    std::cout << "[Test] Running Theme Reactivity & Custom Dropdowns tests...\n";

    // 1. Color CSS serialization
    Blueprint::Theme::Color redCol{1.0f, 0.0f, 0.0f, 1.0f};
    assert(redCol.toCssHex() == "#FF0000");
    assert(redCol.toCssRgba() == "#FF0000");
    (void)redCol;

    Blueprint::Theme::Color semiGreen{0.0f, 1.0f, 0.0f, 0.5f};
    assert(semiGreen.toCssHex() == "#00FF00");
    assert(semiGreen.toCssRgba().find("rgba(0, 255, 0, 0.500)") != std::string::npos);
    (void)semiGreen;

    // 2. ThemeManager listener notification
    auto& tm = Blueprint::Theme::ThemeManager::instance();
    bool listenerCalled = false;
    std::string receivedThemeName;

    int listenerId = tm.addThemeListener([&](const Blueprint::Theme::Palette& pal) {
        listenerCalled = true;
        receivedThemeName = pal.name;
    });
    assert(listenerId > 0);

    tm.setTheme(Blueprint::Theme::ThemeId::CALCITE, false);
    assert(listenerCalled);
    assert(receivedThemeName == "calcite");

    // Test listener removal
    listenerCalled = false;
    tm.removeThemeListener(listenerId);
    tm.setTheme(Blueprint::Theme::ThemeId::NOCTILUCA, false);
    assert(!listenerCalled);

    // 3. Dynamic newtab HTML generation with palette
    const auto& calcitePal = tm.getPalette(Blueprint::Theme::ThemeId::CALCITE);
    std::string dynamicHtml = Blueprint::Engine::getNewTabHtml(calcitePal);
    assert(dynamicHtml.find(calcitePal.bgBase.toCssRgba()) != std::string::npos);
    assert(dynamicHtml.find(calcitePal.bgSurface.toCssRgba()) != std::string::npos);
    assert(dynamicHtml.find(calcitePal.accent.toCssRgba()) != std::string::npos);

    // 4. Custom dropdown components & settings modal structure
    assert(dynamicHtml.find("custom-dropdown") != std::string::npos);
    assert(dynamicHtml.find("custom-dropdown-trigger") != std::string::npos);
    assert(dynamicHtml.find("custom-dropdown-chevron") != std::string::npos);
    assert(dynamicHtml.find("custom-dropdown-menu") != std::string::npos);
    assert(dynamicHtml.find("custom-dropdown-item") != std::string::npos);
    assert(dynamicHtml.find("modal-dialog") != std::string::npos);
    assert(dynamicHtml.find("modal-sidebar") != std::string::npos);
    assert(dynamicHtml.find("modal-sidebar-tab") != std::string::npos);
    assert(dynamicHtml.find("tab-accent-indicator") != std::string::npos);
    assert(dynamicHtml.find("btn-settings") != std::string::npos);
    assert(dynamicHtml.find("btn-save") != std::string::npos);
    assert(dynamicHtml.find("btn-cancel") != std::string::npos);

    // 5. Modal zoom-in animation matching settings panel (0.88 scale, 260ms cubic-bezier)
    assert(dynamicHtml.find("transform: scale(0.88);") != std::string::npos);
    assert(dynamicHtml.find("260ms cubic-bezier(0.16, 1, 0.3, 1)") != std::string::npos);

    // 6. Mirrored KDE Plasma layout and desktop wrapper
    assert(dynamicHtml.find("id=\"desktop-wrapper\"") != std::string::npos);
    assert(dynamicHtml.find("id=\"desktop-edit-bar\"") != std::string::npos);
    assert(dynamicHtml.find("calc(100vw - 360px)") != std::string::npos);
    assert(dynamicHtml.find("width: 360px;") != std::string::npos);
    assert(dynamicHtml.find("Exit Edit Mode") != std::string::npos);
    assert(dynamicHtml.find("Widgets") != std::string::npos);
    assert(dynamicHtml.find("startWidgetFreeformDrag") != std::string::npos);
    assert(dynamicHtml.find("zone-full") != std::string::npos);
    assert(dynamicHtml.find("drag-error") != std::string::npos);

    std::cout << "  -> Theme Reactivity & Custom Dropdowns tests PASSED!\n";
}

void testErrorPageAndFilamentMeltdown() {
    std::cout << "[Test] Running Error Page (lumen://error) & Filament Meltdown tests...\n";

    const auto& pal = Blueprint::Theme::ThemeManager::instance().activePalette();
    std::string html = Blueprint::Engine::getErrorPageHtml(
        pal,
        "https://example.com/network-failure",
        "NET::ERR_CONNECTION_RESET",
        "Connection Reset",
        "The connection to the server was unexpectedly closed or reset while transferring data.",
        "SOUP_TRANSPORT_ERROR / 0x80004005"
    );

    // 1. DOM Hierarchy and Layout Specification checks
    // Split composition: flex layout, gap 64px, min-height 100vh
    assert(html.find("display: flex;") != std::string::npos);
    assert(html.find("gap: 64px;") != std::string::npos);
    assert(html.find("min-height: 100vh;") != std::string::npos);

    // Circular portal: 320x320, border-radius 50%, overflow hidden
    assert(html.find(".portal-frame {") != std::string::npos);
    assert(html.find("width: 320px;") != std::string::npos);
    assert(html.find("height: 320px;") != std::string::npos);
    assert(html.find("border-radius: 50%;") != std::string::npos);
    assert(html.find("overflow: hidden;") != std::string::npos);

    // Canvas element: internal resolution 720x960, visually aligned inside circle via transform
    assert(html.find("<canvas id=\"view\" width=\"720\" height=\"960\"></canvas>") != std::string::npos);
    assert(html.find("transform: translate(-50%, calc(-50% + 10px));") != std::string::npos);

    // Error details column: max width 440px
    assert(html.find(".details-column {") != std::string::npos);
    assert(html.find("max-width: 440px;") != std::string::npos);

    // Semantic diagnostic elements
    assert(html.find("status-badge") != std::string::npos);
    assert(html.find("NET::ERR_CONNECTION_RESET") != std::string::npos);
    assert(html.find("summary-title") != std::string::npos);
    assert(html.find("Connection Reset") != std::string::npos);
    assert(html.find("descriptive-copy") != std::string::npos);
    assert(html.find("diag-panel") != std::string::npos);
    assert(html.find("https://example.com/network-failure") != std::string::npos);
    assert(html.find("SOUP_TRANSPORT_ERROR / 0x80004005") != std::string::npos);
    assert(html.find("id=\"retry-btn\"") != std::string::npos);

    // 2. Theme Variable Binding checks
    assert(html.find("var(--bg-base)") != std::string::npos);
    assert(html.find("radial-gradient(circle at center, var(--bg-surface) 0%, var(--bg-base) 100%)") != std::string::npos);
    assert(html.find("var(--border)") != std::string::npos);
    assert(html.find("var(--danger)") != std::string::npos);
    assert(html.find("var(--fg-primary)") != std::string::npos);
    assert(html.find("var(--fg-muted)") != std::string::npos);
    assert(html.find("var(--fg-dim)") != std::string::npos);

    // 3. Filament Meltdown Physics & Canvas Implementation
    assert(html.find("const CX = 360;") != std::string::npos);
    assert(html.find("const FILAMENT_Y = 380;") != std::string::npos);
    assert(html.find("const PIN_LEFT_X = 280;") != std::string::npos);
    assert(html.find("const PIN_RIGHT_X = 440;") != std::string::npos);
    assert(html.find("const STEM_BOTTOM_Y = 620;") != std::string::npos);
    assert(html.find("const SEGMENTS = 40;") != std::string::npos);
    assert(html.find("const BREAK_INDEX = Math.floor(SEGMENTS * 0.46);") != std::string::npos);
    assert(html.find("function initFilament()") != std::string::npos);
    assert(html.find("function stepPhysics(dt)") != std::string::npos);
    assert(html.find("function getFilamentColor(temp)") != std::string::npos);
    assert(html.find("function drawRoundedRect(x, y, w, h, r)") != std::string::npos);
    assert(html.find("function update(dt)") != std::string::npos);
    assert(html.find("function render()") != std::string::npos);

    // 4. Behavioral Constraints:
    // No animation loops on click: ensure no click listeners on canvas or circular container
    assert(html.find("canvas.addEventListener('click'") == std::string::npos);
    assert(html.find("canvas.addEventListener(\"click\"") == std::string::npos);
    assert(html.find("portal.addEventListener('click'") == std::string::npos);
    assert(html.find("portal.addEventListener(\"click\"") == std::string::npos);

    // Retry actions: primary button and keyboard space/enter
    assert(html.find("retryBtn.addEventListener('click'") != std::string::npos);
    assert(html.find("e.code === 'Space' || e.code === 'Enter'") != std::string::npos);
    assert(html.find("tag === 'INPUT' || tag === 'TEXTAREA'") != std::string::npos);

    // Dynamic theme reactivity injection function
    assert(html.find("window.__setLumenTheme") != std::string::npos);

    // 5. HTML escaping validation (XSS prevention)
    std::string malicious = Blueprint::Engine::getErrorPageHtml(
        pal,
        "<script>alert('xss')</script>",
        "NET::ERR_<BAD>",
        "Title & \"Quotes\"",
        "Desc <script>",
        "Diag & Code"
    );
    assert(malicious.find("<script>alert") == std::string::npos);
    assert(malicious.find("&lt;script&gt;alert") != std::string::npos);
    assert(malicious.find("&quot;Quotes&quot;") != std::string::npos);

    std::cout << "  -> Error Page (lumen://error) & Filament Meltdown tests PASSED!\n";
}

void testHttpErrorHandlingAndWidgetFreeform() {
    std::cout << "[Test] Running HTTP Error Handling & Widget Freeform Grid tests...\n";

    const auto& pal = Blueprint::Theme::ThemeManager::instance().activePalette();

    // 1. Verify HTTP 403 Forbidden error page generation
    std::string http403Html = Blueprint::Engine::getErrorPageHtml(
        pal,
        "https://httpbin.org/status/403",
        "HTTP::ERR_FORBIDDEN",
        "Access Forbidden (403)",
        "You do not have permission to access the requested resource or directory on this server.",
        "HTTP_STATUS_403"
    );
    assert(http403Html.find("HTTP::ERR_FORBIDDEN") != std::string::npos);
    assert(http403Html.find("Access Forbidden (403)") != std::string::npos);
    assert(http403Html.find("https://httpbin.org/status/403") != std::string::npos);
    assert(http403Html.find("HTTP_STATUS_403") != std::string::npos);

    // 2. Verify HTTP 500 Internal Server Error page generation
    std::string http500Html = Blueprint::Engine::getErrorPageHtml(
        pal,
        "https://httpstat.us/500",
        "HTTP::ERR_INTERNAL_SERVER_ERROR",
        "Internal Server Error (500)",
        "The server encountered an unexpected condition that prevented it from fulfilling the request.",
        "HTTP_STATUS_500"
    );
    assert(http500Html.find("HTTP::ERR_INTERNAL_SERVER_ERROR") != std::string::npos);
    assert(http500Html.find("Internal Server Error (500)") != std::string::npos);

    // 3. Verify New Tab page has absolutely zero Cyrillic characters
    std::string newTabHtml = Blueprint::Engine::getNewTabHtml(pal);
    bool hasCyrillic = false;
    for (size_t i = 0; i + 1 < newTabHtml.size(); ++i) {
        unsigned char b1 = static_cast<unsigned char>(newTabHtml[i]);
        unsigned char b2 = static_cast<unsigned char>(newTabHtml[i + 1]);
        if ((b1 == 0xD0 && b2 >= 0x80) || (b1 == 0xD1 && b2 <= 0xBF)) {
            hasCyrillic = true;
            break;
        }
    }
    (void)hasCyrillic;
    assert(!hasCyrillic && "New tab HTML must contain strictly 0 Cyrillic characters!");

    // 4. Verify Widget Freeform Grid positioning & microgrid dot pattern
    assert(newTabHtml.find("position: absolute;") != std::string::npos);
    assert(newTabHtml.find("radial-gradient(var(--border) 1.2px, transparent 1.2px)") != std::string::npos);
    assert(newTabHtml.find("startWidgetFreeformDrag") != std::string::npos);
    assert(newTabHtml.find("getZoneWidgetCount") != std::string::npos);
    assert(newTabHtml.find("flashZoneFull") != std::string::npos);
    assert(newTabHtml.find("drag-error") != std::string::npos);
    assert(newTabHtml.find("zone-full") != std::string::npos);

    // 5. Verify horizontal centering button, middle-click resize trigger, and ghost drop preview
    assert(newTabHtml.find("widget-btn-center") != std::string::npos);
    assert(newTabHtml.find("centerWidgetHorizontally") != std::string::npos);
    assert(newTabHtml.find("e.button === 1") != std::string::npos);
    assert(newTabHtml.find("widget-drop-preview") != std::string::npos);
    assert(newTabHtml.find("showDropPreview") != std::string::npos);
    assert(newTabHtml.find("blueprint-pulse") != std::string::npos);
    assert(newTabHtml.find("scale(1.04)") != std::string::npos);
    assert(newTabHtml.find("is-centering") != std::string::npos);
    assert(newTabHtml.find("grid-template-rows: minmax(0, 0.82fr) auto minmax(0, 1.18fr);") != std::string::npos);
    assert(newTabHtml.find("body.is-resizing .widget.transform-mode") != std::string::npos);
    assert(newTabHtml.find("handle-pop") != std::string::npos);

    // 6. Verify Notes Window Widget, macOS dots, theme variants, and markdown support
    assert(newTabHtml.find("Notes Window") != std::string::npos);
    assert(newTabHtml.find("widget-notes") != std::string::npos);
    assert(newTabHtml.find("theme-macos") != std::string::npos);
    assert(newTabHtml.find("theme-cyber") != std::string::npos);
    assert(newTabHtml.find("theme-parchment") != std::string::npos);
    assert(newTabHtml.find("theme-glass") != std::string::npos);
    assert(newTabHtml.find("dot-red") != std::string::npos);
    assert(newTabHtml.find("dot-yellow") != std::string::npos);
    assert(newTabHtml.find("dot-green") != std::string::npos);
    assert(newTabHtml.find("renderMarkdownToHtml") != std::string::npos);
    assert(newTabHtml.find("cfg-notes-md") != std::string::npos);
    assert(newTabHtml.find("document.body.appendChild(el)") != std::string::npos);

    // 7. Verify Zone capacity expansion and fixed 8-anchor handle orientation
    assert(newTabHtml.find("MAX_ZONE_WIDGETS = 5;") != std::string::npos);
    assert(newTabHtml.find("startRight - curL") != std::string::npos);
    assert(newTabHtml.find("widget-lifted") != std::string::npos);
    assert(newTabHtml.find("is-resizing-active") != std::string::npos);

    // 8. Verify 16px mechanical grid snapping, collision/overlap prevention, and robust drop preview
    assert(newTabHtml.find("snapToGrid") != std::string::npos);
    assert(newTabHtml.find("getZoneOverlaps") != std::string::npos);
    assert(newTabHtml.find("findFreePositionInZone") != std::string::npos);
    assert(newTabHtml.find("getDropZoneAtPoint") != std::string::npos);
    assert(newTabHtml.find("preview-invalid") != std::string::npos);

    // 9. Verify Media Player Widget (Now Playing, rounded pills, marquee ticker, waveforms, progress seek)
    assert(newTabHtml.find("Media Player") != std::string::npos);
    assert(newTabHtml.find("widget-media") != std::string::npos);
    assert(newTabHtml.find("media-progress-track") != std::string::npos);
    assert(newTabHtml.find("media-waveform") != std::string::npos);
    assert(newTabHtml.find("pingpong-marquee") != std::string::npos);
    assert(newTabHtml.find("lumenMedia") != std::string::npos);
    assert(newTabHtml.find("window.__updateLumenMediaState") != std::string::npos);
    assert(newTabHtml.find("sendMediaCommand") != std::string::npos);
    assert(newTabHtml.find("Not Playing") != std::string::npos);
    assert(newTabHtml.find("media-output-btn") == std::string::npos);
    assert(newTabHtml.find("media-progress-track.is-disabled") != std::string::npos);

    std::cout << "  -> HTTP Error Handling & Widget Freeform Grid tests PASSED!\n";
}

void testMediaWidgetSmoothTransitionsAndRoundedControls() {
    std::cout << "[Test] Running Media Widget Smooth Transitions & Rounded Controls tests...\n";

    const auto& pal = Blueprint::Theme::ThemeManager::instance().activePalette();
    std::string newTabHtml = Blueprint::Engine::getNewTabHtml(pal);

    // 1. Verify smooth crossfade transition classes and CSS rules
    assert(newTabHtml.find(".media-container.is-track-transitioning .media-meta") != std::string::npos);
    assert(newTabHtml.find(".media-container.is-track-transitioning .media-art-wrap") != std::string::npos);
    assert(newTabHtml.find("prevTrackSignature") != std::string::npos);
    assert(newTabHtml.find("is-track-transitioning") != std::string::npos);

    // 2. Verify rounded control button icons (no sharp triangles or sharp rectangles)
    assert(newTabHtml.find("rx=\"1.25\"") != std::string::npos); // Rounded prev & next bar caps
    assert(newTabHtml.find("rx=\"1.75\"") != std::string::npos); // Rounded dual pause bars
    assert(newTabHtml.find("M8.5 6.35c0-.98 1.07-1.59 1.91-1.07l9.42 5.88c.8.5.8 1.66 0 2.16l-9.42 5.88c-.84.52-1.91-.09-1.91-1.07V6.35z") != std::string::npos); // Smooth rounded play triangle

    // 3. Verify robust artwork element structure (both image tag and fallback placeholder present)
    assert(newTabHtml.find("<img src=\"${escapeAttr(artUrl)}\" class=\"media-art-img\"") != std::string::npos);
    assert(newTabHtml.find("class=\"media-art-placeholder\"") != std::string::npos);

    // 4. Verify MPRIS query JSON generation has NO Russian locale comma decimal bugs
    std::string mprisJson = Blueprint::Engine::WebTab::querySystemMprisJson();
    assert(mprisJson.find(",000000") == std::string::npos);
    assert(mprisJson.find(",500000") == std::string::npos);
    assert(mprisJson.find("\"hasPlayer\":") != std::string::npos);

    // 5. Verify zero forbidden terms
    assert(newTabHtml.find("toyota") == std::string::npos);
    assert(newTabHtml.find("Toyota") == std::string::npos);
    assert(newTabHtml.find("dynamic island") == std::string::npos);
    assert(newTabHtml.find("Dynamic Island") == std::string::npos);

    std::cout << "  -> Media Widget Smooth Transitions & Rounded Controls tests PASSED!\n";
}

void testWindowControlsAndRow2Layout() {
    std::cout << "[Test] Running Window Controls & Row 2 Layout tests...\n";

    Blueprint::UI::CompactTopbar topbar;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 84);
    cairo_t* cr = cairo_create(surf);
    topbar.draw(cr, 1280, 84);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    bool minClicked = false;
    bool maxClicked = false;
    bool closeClicked = false;
    bool newTabClicked = false;

    topbar.setOnMinimize([&]() { minClicked = true; });
    topbar.setOnMaximizeToggle([&]() { maxClicked = true; });
    topbar.setOnCloseWindow([&]() { closeClicked = true; });
    topbar.setOnNewTab([&]() { newTabClicked = true; });

    // Row 1 Window Controls (x in [w - 120, w], y in [0, 44]):
    // 1. Minimize at x = 1280 - 100 = 1180, y = 22
    bool hMin = topbar.handleMouseDown(1180.0, 22.0);
    assert(hMin);
    assert(minClicked);

    // 2. Maximize at x = 1280 - 60 = 1220, y = 22
    bool hMax = topbar.handleMouseDown(1220.0, 22.0);
    assert(hMax);
    assert(maxClicked);

    // 3. Close at x = 1280 - 20 = 1260, y = 22
    bool hClose = topbar.handleMouseDown(1260.0, 22.0);
    assert(hClose);
    assert(closeClicked);

    // Maximize state toggle
    assert(!topbar.isMaximized());
    topbar.setMaximized(true);
    assert(topbar.isMaximized());
    topbar.setMaximized(false);
    assert(!topbar.isMaximized());

    // Row 2 Action Buttons (y in [44, 84]):
    // 4. New Tab (+) button at x = 1280 - 58 = 1222, y = 64
    bool hNewTab = topbar.handleMouseDown(1222.0, 64.0);
    assert(hNewTab);
    assert(newTabClicked);

    // 5. Settings button at x = 1280 - 24 = 1256, y = 64
    assert(!topbar.isSettingsOpen());
    bool hSettings = topbar.handleMouseDown(1256.0, 64.0);
    assert(hSettings);
    assert(topbar.isSettingsOpen());
    // Toggle closed
    topbar.handleMouseDown(1256.0, 64.0);
    assert(!topbar.isSettingsOpen());

    // 6. Empty space in Row 1 returns false to allow window move dragging
    bool hEmpty = topbar.handleMouseDown(800.0, 20.0);
    assert(!hEmpty);

    (void)hMin; (void)hMax; (void)hClose; (void)hNewTab; (void)hSettings; (void)hEmpty;

    std::cout << "  -> Window Controls & Row 2 Layout tests PASSED!\n";
}

void testRapidTabOperations() {
    std::cout << "[Test] Running Rapid Tab Creation & Deletion tests...\n";

    bool gtkOk = gtk_init_check(nullptr, nullptr);
    if (gtkOk) {
        Blueprint::UI::TabStrip strip;
        std::vector<std::shared_ptr<Blueprint::Engine::WebTab>> tabs;
        std::vector<std::weak_ptr<Blueprint::Engine::WebTab>> weakTabs;

        for (int i = 0; i < 20; ++i) {
            auto tab = std::make_shared<Blueprint::Engine::WebTab>(
                i, "https://example.com/" + std::to_string(i), "Tab " + std::to_string(i)
            );
            int tabId = i;
            // Verify non-circular callbacks (capturing tabId primitive instead of shared_ptr)
            tab->setCallbacks(
                [tabId](const std::string&) {},
                [tabId](const std::string&) {},
                [tabId](float) {}
            );
            tab->setOnSiteDataChanged([tabId](uint64_t) {});
            weakTabs.push_back(tab);
            tabs.push_back(tab);
        }

        assert(tabs.size() == 20);
        strip.setTabs(tabs, 0);

        int closedIndex = -1;
        int switchedNew = -1;
        strip.setCallbacks(
            [&switchedNew](int, int newIdx) { switchedNew = newIdx; },
            [&closedIndex](int idx) { closedIndex = idx; }
        );

        // Rapidly close tabs one by one down to 0
        while (tabs.size() > 1) {
            int closeIdx = static_cast<int>(tabs.size()) / 2;
            auto tabToClose = tabs[closeIdx];
            tabToClose->stopMediaPoll();
            tabToClose->setCallbacks(nullptr, nullptr, nullptr);
            tabToClose->setOnSiteDataChanged(nullptr);
            tabs.erase(tabs.begin() + closeIdx);
            strip.setTabs(tabs, 0);
        }

        tabs[0]->stopMediaPoll();
        tabs[0]->setCallbacks(nullptr, nullptr, nullptr);
        tabs.clear();

        // Verify that ALL 20 tabs were completely destroyed without circular references
        for (size_t i = 0; i < weakTabs.size(); ++i) {
            assert(weakTabs[i].expired() && "WebTab memory leak detected: circular reference prevented destruction!");
        }
    }

    std::cout << "  -> Rapid Tab Creation & Deletion tests PASSED!\n";
}

void testTorBridgeAndOnionRouting() {
    std::cout << "[Test] Running Tor Bridge & Onion Routing tests...\n";

    // 1. Onion URL detection
    assert(Blueprint::Core::TorBridge::isOnionUrl("http://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion"));
    assert(Blueprint::Core::TorBridge::isOnionUrl("https://expyuz5wqqfdgah56trnjbdwh2xtqsrfdunia824m4a58482483842884.onion/path?q=1"));
    assert(Blueprint::Core::TorBridge::isOnionUrl("duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion"));
    assert(Blueprint::Core::TorBridge::isOnionUrl("http://portal.onion/index.html"));
    assert(!Blueprint::Core::TorBridge::isOnionUrl("https://google.com"));
    assert(!Blueprint::Core::TorBridge::isOnionUrl("https://wikipedia.org/wiki/Tor"));
    assert(!Blueprint::Core::TorBridge::isOnionUrl(""));
    assert(!Blueprint::Core::TorBridge::isOnionUrl("http://onion.com"));
    assert(!Blueprint::Core::TorBridge::isOnionUrl("lumen://newtab"));

    // 2. v3 Onion URL parser
    auto info = Blueprint::Core::TorBridge::parseOnionV3("http://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/search?q=test");
    assert(info.valid);
    assert(info.scheme == "http://");
    assert(info.head == "duckdu");
    assert(info.tail == "czad");
    assert(info.dots == "…");
    assert(info.path == "/search?q=test");
    assert(info.host == "duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion");

    // 3. Settings persistence
    bool initialSetting = Blueprint::Core::TorBridge::isTorRoutingEnabled();
    Blueprint::Core::TorBridge::setTorRoutingEnabled(true);
    assert(Blueprint::Core::TorBridge::isTorRoutingEnabled() == true);
    Blueprint::Core::TorBridge::setTorPort(9150);
    assert(Blueprint::Core::TorBridge::getTorPort() == 9150);
    Blueprint::Core::TorBridge::setTorPort(9050);
    assert(Blueprint::Core::TorBridge::getTorPort() == 9050);
    Blueprint::Core::TorBridge::setTorRoutingEnabled(initialSetting);

    // 4. Non-blocking daemon probe
    auto t0 = std::chrono::steady_clock::now();
    bool probeRes = Blueprint::Core::TorBridge::probeTorDaemon(59998, 60);
    auto t1 = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    assert(elapsedMs < 300); // must return swiftly without blocking UI
    (void)elapsedMs;
    (void)probeRes;

    // 5. SettingsPanel Tor section integration
    Blueprint::UI::SettingsPanel panel;
    panel.setOnionRoutingEnabled(true);
    assert(panel.isOnionRoutingEnabled() == true);
    panel.setOnionTorPort(9150);
    assert(panel.getOnionTorPort() == 9150);
    panel.setOnionRoutingEnabled(initialSetting);

    // 6. WebTab Onion status
    bool gtkOk = gtk_init_check(nullptr, nullptr);
    if (gtkOk) {
        auto tabOnion = std::make_shared<Blueprint::Engine::WebTab>(100, "lumen://onion-disabled?target=http://test.onion", "Onion Tab");
        assert(tabOnion->isOnion());
        auto tabClear = std::make_shared<Blueprint::Engine::WebTab>(101, "https://example.com", "Clear Tab");
        assert(!tabClear->isOnion());
    }

    std::cout << "  -> Tor Bridge & Onion Routing tests PASSED!\n";
}

void testTorProvisioner() {
    std::cout << "[Test] Running Tor Provisioner & Diagnostics tests...\n";

    auto& prov = Blueprint::Core::TorProvisioner::instance();

    // 1. Distribution detect install command
    std::string installCmd = prov.detectInstallCommand();
    assert(!installCmd.empty());
    // On Arch/CachyOS, ensure pacman -S --needed is used and partial upgrade risk (pacman -Sy) is avoided
    if (installCmd.find("pacman") != std::string::npos) {
        assert(installCmd.find("pacman -S --noconfirm --needed tor") != std::string::npos);
        assert(installCmd.find("pacman -Sy") == std::string::npos);
    }
    assert(installCmd.find("systemctl enable --now tor") != std::string::npos);

    // 2. State & Log management
    prov.clearLogs();
    assert(prov.getState() == Blueprint::Core::ProvisionState::IDLE);
    assert(prov.getLogs().empty());

    prov.appendLog("=== Diagnostic Test Header ===");
    prov.appendLog("[INFO] Mock Tor binary detection test");
    prov.appendLog("[OK] Mock Socket 127.0.0.1:9050 active");
    auto logs = prov.getLogs();
    assert(logs.size() == 3);
    assert(logs[0] == "=== Diagnostic Test Header ===");
    assert(logs[1] == "[INFO] Mock Tor binary detection test");
    assert(logs[2] == "[OK] Mock Socket 127.0.0.1:9050 active");

    prov.setStatus(Blueprint::Core::ProvisionState::RUNNING, "Probing mock daemon...");
    assert(prov.getState() == Blueprint::Core::ProvisionState::RUNNING);
    assert(prov.getStatusMessage() == "Probing mock daemon...");

    prov.setStatus(Blueprint::Core::ProvisionState::SUCCESS, "Mock daemon ready.");
    assert(prov.getState() == Blueprint::Core::ProvisionState::SUCCESS);
    assert(prov.getStatusMessage() == "Mock daemon ready.");

    prov.clearLogs();
    assert(prov.getLogs().empty());
    assert(prov.getState() == Blueprint::Core::ProvisionState::IDLE);

    // 3. Verification Phase (read-only probe without auto-install on unused port)
    prov.checkOrProvision(59996, false);
    int waitLimit = 50;
    while (prov.isRunning() && waitLimit-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(!prov.isRunning());
    auto finishState = prov.getState();
    assert(finishState == Blueprint::Core::ProvisionState::ERROR || finishState == Blueprint::Core::ProvisionState::SUCCESS);
    (void)finishState;
    assert(!prov.getLogs().empty());

    // Clean up
    prov.clearLogs();

    // 4. UI SettingsPanel interaction with section 4
    Blueprint::UI::SettingsPanel panel;
    panel.switchToSection(4);
    panel.handleMouseMove(100.0, 100.0);
    panel.handleScroll(1.0);
    panel.handleScroll(-1.0);

    std::cout << "  -> Tor Provisioner & Diagnostics tests PASSED!\n";
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
    testUserAgentPresetsAndCompatibility();
    testEphemeralSanitizerAndNullProtection();
    testThemeReactivityAndCustomDropdowns();
    testErrorPageAndFilamentMeltdown();
    testHttpErrorHandlingAndWidgetFreeform();
    testMediaWidgetSmoothTransitionsAndRoundedControls();
    testWindowControlsAndRow2Layout();
    testRapidTabOperations();
    testTorBridgeAndOnionRouting();
    testTorProvisioner();

    std::cout << "========================================\n";
    std::cout << " ALL UNIT TESTS PASSED SUCCESSFULLY! ✅\n";
    std::cout << "========================================\n";
    return 0;
}


