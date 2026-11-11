#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <gtk/gtk.h>
#include <libintl.h>
#include <string>
#include <vector>

#define ANTIX_GETTEXT_DOMAIN "antix-sidebar"
#define _(s) dgettext(ANTIX_GETTEXT_DOMAIN, s)

class ThemeManager {
public:
    struct ThemeInfo {
        std::string id;    // nome file senza estensione (es. "dark-navy")
        std::string path;  // percorso completo del file .css
        std::string name;  // nome leggibile, estratto dal primo commento CSS
    };

    // Directory dei temi: env ANTIX_SIDEBAR_THEMES_DIR, poi percorsi
    // relativi all'eseguibile, poi percorsi di sistema.
    static std::string themes_dir();

    // Applica un file CSS a livello di screen (tutte le finestre del
    // processo). Sostituisce il tema precedente senza accumularlo.
    static bool apply_css_file(const std::string& css_path);

    // Applica un tema per id; vuoto = tema salvato, poi default.
    static bool apply_theme(const std::string& theme_id);

    static const std::vector<ThemeInfo>& get_available_themes();
    static std::string get_default_theme_id();
    static std::string get_current_theme_id();

    // Sospensione temporanea del tema a livello di screen.
    // Utile per mostrare GtkFileChooser e dialoghi con il tema
    // di sistema: detach stacca il provider attivo, reattach
    // lo riattacca. Entrambi sono idempotenti.
    static void detach_screen_provider();
    static void reattach_screen_provider();
    static bool provider_detached();

    // Persistenza dell'id tema (~/.config/antix-sidebar/theme.conf)
    static std::string load_saved_theme_id();
    static void        save_theme_id(const std::string& theme_id);

    // Lingua corrente derivata da $LANG ("it", "en", ...)
    static std::string current_language();

private:
    // Nessuna istanza: la classe è tutta statica.
    ThemeManager() = delete;

    static std::string extract_theme_name(const std::string& css_raw);
};

#endif // THEME_MANAGER_H
