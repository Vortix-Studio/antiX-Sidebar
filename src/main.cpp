#include <gtk/gtk.h>
#include <glib-unix.h>

#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <libintl.h>
#include <locale.h>

#include "sidebar.h"
#include "theme_manager.h"

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

static int lock_fd = -1;
static Sidebar* sidebar = nullptr;

static bool acquire_single_instance_lock()
{
    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    std::string path = runtime_dir ? runtime_dir : "/tmp";
    path += "/antix-sidebar.lock";

    lock_fd = open(path.c_str(), O_CREAT | O_RDWR, 0600);
    return lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) == 0;
}

static GdkRectangle get_work_area()
{
    GdkRectangle area{};
    GdkDisplay* display = gdk_display_get_default();

    if (!display)
        return area;

    GdkMonitor* monitor = gdk_display_get_primary_monitor(display);
    if (!monitor)
        monitor = gdk_display_get_monitor(display, 0);
    if (!monitor)
        return area;

    gdk_monitor_get_workarea(monitor, &area);

    if (area.width <= 0 || area.height <= 0)
        gdk_monitor_get_geometry(monitor, &area);

    return area;
}

static gboolean on_unix_signal(gpointer)
{
    if (sidebar && sidebar->window)
        sidebar->request_close();
    else
        gtk_main_quit();

    return G_SOURCE_REMOVE;
}

static void on_window_destroy(GtkWidget*, gpointer)
{
    gtk_main_quit();
}

int main(int argc, char* argv[])
{
    setenv("GTK_OVERLAY_SCROLLING", "0", 0);

    setlocale(LC_ALL, "");
    bindtextdomain(
        ANTIX_GETTEXT_DOMAIN,
        g_getenv("ANTIX_SIDEBAR_LOCALEDIR")
            ? g_getenv("ANTIX_SIDEBAR_LOCALEDIR")
            : LOCALEDIR);
    bind_textdomain_codeset(ANTIX_GETTEXT_DOMAIN, "UTF-8");
    textdomain(ANTIX_GETTEXT_DOMAIN);

    gtk_init(&argc, &argv);

    if (!acquire_single_instance_lock()) {
        g_printerr("antiX Sidebar e' gia' in esecuzione. Esco.\n");
        return 0;
    }

    const char* css = std::getenv("ANTIX_SIDEBAR_CSS");
    if (css && std::ifstream(css).good())
        ThemeManager::apply_css_file(css);
    else
        ThemeManager::apply_theme("");

    sidebar = Sidebar::create(get_work_area(), false);
    if (!sidebar || !sidebar->window) {
        g_printerr("Errore fatale: impossibile creare la finestra\n");
        return 1;
    }

    g_unix_signal_add(SIGTERM, on_unix_signal, nullptr);
    g_unix_signal_add(SIGINT, on_unix_signal, nullptr);
    g_signal_connect(sidebar->window, "destroy",
                     G_CALLBACK(on_window_destroy), nullptr);

    gtk_main();
    return 0;
}
