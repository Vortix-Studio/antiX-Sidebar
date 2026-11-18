#ifndef ANTIX_SIDEBAR_I18N_H
#define ANTIX_SIDEBAR_I18N_H

#include <libintl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <unistd.h>
#include <linux/limits.h>
#include <glib.h>

#ifndef ANTIX_GETTEXT_DOMAIN
#define ANTIX_GETTEXT_DOMAIN "antix-sidebar"
#endif

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

inline std::string get_effective_localedir()
{
    const char* env = g_getenv("ANTIX_SIDEBAR_LOCALEDIR");
    if (env && std::filesystem::is_directory(env)) {
        return env;
    }

    char exe_path[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        std::filesystem::path bin_dir = std::filesystem::path(exe_path).parent_path();
        
        std::vector<std::filesystem::path> candidates = {
            bin_dir / "locale",
            bin_dir / ".." / "locale",
            bin_dir / ".." / "share" / "locale",
            bin_dir / ".." / "po" / "build" / "locale",
            bin_dir / "build" / "locale"
        };

        for (const auto& cand : candidates) {
            std::error_code ec;
            if (std::filesystem::is_directory(cand, ec)) {
                // Verifichiamo se contiene almeno una cartella con antix-sidebar.mo
                if (std::filesystem::exists(cand / "it" / "LC_MESSAGES" / (std::string(ANTIX_GETTEXT_DOMAIN) + ".mo"), ec) ||
                    std::filesystem::exists(cand / "es" / "LC_MESSAGES" / (std::string(ANTIX_GETTEXT_DOMAIN) + ".mo")) ||
                    std::filesystem::exists(cand / "de" / "LC_MESSAGES" / (std::string(ANTIX_GETTEXT_DOMAIN) + ".mo"))) {
                    return cand.lexically_normal().string();
                }
            }
        }
    }

    // Controlla percorsi standard di sistema
    const std::vector<std::string> sys_candidates = {
        "/usr/local/share/locale",
        LOCALEDIR,
        "/usr/share/locale"
    };
    for (const auto& path : sys_candidates) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(path) / "it" / "LC_MESSAGES" / (std::string(ANTIX_GETTEXT_DOMAIN) + ".mo"), ec)) {
            return path;
        }
    }

    return LOCALEDIR;
}

#ifndef _
#define _(s) dgettext(ANTIX_GETTEXT_DOMAIN, s)
#endif

#endif
