#include "installer/installer_steps.hpp"
#include "core/tor_bridge.hpp"
#include "core/tor_provisioner.hpp"
#include <librsvg/rsvg.h>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace Blueprint::Installer {

static std::string resolveSvgPath(const std::string& filename) {
    const std::vector<std::string> prefixes = {
        "assets/search_engines/",
        "../assets/search_engines/",
        "/home/elliot/Проекты/Blueprint Browser/assets/search_engines/",
        "./assets/search_engines/"
    };
    for (const auto& p : prefixes) {
        std::string full = p + filename;
        if (access(full.c_str(), R_OK) == 0) {
            return full;
        }
    }
    return "assets/search_engines/" + filename;
}

InstallerSteps::InstallerSteps(GtkWindow* parentWindow)
    : m_parentWindow(parentWindow) {
    initThemes();
    initSearchEngines();

    m_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(m_container, 660, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_container), "installer-deck");

    m_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(m_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(m_stack), 200);
    gtk_box_pack_start(GTK_BOX(m_container), m_stack, TRUE, TRUE, 0);

    buildStep0Welcome();
    buildStep1Appearance();
    buildStep2Search();
    buildStep3Tor();
    buildStep4Deploy();

    setStep(0);
}

InstallerSteps::~InstallerSteps() {
    for (auto& cache : m_searchIconCaches) {
        if (cache.colorSurface) {
            cairo_surface_destroy(cache.colorSurface);
            cache.colorSurface = nullptr;
        }
        if (cache.grayscaleSurface) {
            cairo_surface_destroy(cache.grayscaleSurface);
            cache.grayscaleSurface = nullptr;
        }
    }
    m_searchIconCaches.clear();
}

void InstallerSteps::initThemes() {
    m_themes = {
        // 5 Dark Themes
        {
            "noctiluca", "Noctiluca", true,
            { "#0C1214", "#141D22", "#6CE5B9" },
            { 0, 0, 0 },
            "#0C1214", "#141D22", "#19252C", "#24333B",
            "#E1ECF0", "#6E8590", "#6CE5B9", "#6CE5B9", "#0C1214"
        },
        {
            "morion", "Morion", true,
            { "#131114", "#1D1920", "#DCA574" },
            { 0, 0, 0 },
            "#131114", "#1D1920", "#26202A", "#342D38",
            "#ECE4EB", "#98899A", "#DCA574", "#DCA574", "#131114"
        },
        {
            "scoria", "Scoria", true,
            { "#111012", "#1A181C", "#E5935C" },
            { 0, 0, 0 },
            "#111012", "#1A181C", "#221F25", "#2E2A33",
            "#E8E2E6", "#8E8594", "#E5935C", "#E5935C", "#111012"
        },
        {
            "stibnite", "Stibnite", true,
            { "#0E1014", "#161920", "#78A9FF" },
            { 0, 0, 0 },
            "#0E1014", "#161920", "#1D222B", "#262C38",
            "#E4E8F0", "#737E94", "#78A9FF", "#78A9FF", "#0E1014"
        },
        {
            "tephra", "Tephra", true,
            { "#121212", "#1C1C1C", "#9EC49E" },
            { 0, 0, 0 },
            "#121212", "#1C1C1C", "#242424", "#2E2E2E",
            "#DEDEDE", "#7A7A7A", "#9EC49E", "#9EC49E", "#121212"
        },
        // 5 Light Themes
        {
            "calcite", "Calcite", false,
            { "#F4F2EA", "#E8E5DB", "#3B685C" },
            { 0, 0, 0 },
            "#F4F2EA", "#E8E5DB", "#DCD7CB", "#D0CBBE",
            "#201E1A", "#858072", "#3B685C", "#201E1A", "#FFFFFF"
        },
        {
            "rime", "Rime", false,
            { "#ECF1F4", "#DFE6EB", "#2D638E" },
            { 0, 0, 0 },
            "#ECF1F4", "#DFE6EB", "#D1DCE3", "#C2CDD7",
            "#182129", "#728494", "#2D638E", "#182129", "#FFFFFF"
        },
        {
            "kaolin", "Kaolin", false,
            { "#F6F3ED", "#EBE6DC", "#B5543C" },
            { 0, 0, 0 },
            "#F6F3ED", "#EBE6DC", "#DED7C9", "#D4CDBF",
            "#24211D", "#80776B", "#B5543C", "#24211D", "#FFFFFF"
        },
        {
            "selenite", "Selenite", false,
            { "#F1F0F5", "#E5E3EC", "#6052A8" },
            { 0, 0, 0 },
            "#F1F0F5", "#E5E3EC", "#DBD8E5", "#CBCEDB",
            "#1F1C2B", "#77738A", "#6052A8", "#1F1C2B", "#FFFFFF"
        },
        {
            "loess", "Loess", false,
            { "#F5F1E6", "#E8E2D1", "#586E3F" },
            { 0, 0, 0 },
            "#F5F1E6", "#E8E2D1", "#DDD5BE", "#CFC7B0",
            "#26231A", "#827B68", "#586E3F", "#26231A", "#FFFFFF"
        }
    };

    for (auto& t : m_themes) {
        for (int j = 0; j < 3; ++j) {
            unsigned int c = 0;
            if (t.colors[j].size() >= 7 && t.colors[j][0] == '#') {
                if (sscanf(t.colors[j].c_str() + 1, "%x", &c) == 1) {
                    t.previewRgb[j] = c;
                }
            }
        }
    }

    m_config.themeName = m_themes[0].name;
    m_config.themeId = m_themes[0].id;
}

