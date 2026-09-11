cmake_minimum_required(VERSION 3.20)
project(BlueprintBrowser VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization and debug flags for Linux
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g -pthread")

find_package(PkgConfig REQUIRED)

# Required dependencies
pkg_check_modules(OPENGL REQUIRED gl glew)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pango pangocairo)
pkg_check_modules(SQLITE REQUIRED sqlite3)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1 gtk+-3.0)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENGL_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${WEBKIT_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

set(COMMON_SOURCES
    src/core/config.cpp
    src/theme/colors.cpp
    src/graphics/gl_renderer.cpp
    src/graphics/shaders.cpp
    src/engine/tab_transition.cpp
    src/engine/web_tab.cpp
    src/ui/tab_strip.cpp
    src/ui/omnibox.cpp
    src/ui/topbar.cpp
    src/ui/neon_progress.cpp
    src/ui/settings_panel.cpp
    src/omnibox/calc_parser.cpp
    src/omnibox/converter.cpp
    src/omnibox/fuzzy_search.cpp
    src/storage/database.cpp
    src/ai/lazy_ai_core.cpp
    src/core/tor_bridge.cpp
    src/core/tor_provisioner.cpp
)

add_executable(blueprint_browser src/main.cpp src/core/app.cpp ${COMMON_SOURCES})

target_link_libraries(blueprint_browser
    ${OPENGL_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${WEBKIT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    pthread
    m
)

add_executable(unit_tests tests/unit_tests.cpp ${COMMON_SOURCES})

target_link_libraries(unit_tests
    ${OPENGL_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${WEBKIT_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    pthread
    m
)
