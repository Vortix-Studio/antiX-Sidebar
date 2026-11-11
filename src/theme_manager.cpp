#include "theme_manager.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static std::vector<ThemeManager::ThemeInfo> g_cached_themes;
static std::string g_current_theme_id;
static GtkCssProvider* g_active_provider = nullptr;
static bool g_detached = false;

struct ActiveProviderCleanup {
    ~ActiveProviderCleanup()
    {
        if (!g_active_provider)
            return;

        if (!g_detached) {
            gtk_style_context_remove_provider_for_screen(
                gdk_screen_get_default(),
                GTK_STYLE_PROVIDER(g_active_provider));
        }

        g_object_unref(g_active_provider);
        g_active_provider = nullptr;
        g_detached = false;
    }
};

static ActiveProviderCleanup g_provider_cleanup;

static std::string get_exe_dir()
{
    char path[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (len == -1)
        return "/usr/local/share/antix-sidebar";

    path[len] = '\0';
    return fs::path(path).parent_path().string();
}

static std::string config_file_path()
{
    const char* config_dir = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");

    fs::path base;

    if (config_dir && *config_dir) {
        base = config_dir;
    } else if (home && *home) {
        base = fs::path(home) / ".config";
    } else {
        base = ".";
    }

    return (base / "antix-sidebar" / "theme.conf").string();
}

std::string ThemeManager::themes_dir()
{
    const char* env = std::getenv("ANTIX_SIDEBAR_THEMES_DIR");

    if (env && *env && fs::exists(env) && fs::is_directory(env)) {
        g_debug("theme_manager: directory temi da env: %s", env);
        return env;
    }

    const std::string exe_dir = get_exe_dir();

    const std::vector<std::string> candidates = {
        exe_dir + "/themes",
        exe_dir + "/../share/antix-sidebar/themes",
        exe_dir + "/../antix-sidebar/themes",
        "/usr/local/share/antix-sidebar/themes",
        "/usr/share/antix-sidebar/themes"
    };

    for (const auto& path : candidates) {
        if (fs::exists(path) && fs::is_directory(path)) {
            g_debug("theme_manager: directory temi trovata: %s",
                    path.c_str());
            return path;
        }
    }

    g_warning("theme_manager: nessuna directory temi trovata");
    return exe_dir;
}

bool ThemeManager::apply_css_file(const std::string& css_path)
{
    if (css_path.empty())
        return false;

    GtkCssProvider* provider = gtk_css_provider_new();
    GError* error = nullptr;

    if (!gtk_css_provider_load_from_path(
            provider, css_path.c_str(), &error)) {
        g_warning("theme_manager: impossibile caricare %s: %s",
                  css_path.c_str(),
                  error ? error->message : "errore sconosciuto");

        g_clear_error(&error);
        g_object_unref(provider);
        return false;
    }

    GdkScreen* screen = gdk_screen_get_default();

    if (g_active_provider && !g_detached) {
        gtk_style_context_remove_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(g_active_provider));
    }

    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    if (g_active_provider)
        g_object_unref(g_active_provider);

    g_active_provider = provider;
    g_detached = false;

    g_debug("theme_manager: CSS applicato: %s", css_path.c_str());
    return true;
}

void ThemeManager::detach_screen_provider()
{
    if (!g_active_provider || g_detached)
        return;

    gtk_style_context_remove_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(g_active_provider));

    g_detached = true;
}

void ThemeManager::reattach_screen_provider()
{
    if (!g_active_provider || !g_detached)
        return;

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(g_active_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_detached = false;
}

bool ThemeManager::provider_detached()
{
    return g_detached;
}

std::string ThemeManager::load_saved_theme_id()
{
    std::ifstream file(config_file_path());

    if (!file.is_open())
        return {};

    std::string theme_id;
    std::getline(file, theme_id);

    const size_t start = theme_id.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return {};

    const size_t end = theme_id.find_last_not_of(" \t\n\r");
    return theme_id.substr(start, end - start + 1);
}

void ThemeManager::save_theme_id(const std::string& theme_id)
{
    const std::string path = config_file_path();
    std::error_code ec;

    fs::create_directories(fs::path(path).parent_path(), ec);

    if (ec) {
        g_warning("theme_manager: impossibile creare la directory config: %s",
                  ec.message().c_str());
        return;
    }

    std::ofstream file(path);

    if (file.is_open())
        file << theme_id << '\n';
}

std::string ThemeManager::extract_theme_name(const std::string& css_raw)
{
    const size_t start = css_raw.find("/*");

    if (start == std::string::npos)
        return {};

    const size_t end = css_raw.find("*/", start);

    if (end == std::string::npos)
        return {};

    const std::string comment =
        css_raw.substr(start + 2, end - start - 2);

    const size_t first = comment.find_first_not_of(" \t\n\r");

    if (first == std::string::npos)
        return {};

    const size_t last = comment.find_last_not_of(" \t\n\r");
    return comment.substr(first, last - first + 1);
}

const std::vector<ThemeManager::ThemeInfo>&
ThemeManager::get_available_themes()
{
    if (!g_cached_themes.empty())
        return g_cached_themes;

    try {
        for (const auto& entry : fs::directory_iterator(themes_dir())) {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".css") {
                continue;
            }

            ThemeInfo info;
            info.id = entry.path().stem().string();
            info.path = entry.path().string();

            std::ifstream file(entry.path());

            if (file.is_open()) {
                const std::string content{
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                };

                info.name = extract_theme_name(content);
            }

            if (info.name.empty())
                info.name = info.id;

            g_cached_themes.push_back(std::move(info));
        }
    } catch (const std::exception& e) {
        g_warning("theme_manager: errore nella scansione dei temi: %s",
                  e.what());
    }

    std::sort(g_cached_themes.begin(), g_cached_themes.end(),
              [](const ThemeInfo& a, const ThemeInfo& b) {
                  return a.name < b.name;
              });

    return g_cached_themes;
}

std::string ThemeManager::get_default_theme_id()
{
    const auto& themes = get_available_themes();

    if (themes.empty())
        return {};

    for (const auto& theme : themes) {
        if (theme.id == "dark-navy" || theme.id == "violet-night")
            return theme.id;
    }

    return themes.front().id;
}

bool ThemeManager::apply_theme(const std::string& theme_id)
{
    const auto& themes = get_available_themes();

    std::string target_id = theme_id;

    if (target_id.empty()) {
        target_id = load_saved_theme_id();

        if (target_id.empty())
            target_id = get_default_theme_id();
    }

    for (const auto& theme : themes) {
        if (theme.id == target_id) {
            if (apply_css_file(theme.path)) {
                g_current_theme_id = target_id;
                save_theme_id(target_id);
                return true;
            }
        }
    }

    const std::string default_id = get_default_theme_id();

    if (target_id != default_id)
        return apply_theme(default_id);

    return false;
}

std::string ThemeManager::get_current_theme_id()
{
    if (!g_current_theme_id.empty())
        return g_current_theme_id;

    return load_saved_theme_id();
}

std::string ThemeManager::current_language()
{
    const char* lang = std::getenv("LANG");

    if (!lang)
        return "en";

    std::string code(lang);

    const size_t dot = code.find('.');
    if (dot != std::string::npos)
        code.resize(dot);

    const size_t underscore = code.find('_');
    if (underscore != std::string::npos)
        code.resize(underscore);

    static const std::vector<std::string> supported = {
        "it", "fr", "de", "es", "pt", "nl", "pl", "ru"
    };

    if (std::find(supported.begin(), supported.end(), code) != supported.end())
        return code;

    return "en";
}