void InstallerSteps::precacheSearchIcons() {
    const int w = 28, h = 28;
    for (const auto& se : m_searchEngines) {
        SearchIconCache cache;
        std::string path = resolveSvgPath(se.iconFile);
        GError* err = nullptr;
        RsvgHandle* handle = rsvg_handle_new_from_file(path.c_str(), &err);
        if (handle) {
            // 1. Color surface
            cache.colorSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_t* cr = cairo_create(cache.colorSurface);
            RsvgRectangle viewport = { 0.0, 0.0, static_cast<double>(w), static_cast<double>(h) };
            rsvg_handle_render_document(handle, cr, &viewport, nullptr);
            cairo_destroy(cr);

            // 2. Grayscale surface
            cache.grayscaleSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_t* grayCr = cairo_create(cache.grayscaleSurface);
            cairo_set_source_surface(grayCr, cache.colorSurface, 0, 0);
            cairo_paint(grayCr);
            cairo_destroy(grayCr);

            cairo_surface_flush(cache.grayscaleSurface);
            unsigned char* raw = cairo_image_surface_get_data(cache.grayscaleSurface);
            int stride = cairo_image_surface_get_stride(cache.grayscaleSurface);
            for (int y = 0; y < h; ++y) {
                uint32_t* row = reinterpret_cast<uint32_t*>(raw + y * stride);
                for (int x = 0; x < w; ++x) {
                    uint32_t pixel = row[x];
                    uint8_t a = (pixel >> 24) & 0xFF;
                    if (a == 0) continue;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t b = pixel & 0xFF;
                    uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
                    uint8_t dimmedGray = static_cast<uint8_t>(gray * 0.70f);
                    uint8_t dimmedA = static_cast<uint8_t>(a * 0.60f);
                    row[x] = (dimmedA << 24) | (dimmedGray << 16) | (dimmedGray << 8) | dimmedGray;
                }
            }
            cairo_surface_mark_dirty(cache.grayscaleSurface);

            g_object_unref(handle);
        } else {
            if (err) g_error_free(err);
        }
        m_searchIconCaches.push_back(cache);
    }
}

void InstallerSteps::initSearchEngines() {
    m_searchEngines = {
        { "Brave Search", "Independent index, zero profiling", "https://search.brave.com/search?q=%s", "brave.svg" },
        { "DuckDuckGo", "Privacy standard, no tracking", "https://duckduckgo.com/?q=%s", "duckduckgo.svg" },
        { "Ecosia", "Eco-driven search infrastructure", "https://www.ecosia.org/search?q=%s", "ecosia.svg" },
        { "Startpage", "Google results with complete proxying", "https://www.startpage.com/sp/search?query=%s", "startpage.svg" },
        { "Bing", "Microsoft search network", "https://www.bing.com/search?q=%s", "bing.svg" },
        { "Google", "Standard commercial engine", "https://www.google.com/search?q=%s", "google.svg" }
    };

    m_config.searchEngineName = m_searchEngines[0].name;
    m_config.searchEngineUrl = m_searchEngines[0].urlTemplate;

    precacheSearchIcons();
}

void InstallerSteps::setStep(int step) {
    m_currentStep = step;
    std::string name = "step_" + std::to_string(step);
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), name.c_str());

    if (m_onStepChanged) {
        m_onStepChanged(step);
    }

    if (step == 3) {
        checkTorSocket(m_config.torPort);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 0: Welcome
// ─────────────────────────────────────────────────────────────────────────────
void InstallerSteps::buildStep0Welcome() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 56);
    gtk_widget_set_margin_end(box, 56);
    gtk_widget_set_margin_top(box, 70);
    gtk_widget_set_margin_bottom(box, 60);

    // Top Header Section
    GtkWidget* topBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_valign(topBox, GTK_ALIGN_START);

    GtkWidget* title = gtk_label_new("Welcome to lumen");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "deck-title-large");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(topBox), title, FALSE, FALSE, 0);

    GtkWidget* subtitle = gtk_label_new("A lightweight, deeply isolated browser engineered for speed and clarity.");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "deck-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(subtitle), 48);
    gtk_box_pack_start(GTK_BOX(topBox), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), topBox, FALSE, FALSE, 0);

    // Center spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(box), spacer, TRUE, TRUE, 0);

    // Bottom Action Buttons
    GtkWidget* bottomBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(bottomBox, GTK_ALIGN_END);

    GtkWidget* btnStart = gtk_button_new_with_label("Start Setup");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnStart), "deck-btn-primary");
    gtk_widget_set_size_request(btnStart, -1, 52);
    g_signal_connect(btnStart, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        self->setStep(1);
    }), this);
    gtk_box_pack_start(GTK_BOX(bottomBox), btnStart, FALSE, FALSE, 0);

    GtkWidget* btnCancel = gtk_button_new_with_label("Cancel Installation");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnCancel), "deck-btn-secondary");
    gtk_widget_set_size_request(btnCancel, -1, 40);
    g_signal_connect(btnCancel, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        if (self->m_onCancel) self->m_onCancel();
        else gtk_window_close(self->m_parentWindow);
    }), this);
    gtk_box_pack_start(GTK_BOX(bottomBox), btnCancel, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), bottomBox, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(m_stack), box, "step_0");
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 1: Appearance (Visual Profile)
// ─────────────────────────────────────────────────────────────────────────────
void InstallerSteps::buildStep1Appearance() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    gtk_widget_set_margin_top(box, 36);
    gtk_widget_set_margin_bottom(box, 28);

    // Title & Subtitle
    GtkWidget* title = gtk_label_new("Choose Theme");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "deck-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget* subtitle = gtk_label_new("Select an aesthetic profile. Dark palettes are listed first, followed by light modes.");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "deck-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_widget_set_margin_bottom(box, 14);
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 6);

    // Scrollable Theme Card Deck
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);

    GtkWidget* themeListBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(themeListBox, 4);
    gtk_widget_set_margin_bottom(themeListBox, 8);

    for (size_t i = 0; i < m_themes.size(); ++i) {
        const auto& t = m_themes[i];

        // Section separator between dark and light themes
        if (i == 5) {
            GtkWidget* sepBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_widget_set_margin_top(sepBox, 8);
            gtk_widget_set_margin_bottom(sepBox, 4);

            GtkWidget* sepLabel = gtk_label_new("LIGHT THEMES");
            gtk_style_context_add_class(gtk_widget_get_style_context(sepLabel), "deck-section-tag");
            gtk_label_set_xalign(GTK_LABEL(sepLabel), 0.0);
            gtk_box_pack_start(GTK_BOX(sepBox), sepLabel, FALSE, FALSE, 0);

            gtk_box_pack_start(GTK_BOX(themeListBox), sepBox, FALSE, FALSE, 0);
        } else if (i == 0) {
            GtkWidget* sepLabel = gtk_label_new("DARK THEMES");
            gtk_style_context_add_class(gtk_widget_get_style_context(sepLabel), "deck-section-tag");
            gtk_label_set_xalign(GTK_LABEL(sepLabel), 0.0);
            gtk_box_pack_start(GTK_BOX(themeListBox), sepLabel, FALSE, FALSE, 0);
        }

        // Theme card button
        GtkWidget* card = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(card), "theme-card");
        if (i == 0) {
            gtk_style_context_add_class(gtk_widget_get_style_context(card), "theme-card-active");
        }
        gtk_widget_set_size_request(card, -1, 48);

        GtkWidget* cardContent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(cardContent, 16);
        gtk_widget_set_margin_end(cardContent, 16);

        // Theme name
        GtkWidget* lblName = gtk_label_new(t.name.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(lblName), "theme-card-name");
        gtk_label_set_xalign(GTK_LABEL(lblName), 0.0);
        gtk_box_pack_start(GTK_BOX(cardContent), lblName, TRUE, TRUE, 0);

        // 3-circle preview palette
        GtkWidget* previewArea = gtk_drawing_area_new();
        gtk_widget_set_size_request(previewArea, 58, 20);
        gtk_widget_set_valign(previewArea, GTK_ALIGN_CENTER);

        // Store theme index
        g_object_set_data(G_OBJECT(previewArea), "theme_index", GINT_TO_POINTER(static_cast<int>(i)));

        g_signal_connect(previewArea, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer user_data) -> gboolean {
            auto* self = static_cast<InstallerSteps*>(user_data);
            int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "theme_index"));
            if (idx < 0 || idx >= static_cast<int>(self->m_themes.size())) return TRUE;
            const auto& t = self->m_themes[idx];

            auto drawCircle = [cr](double x, double y, double radius, uint32_t c) {
                double r = ((c >> 16) & 0xFF) / 255.0;
                double g = ((c >> 8) & 0xFF) / 255.0;
                double b = (c & 0xFF) / 255.0;
                cairo_set_source_rgb(cr, r, g, b);
                cairo_arc(cr, x, y, radius, 0, 2 * G_PI);
                cairo_fill_preserve(cr);
                cairo_set_source_rgba(cr, 0, 0, 0, 0.18);
                cairo_set_line_width(cr, 1.0);
                cairo_stroke(cr);
            };

            drawCircle(10, 10, 7, t.previewRgb[0]);
            drawCircle(28, 10, 7, t.previewRgb[1]);
            drawCircle(46, 10, 7, t.previewRgb[2]);
            return TRUE;
        }), this);

        gtk_box_pack_end(GTK_BOX(cardContent), previewArea, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(card), cardContent);

        // Click handler
        g_signal_connect(card, "clicked", G_CALLBACK(+[](GtkWidget* clickedWidget, gpointer user_data) {
            auto* self = static_cast<InstallerSteps*>(user_data);
            for (size_t idx = 0; idx < self->m_themeCards.size(); ++idx) {
                GtkStyleContext* sc = gtk_widget_get_style_context(self->m_themeCards[idx]);
                if (self->m_themeCards[idx] == clickedWidget) {
                    gtk_style_context_add_class(sc, "theme-card-active");
                    self->m_selectedThemeIndex = static_cast<int>(idx);
                    self->m_config.themeName = self->m_themes[idx].name;
                    self->m_config.themeId = self->m_themes[idx].id;
                    if (self->m_onThemeChanged) {
                        self->m_onThemeChanged(self->m_themes[idx]);
                    }
                } else {
                    gtk_style_context_remove_class(sc, "theme-card-active");
                }
            }
        }), this);

        m_themeCards.push_back(card);
        gtk_box_pack_start(GTK_BOX(themeListBox), card, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(scrolled), themeListBox);
    gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

    // Bottom Navigation
    GtkWidget* navBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(navBox, 16);

    GtkWidget* btnBack = gtk_button_new_with_label("Back");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnBack), "deck-btn-secondary");
    gtk_widget_set_size_request(btnBack, 100, 44);
    g_signal_connect(btnBack, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(0);
    }), this);
    gtk_box_pack_start(GTK_BOX(navBox), btnBack, FALSE, FALSE, 0);

    GtkWidget* btnContinue = gtk_button_new_with_label("Continue");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnContinue), "deck-btn-primary");
    gtk_widget_set_size_request(btnContinue, 130, 44);
    g_signal_connect(btnContinue, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(2);
    }), this);
    gtk_box_pack_end(GTK_BOX(navBox), btnContinue, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), navBox, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(m_stack), box, "step_1");
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2: Search Engine Selection
// ─────────────────────────────────────────────────────────────────────────────
void InstallerSteps::buildStep2Search() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 28);

    // Title & Subtitle
    GtkWidget* title = gtk_label_new("Default Search Engine");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "deck-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget* subtitle = gtk_label_new("Your daily privacy, tracking exposure, and search latency depend directly on your query node.");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "deck-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 8);

    // 2 columns x 3 rows Grid
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_widget_set_margin_top(grid, 18);
    gtk_widget_set_vexpand(grid, TRUE);

    for (size_t i = 0; i < m_searchEngines.size(); ++i) {
        const auto& se = m_searchEngines[i];

        GtkWidget* card = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(card), "search-card");
        if (i == 0) {
            gtk_style_context_add_class(gtk_widget_get_style_context(card), "search-card-active");
        }

        GtkWidget* cardBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(cardBox, 14);
        gtk_widget_set_margin_end(cardBox, 14);
        gtk_widget_set_margin_top(cardBox, 10);
        gtk_widget_set_margin_bottom(cardBox, 10);

        // Pre-cached offline SVG Logo (28x28)
        GtkWidget* iconArea = gtk_drawing_area_new();
        gtk_widget_set_size_request(iconArea, 28, 28);
        gtk_widget_set_valign(iconArea, GTK_ALIGN_CENTER);

        g_object_set_data(G_OBJECT(iconArea), "engine_index", GINT_TO_POINTER(static_cast<int>(i)));

        auto onIconDraw = +[](GtkWidget* widget, cairo_t* cr, gpointer user_data) -> gboolean {
            auto* self = static_cast<InstallerSteps*>(user_data);
            int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "engine_index"));
            if (idx >= 0 && idx < static_cast<int>(self->m_searchIconCaches.size())) {
                bool isSelected = (idx == self->m_selectedSearchIndex);
                cairo_surface_t* surf = isSelected ? self->m_searchIconCaches[idx].colorSurface
                                                   : self->m_searchIconCaches[idx].grayscaleSurface;
                if (surf) {
                    cairo_set_source_surface(cr, surf, 0, 0);
                    cairo_paint(cr);
                }
            }
            return TRUE;
        };

        g_signal_connect(iconArea, "draw", G_CALLBACK(onIconDraw), this);

        gtk_box_pack_start(GTK_BOX(cardBox), iconArea, FALSE, FALSE, 0);
        m_searchIconAreas.push_back(iconArea);

        GtkWidget* textCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_valign(textCol, GTK_ALIGN_CENTER);

        GtkWidget* lblName = gtk_label_new(se.name.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(lblName), "search-card-title");
        gtk_label_set_xalign(GTK_LABEL(lblName), 0.0);
        gtk_box_pack_start(GTK_BOX(textCol), lblName, FALSE, FALSE, 0);

        GtkWidget* lblDesc = gtk_label_new(se.description.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(lblDesc), "search-card-desc");
        gtk_label_set_xalign(GTK_LABEL(lblDesc), 0.0);
        gtk_label_set_line_wrap(GTK_LABEL(lblDesc), TRUE);
        gtk_box_pack_start(GTK_BOX(textCol), lblDesc, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(cardBox), textCol, TRUE, TRUE, 0);

        // Checkmark indicator badge
        GtkWidget* checkBadge = gtk_label_new(i == 0 ? "✓" : "");
        gtk_style_context_add_class(gtk_widget_get_style_context(checkBadge), "search-card-check");
        gtk_widget_set_valign(checkBadge, GTK_ALIGN_CENTER);
        gtk_box_pack_end(GTK_BOX(cardBox), checkBadge, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(card), cardBox);

        m_searchCards.push_back(card);
        m_searchCheckmarks.push_back(checkBadge);

        int col = i % 2;
        int row = i / 2;
        gtk_grid_attach(GTK_GRID(grid), card, col, row, 1, 1);

        g_signal_connect(card, "clicked", G_CALLBACK(+[](GtkWidget* clickedWidget, gpointer user_data) {
            auto* self = static_cast<InstallerSteps*>(user_data);
            for (size_t idx = 0; idx < self->m_searchCards.size(); ++idx) {
                GtkStyleContext* sc = gtk_widget_get_style_context(self->m_searchCards[idx]);
                if (self->m_searchCards[idx] == clickedWidget) {
                    gtk_style_context_add_class(sc, "search-card-active");
                    gtk_label_set_text(GTK_LABEL(self->m_searchCheckmarks[idx]), "✓");
                    self->m_selectedSearchIndex = static_cast<int>(idx);
                    self->m_config.searchEngineName = self->m_searchEngines[idx].name;
                    self->m_config.searchEngineUrl = self->m_searchEngines[idx].urlTemplate;
                } else {
                    gtk_style_context_remove_class(sc, "search-card-active");
                    gtk_label_set_text(GTK_LABEL(self->m_searchCheckmarks[idx]), "");
                }
            }
            for (auto* area : self->m_searchIconAreas) {
                gtk_widget_queue_draw(area);
            }
        }), this);
    }

    gtk_box_pack_start(GTK_BOX(box), grid, TRUE, TRUE, 0);

    // Bottom Navigation
    GtkWidget* navBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(navBox, 20);

    GtkWidget* btnBack = gtk_button_new_with_label("Back");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnBack), "deck-btn-secondary");
    gtk_widget_set_size_request(btnBack, 100, 44);
    g_signal_connect(btnBack, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(1);
    }), this);
    gtk_box_pack_start(GTK_BOX(navBox), btnBack, FALSE, FALSE, 0);

    GtkWidget* btnContinue = gtk_button_new_with_label("Continue");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnContinue), "deck-btn-primary");
    gtk_widget_set_size_request(btnContinue, 130, 44);
    g_signal_connect(btnContinue, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(3);
    }), this);
    gtk_box_pack_end(GTK_BOX(navBox), btnContinue, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), navBox, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(m_stack), box, "step_2");
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 3: Tor Bridge
// ─────────────────────────────────────────────────────────────────────────────
void InstallerSteps::buildStep3Tor() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 28);

    // Title & Subtitle
    GtkWidget* title = gtk_label_new("Tor Hidden Services");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "deck-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget* subtitle = gtk_label_new("Direct routing for .onion domains via local SOCKS5h proxy with hardware data isolation.");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "deck-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 10);

    // Segmented Control: System Daemon vs Tor Bundle
    GtkWidget* segBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(segBox), "segmented-control");
    gtk_widget_set_halign(segBox, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(segBox, 18);
    gtk_widget_set_margin_bottom(segBox, 24);

    m_btnTorDaemon = gtk_button_new_with_label("System Daemon (Port 9050)");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnTorDaemon), "segmented-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnTorDaemon), "segmented-btn-active");
    gtk_box_pack_start(GTK_BOX(segBox), m_btnTorDaemon, TRUE, TRUE, 0);

    m_btnTorBundle = gtk_button_new_with_label("Tor Bundle (Port 9150)");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnTorBundle), "segmented-btn");
    gtk_box_pack_start(GTK_BOX(segBox), m_btnTorBundle, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(box), segBox, FALSE, FALSE, 0);

    g_signal_connect(m_btnTorDaemon, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        gtk_style_context_add_class(gtk_widget_get_style_context(self->m_btnTorDaemon), "segmented-btn-active");
        gtk_style_context_remove_class(gtk_widget_get_style_context(self->m_btnTorBundle), "segmented-btn-active");
        self->m_config.torPort = 9050;
        self->checkTorSocket(9050);
    }), this);

    g_signal_connect(m_btnTorBundle, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        gtk_style_context_add_class(gtk_widget_get_style_context(self->m_btnTorBundle), "segmented-btn-active");
        gtk_style_context_remove_class(gtk_widget_get_style_context(self->m_btnTorDaemon), "segmented-btn-active");
        self->m_config.torPort = 9150;
        self->checkTorSocket(9150);
    }), this);

    // Status / Warning Card Area
    GtkWidget* statusCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusCard), "tor-status-card");
    gtk_widget_set_margin_top(statusCard, 8);
    gtk_widget_set_margin_bottom(statusCard, 16);

    m_lblTorStatus = gtk_label_new("Probing Tor daemon on port 9050...");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_lblTorStatus), "tor-status-text");
    gtk_label_set_xalign(GTK_LABEL(m_lblTorStatus), 0.0);
    gtk_box_pack_start(GTK_BOX(statusCard), m_lblTorStatus, FALSE, FALSE, 0);

    // Warning & Auto-Config Box
    m_boxTorWarning = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_no_show_all(m_boxTorWarning, TRUE);

    m_lblTorWarning = gtk_label_new("Tor daemon is not running on port 9050.");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_lblTorWarning), "tor-warning-text");
    gtk_label_set_xalign(GTK_LABEL(m_lblTorWarning), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(m_lblTorWarning), TRUE);
    gtk_box_pack_start(GTK_BOX(m_boxTorWarning), m_lblTorWarning, FALSE, FALSE, 0);

    GtkWidget* actionRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    m_btnTorAutoConfig = gtk_button_new_with_label("Auto-Configure Tor Service");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnTorAutoConfig), "deck-btn-accent");
    gtk_widget_set_size_request(m_btnTorAutoConfig, -1, 40);

    g_signal_connect(m_btnTorAutoConfig, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        self->runTorProvisioning();
    }), this);
    gtk_box_pack_start(GTK_BOX(actionRow), m_btnTorAutoConfig, FALSE, FALSE, 0);

    m_spinnerTor = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(actionRow), m_spinnerTor, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(m_boxTorWarning), actionRow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(statusCard), m_boxTorWarning, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), statusCard, TRUE, TRUE, 0);

    // Bottom Navigation (Skip, Back, Continue)
    GtkWidget* navBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(navBox, 16);

    GtkWidget* btnSkip = gtk_button_new_with_label("Skip");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnSkip), "deck-btn-secondary");
    gtk_widget_set_size_request(btnSkip, 90, 44);
    g_signal_connect(btnSkip, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        self->m_config.onionEnabled = false;
        self->setStep(4);
    }), this);
    gtk_box_pack_start(GTK_BOX(navBox), btnSkip, FALSE, FALSE, 0);

    GtkWidget* btnBack = gtk_button_new_with_label("Back");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnBack), "deck-btn-secondary");
    gtk_widget_set_size_request(btnBack, 100, 44);
    g_signal_connect(btnBack, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(2);
    }), this);
    gtk_box_pack_start(GTK_BOX(navBox), btnBack, FALSE, FALSE, 0);

    GtkWidget* btnContinue = gtk_button_new_with_label("Continue");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnContinue), "deck-btn-primary");
    gtk_widget_set_size_request(btnContinue, 130, 44);
    g_signal_connect(btnContinue, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        self->m_config.onionEnabled = true;
        self->setStep(4);
    }), this);
    gtk_box_pack_end(GTK_BOX(navBox), btnContinue, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), navBox, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(m_stack), box, "step_3");
}

void InstallerSteps::checkTorSocket(int port) {
    if (m_torChecking) return;
    m_torChecking = true;

    gtk_label_set_text(GTK_LABEL(m_lblTorStatus), ("Probing 127.0.0.1:" + std::to_string(port) + "...").c_str());
    gtk_widget_hide(m_boxTorWarning);

    std::thread([this, port]() {
        bool ok = Blueprint::Core::TorBridge::probeTorDaemon(port, 400);

        struct ResultEv {
            InstallerSteps* self;
            int port;
            bool ok;
        };
        auto* ev = new ResultEv{ this, port, ok };

        g_idle_add(+[](gpointer d) -> gboolean {
            auto* e = static_cast<ResultEv*>(d);
            e->self->m_torChecking = false;

            if (e->ok) {
                gtk_label_set_text(GTK_LABEL(e->self->m_lblTorStatus), "● Tor service detected and ready.");
                gtk_style_context_remove_class(gtk_widget_get_style_context(e->self->m_lblTorStatus), "tor-status-err");
                gtk_style_context_add_class(gtk_widget_get_style_context(e->self->m_lblTorStatus), "tor-status-ok");
                gtk_widget_hide(e->self->m_boxTorWarning);
            } else {
                gtk_label_set_text(GTK_LABEL(e->self->m_lblTorStatus), "○ Tor service is not reachable.");
                gtk_style_context_remove_class(gtk_widget_get_style_context(e->self->m_lblTorStatus), "tor-status-ok");
                gtk_style_context_add_class(gtk_widget_get_style_context(e->self->m_lblTorStatus), "tor-status-err");

                std::string warn = "Tor daemon is not running on port " + std::to_string(e->port) + ".";
                gtk_label_set_text(GTK_LABEL(e->self->m_lblTorWarning), warn.c_str());
                gtk_widget_show_all(e->self->m_boxTorWarning);
            }

            delete e;
            return G_SOURCE_REMOVE;
        }, ev);
    }).detach();
}

void InstallerSteps::runTorProvisioning() {
    gtk_widget_set_sensitive(m_btnTorAutoConfig, FALSE);
    gtk_spinner_start(GTK_SPINNER(m_spinnerTor));
    gtk_label_set_text(GTK_LABEL(m_lblTorStatus), "Provisioning Tor system service with pkexec...");

    auto& prov = Blueprint::Core::TorProvisioner::instance();
    prov.setOnStatusChanged([this](Blueprint::Core::ProvisionState state, const std::string& msg) {
        if (state == Blueprint::Core::ProvisionState::SUCCESS) {
            gtk_spinner_stop(GTK_SPINNER(m_spinnerTor));
            gtk_widget_set_sensitive(m_btnTorAutoConfig, TRUE);
            checkTorSocket(m_config.torPort);
        } else if (state == Blueprint::Core::ProvisionState::ERROR) {
            gtk_spinner_stop(GTK_SPINNER(m_spinnerTor));
            gtk_widget_set_sensitive(m_btnTorAutoConfig, TRUE);
            gtk_label_set_text(GTK_LABEL(m_lblTorWarning), ("Configuration error: " + msg + " You can skip or retry.").c_str());
        }
    });

    prov.checkOrProvision(m_config.torPort, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 4: Deploy & Finalize
// ─────────────────────────────────────────────────────────────────────────────
void InstallerSteps::buildStep4Deploy() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 36);

    // Title & Subtitle
    GtkWidget* title = gtk_label_new("Ready to Install");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "deck-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget* subtitle = gtk_label_new("Review final system integration preferences before deploying lumen to your machine.");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "deck-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 8);

    // Options Container
    GtkWidget* optionsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(optionsBox, 24);
    gtk_widget_set_vexpand(optionsBox, TRUE);

    // 1. Desktop shortcut checkbox
    const char* deskDir = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    std::string deskLabel = "Create Desktop shortcut (" + (deskDir ? std::string(deskDir) + "/lumen.desktop" : "~/Desktop/lumen.desktop") + ")";
    m_chkDesktop = gtk_check_button_new_with_label(deskLabel.c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_chkDesktop), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_chkDesktop), "deck-checkbox");
    gtk_box_pack_start(GTK_BOX(optionsBox), m_chkDesktop, FALSE, FALSE, 0);

    // 2. Default browser checkbox
    m_chkDefaultBrowser = gtk_check_button_new_with_label("Set lumen as default system browser (xdg-settings)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_chkDefaultBrowser), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(m_chkDefaultBrowser), "deck-checkbox");
    gtk_box_pack_start(GTK_BOX(optionsBox), m_chkDefaultBrowser, FALSE, FALSE, 0);

    // Error label if deployment fails
    m_lblDeployError = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_lblDeployError), "deck-error-label");
    gtk_widget_set_no_show_all(m_lblDeployError, TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(m_lblDeployError), TRUE);
    gtk_box_pack_start(GTK_BOX(optionsBox), m_lblDeployError, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(box), optionsBox, TRUE, TRUE, 0);

    // Bottom Action Area
    GtkWidget* actionBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(actionBox, GTK_ALIGN_END);

    m_btnDeploy = gtk_button_new_with_label("Install & launch lumen");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnDeploy), "deck-btn-primary");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnDeploy), "deck-btn-deploy");
    gtk_widget_set_size_request(m_btnDeploy, -1, 52);

    g_signal_connect(m_btnDeploy, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        auto* self = static_cast<InstallerSteps*>(user_data);
        self->m_config.createDesktopShortcut = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->m_chkDesktop));
        self->m_config.setDefaultBrowser = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->m_chkDefaultBrowser));

        if (self->m_onDeploy) {
            self->m_onDeploy(self->m_config);
        } else {
            std::string errMsg;
            if (InstallExecutor::deploy(self->m_config, errMsg)) {
                InstallExecutor::launchLumen(errMsg);
                gtk_window_close(self->m_parentWindow);
            } else {
                gtk_label_set_text(GTK_LABEL(self->m_lblDeployError), errMsg.c_str());
                gtk_widget_show(self->m_lblDeployError);
            }
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(actionBox), m_btnDeploy, FALSE, FALSE, 0);

    GtkWidget* btnBack = gtk_button_new_with_label("Back");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnBack), "deck-btn-secondary");
    gtk_widget_set_size_request(btnBack, -1, 40);
    g_signal_connect(btnBack, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer user_data) {
        static_cast<InstallerSteps*>(user_data)->setStep(3);
    }), this);
    gtk_box_pack_start(GTK_BOX(actionBox), btnBack, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), actionBox, FALSE, FALSE, 0);

    gtk_stack_add_named(GTK_STACK(m_stack), box, "step_4");
}

} // namespace Blueprint::Installer
