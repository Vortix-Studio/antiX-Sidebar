#include "sidebar.h"
#include "audio_control_widget.h"
#include "calendar_widget.h"
#include "weather_widget.h"
#include "news_widget.h"
#include "theme_manager.h"
#include <libintl.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <libintl.h>
#include <locale.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>



namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════════
// Utility
// ═══════════════════════════════════════════════════════════════════════════

static int safe_stoi(const std::string& s, int fallback = 0)
{
    try { return std::stoi(s); }
    catch (...) { return fallback; }
}

// ═══════════════════════════════════════════════════════════════════════════
// Locale / gettext
// ═══════════════════════════════════════════════════════════════════════════

static void set_locale(const std::string& lang)
{
    static bool initialized = false;

    if (!initialized) {
        bindtextdomain("antix-sidebar", "/usr/share/locale");
        bind_textdomain_codeset("antix-sidebar", "UTF-8");
        textdomain("antix-sidebar");
        initialized = true;
    }

    // LANGUAGE guida la scelta dei cataloghi gettext; il setlocale
    // successivo fa sì che i testi già tradotti vengano ricaricati.
    setenv("LANGUAGE", lang.c_str(), 1);
    setlocale(LC_ALL, "");
}

// ═══════════════════════════════════════════════════════════════════════════
// Accessori widget
// ═══════════════════════════════════════════════════════════════════════════

GtkWidget* Sidebar::audio_widget() const
{
    return audio ? audio->widget() : nullptr;
}

GtkWidget* Sidebar::calendar_widget() const
{
    return calendar ? calendar->get_container() : nullptr;
}

GtkWidget* Sidebar::weather_widget() const
{
    return weather ? weather->get_container() : nullptr;
}

GtkWidget* Sidebar::news_widget() const
{
    return news ? news->get_widget() : nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════
// Riordino widget
// ═══════════════════════════════════════════════════════════════════════════

void Sidebar::apply_widget_order(const int order[4])
{
    GtkWidget* widgets[4] = {
        audio_widget(),
        calendar_widget(),
        weather_widget(),
        news_widget()
    };

    for (int i = 0; i < 4; ++i) {
        if (widgets[i] && gtk_widget_get_parent(widgets[i]) == GTK_WIDGET(inner)) {
            g_object_ref(widgets[i]);
            gtk_container_remove(GTK_CONTAINER(inner), widgets[i]);
        }
    }

    int position = 1;
    for (int i = 0; i < 4; ++i) {
        const int index = order[i];
        if (index < 0 || index >= 4 || !widgets[index])
            continue;

        gtk_box_pack_start(GTK_BOX(inner), widgets[index], FALSE, FALSE, 6);
        gtk_box_reorder_child(GTK_BOX(inner), widgets[index], position);
        ++position;
        g_object_unref(widgets[index]);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Costanti
// ═══════════════════════════════════════════════════════════════════════════

static constexpr int SIDEBAR_WIDTH_INITIAL = 190;   // ◄── MODIFICA: era 230
static constexpr int SIDEBAR_WIDTH_MIN     = 140;
static constexpr int SIDEBAR_WIDTH_MAX     = 560;
static constexpr int GRIP_WIDTH            = 6;

static constexpr int COMPACT_THRESHOLD = 175;
static constexpr int WIDE_THRESHOLD    = 330;

static constexpr int INNER_BORDER_WIDTH = 8;        // ◄── MODIFICA: era 12
static constexpr int CHILD_SPACING      = 4;        // ◄── MODIFICA: era 6

static constexpr guint ABOVE_GUARD_INTERVAL_MS = 30000;
static constexpr guint CLOSE_UNMAP_DELAY_MS    = 1000;

// Lingue disponibili nel menù a tendina
static const struct { const char* id; const char* name; } LANGS[] = {
    { "it", "Italiano"   },
    { "en", "English"    },
    { "de", "Deutsch"    },
    { "es", "Español"    },
    { "fr", "Français"   },
    { "ja", "日本語"      },
    { "nl", "Nederlands" },
    { "pl", "Polski"     },
    { "pt", "Português"  },
    { "ru", "Русский"    },
    { "uk", "Українська" }
};

// ═══════════════════════════════════════════════════════════════════════════
// Cache Atom X11
// ═══════════════════════════════════════════════════════════════════════════

struct AtomCache {
    Atom above   = None;
    Atom sticky  = None;
    Atom skip_tb = None;
    Atom skip_pg = None;
    Atom state   = None;
    bool ready   = false;

    void init(Display* dpy) {
        if (ready) return;
        above   = XInternAtom(dpy, "_NET_WM_STATE_ABOVE",        False);
        sticky  = XInternAtom(dpy, "_NET_WM_STATE_STICKY",       False);
        skip_tb = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        skip_pg = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER",   False);
        state   = XInternAtom(dpy, "_NET_WM_STATE",              False);
        ready   = true;
    }
};
static AtomCache g_atoms;

// ═══════════════════════════════════════════════════════════════════════════
// Helper X11 / finestra
// ═══════════════════════════════════════════════════════════════════════════

static void pack_top(GtkBox* box, GtkWidget* child, guint pad)
{
    if (!box || !GTK_IS_BOX(box)) return;
    if (!child || !GTK_IS_WIDGET(child)) return;
    gtk_box_pack_start(box, child, FALSE, FALSE, pad);
}

static int current_window_width(GtkWidget* window)
{
    if (!window || !GTK_IS_WINDOW(window)) return SIDEBAR_WIDTH_INITIAL;
    gint w = 0, h = 0;
    gtk_window_get_size(GTK_WINDOW(window), &w, &h);
    return (w > 0) ? w : SIDEBAR_WIDTH_INITIAL;
}

// ═══════════════════════════════════════════════════════════════════════════
// Gestione focus
// ═══════════════════════════════════════════════════════════════════════════

// Un widget merita il focus di tastiera solo se è interagibile.
// Tutto il resto (contenitori, etichette, box) non lo prende mai.
static bool focusable_by_type(GtkWidget* w)
{
    return GTK_IS_BUTTON(w)          ||
           GTK_IS_TOGGLE_BUTTON(w)   ||
           GTK_IS_CHECK_BUTTON(w)    ||
           GTK_IS_RADIO_BUTTON(w)    ||
           GTK_IS_SCALE(w)           ||
           GTK_IS_RANGE(w)           ||
           GTK_IS_SCROLLBAR(w)       ||
           GTK_IS_COMBO_BOX(w)       ||
           GTK_IS_ENTRY(w)           ||
           GTK_IS_SEARCH_ENTRY(w)    ||
           GTK_IS_SPIN_BUTTON(w)     ||
           GTK_IS_SWITCH(w)          ||
           GTK_IS_LIST_BOX_ROW(w);
}

static void restrict_focus_recursive(GtkWidget* w)
{
    if (!w || !GTK_IS_WIDGET(w))
        return;

    gtk_widget_set_can_focus(w, focusable_by_type(w) ? TRUE : FALSE);
    gtk_widget_set_focus_on_click(w, FALSE);

    if (GTK_IS_CONTAINER(w)) {
        GList* kids = gtk_container_get_children(GTK_CONTAINER(w));
        for (GList* n = kids; n; n = n->next)
            restrict_focus_recursive(GTK_WIDGET(n->data));
        g_list_free(kids);
    }
}

static void send_wm_state(Display* dpy, Window xwin, long action, Atom a1, Atom a2)
{
    XEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = xwin;
    ev.xclient.message_type = g_atoms.state;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = action;
    ev.xclient.data.l[1]    = static_cast<long>(a1);
    ev.xclient.data.l[2]    = static_cast<long>(a2);
    ev.xclient.data.l[3]    = 1;
    ev.xclient.data.l[4]    = 0;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

void Sidebar::apply_always_on_top()
{
    if (!window || !GTK_IS_WIDGET(window)) return;
    GdkWindow* gw = gtk_widget_get_window(window);
    if (!gw) return;

    if (gdk_window_get_state(gw) & GDK_WINDOW_STATE_ABOVE) return;

    Display* dpy  = GDK_WINDOW_XDISPLAY(gw);
    Window   xwin = GDK_WINDOW_XID(gw);

    g_atoms.init(dpy);

    Atom states[4] = { g_atoms.above, g_atoms.sticky, g_atoms.skip_tb, g_atoms.skip_pg };
    XChangeProperty(dpy, xwin, g_atoms.state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states), 4);

    send_wm_state(dpy, xwin, 1, g_atoms.above,   g_atoms.sticky);
    send_wm_state(dpy, xwin, 1, g_atoms.skip_tb, g_atoms.skip_pg);

    XRaiseWindow(dpy, xwin);
    XFlush(dpy);
}

void Sidebar::setup_window_hints(GtkWidget* window, GdkRectangle work_area)
{
    if (!window || !GTK_IS_WIDGET(window)) return;
    GdkWindow* gdk_win = gtk_widget_get_window(window);
    if (!gdk_win) return;

    Display* dpy  = GDK_WINDOW_XDISPLAY(gdk_win);
    Window   xwin = GDK_WINDOW_XID(gdk_win);

    Atom desktop_a = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    long all_desks = 0xFFFFFFFF;
    XChangeProperty(dpy, xwin, desktop_a, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&all_desks), 1);

    if (!struts_enabled) {
        Atom strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
        Atom strut         = XInternAtom(dpy, "_NET_WM_STRUT", False);

        XDeleteProperty(dpy, xwin, strut_partial);
        XDeleteProperty(dpy, xwin, strut);

        apply_always_on_top();
        XFlush(dpy);
        return;
    }

    const long w = current_width;
    long strut[12] = {
        w, 0, 0, 0,
        work_area.y,
        work_area.y + work_area.height - 1,
        0, 0, 0, 0, 0, 0
    };

    Atom strut_p = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    XChangeProperty(dpy, xwin, strut_p, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(strut), 12);

    Atom strut_a = XInternAtom(dpy, "_NET_WM_STRUT", False);
    XChangeProperty(dpy, xwin, strut_a, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(strut), 4);

    apply_always_on_top();
    XFlush(dpy);
}

void Sidebar::update_strut(GtkWidget* window)
{
    if (closing) return;
    if (!window || !struts_enabled) return;
    if (current_width == last_strut_width) return;
    last_strut_width = current_width;
    setup_window_hints(window, work_area);
}

void Sidebar::clear_strut()
{
    if (!window || !GTK_IS_WIDGET(window)) return;
    GdkWindow* gdk_win = gtk_widget_get_window(window);
    if (!gdk_win) return;

    Display* dpy = GDK_WINDOW_XDISPLAY(gdk_win);
    Window xwin = GDK_WINDOW_XID(gdk_win);

    Atom strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom strut         = XInternAtom(dpy, "_NET_WM_STRUT", False);

    XDeleteProperty(dpy, xwin, strut_partial);
    XDeleteProperty(dpy, xwin, strut);
    XSync(dpy, False);
}

void Sidebar::apply_width(int width)
{
    if (!window || !GTK_IS_WIDGET(window)) return;
    if (width < SIDEBAR_WIDTH_MIN) width = SIDEBAR_WIDTH_MIN;
    if (width > SIDEBAR_WIDTH_MAX) width = SIDEBAR_WIDTH_MAX;

    current_width = width;
    gtk_window_resize(GTK_WINDOW(window), width, work_area.height);
    gtk_window_move(GTK_WINDOW(window), work_area.x, work_area.y);

    last_strut_width = -1;
    update_strut(window);
    update_layout_classes(width);
}

void Sidebar::update_layout_classes(int width)
{
    if (!inner || !GTK_IS_WIDGET(inner)) return;
    GtkStyleContext* ctx = gtk_widget_get_style_context(inner);

    if (width < COMPACT_THRESHOLD)
        gtk_style_context_add_class(ctx, "compact");
    else
        gtk_style_context_remove_class(ctx, "compact");

    if (width >= WIDE_THRESHOLD)
        gtk_style_context_add_class(ctx, "wide");
    else
        gtk_style_context_remove_class(ctx, "wide");
}

// ═══════════════════════════════════════════════════════════════════════════
// Persistenza layout
// ═══════════════════════════════════════════════════════════════════════════

std::string Sidebar::layout_path() const
{
    const char* config_dir = std::getenv("XDG_CONFIG_HOME");
    std::string base = config_dir
        ? std::string(config_dir)
        : std::string(std::getenv("HOME")) + "/.config";
    return base + "/antix-sidebar/layout.conf";
}

void Sidebar::save_layout()
{
    std::string path = layout_path();
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    if (ec) {
        g_warning("sidebar: impossibile creare directory config: %s",
                  ec.message().c_str());
        return;
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        g_warning("sidebar: impossibile salvare layout in %s", path.c_str());
        return;
    }

    file << "# antiX Sidebar layout config\n";
    file << "width=" << current_width << "\n";
    file << "language=" << language << "\n";
    file << "theme=" << current_theme << "\n";
    file << "audio=" << (audio_visible ? 1 : 0) << "\n";
    file << "calendar=" << (calendar_visible ? 1 : 0) << "\n";
    file << "weather=" << (weather_visible ? 1 : 0) << "\n";
    file << "news=" << (news_visible ? 1 : 0) << "\n";

    GList* children = gtk_container_get_children(GTK_CONTAINER(inner));
    int order_count = 0;

    for (GList* node = children; node && order_count < 4; node = node->next) {
        GtkWidget* child = GTK_WIDGET(node->data);
        int idx = -1;

        if (child == audio_widget())         idx = 0;
        else if (child == calendar_widget()) idx = 1;
        else if (child == weather_widget())  idx = 2;
        else if (child == news_widget())     idx = 3;

        if (idx >= 0) {
            file << "order=" << idx << "\n";
            ++order_count;
        }
    }
    g_list_free(children);

    g_debug("sidebar: layout salvato in %s", path.c_str());
}

void Sidebar::load_layout()
{
    std::string path = layout_path();
    std::ifstream file(path);
    if (!file.is_open()) {
        g_debug("sidebar: nessun layout salvato, uso default");
        return;
    }

    int order[4] = {-1, -1, -1, -1};
    int order_count = 0;
    int saved_width = -1;
    std::string saved_lang;
    std::string saved_theme;

    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        size_t vs = val.find_first_not_of(" \t\r\n");
        size_t ve = val.find_last_not_of(" \t\r\n");
        if (vs != std::string::npos)
            val = val.substr(vs, ve - vs + 1);
        else
            continue;

        if (key == "audio")
            audio_visible = (val == "1");
        else if (key == "calendar")
            calendar_visible = (val == "1");
        else if (key == "weather")
            weather_visible = (val == "1");
        else if (key == "news")
            news_visible = (val == "1");
        else if (key == "order" && order_count < 4) {
            int idx = safe_stoi(val, -1);
            if (idx >= 0 && idx < 4)
                order[order_count++] = idx;
        }
        else if (key == "width")
            saved_width = safe_stoi(val, -1);
        else if (key == "language")
            saved_lang = val;
        else if (key == "theme")
            saved_theme = val;
    }

    if (!saved_lang.empty())
        language = saved_lang;

    if (!saved_theme.empty() && saved_theme != current_theme &&
        saved_theme != "custom")
        apply_theme(saved_theme);

    GtkWidget* widgets[4] = {
        audio_widget(),
        calendar_widget(),
        weather_widget(),
        news_widget()
    };
    const bool vis[4] = { audio_visible, calendar_visible,
                          weather_visible, news_visible };

    for (int i = 0; i < 4; ++i) {
        if (widgets[i]) {
            gtk_widget_set_no_show_all(widgets[i], TRUE);
            gtk_widget_set_visible(widgets[i], vis[i]);
        }
    }

    if (order_count == 4)
        apply_widget_order(order);

    if (saved_width >= SIDEBAR_WIDTH_MIN && saved_width <= SIDEBAR_WIDTH_MAX)
        current_width = saved_width;

    g_debug("sidebar: layout caricato da %s", path.c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// Costruzione UI
// ═══════════════════════════════════════════════════════════════════════════

GtkWidget* Sidebar::make_separator()
{
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(sep, "section-sep");
    return sep;
}

GtkWidget* Sidebar::make_header()
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(row, "header-box");

    GtkWidget* l_anti = gtk_label_new("anti");
    gtk_widget_set_name(l_anti, "logo-label");

    GtkWidget* l_x = gtk_label_new("X");
    gtk_widget_set_name(l_x, "logo-x");

    customize_btn = gtk_button_new();
    gtk_widget_set_name(customize_btn, "customize-button");

    GtkWidget* customize_icon =
        gtk_image_new_from_icon_name(
            "preferences-system-symbolic",
            GTK_ICON_SIZE_BUTTON);

    gtk_container_add(GTK_CONTAINER(customize_btn), customize_icon);
    gtk_button_set_relief(GTK_BUTTON(customize_btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(customize_btn, _("Customize sidebar"));
    gtk_widget_set_margin_start(customize_btn, 4);
    gtk_widget_set_margin_end(customize_btn, 4);

    GtkWidget* btn = gtk_button_new_with_label("\xC3\x97");
    gtk_widget_set_name(btn, "close-btn");
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn, _("Close sidebar"));

    gtk_box_pack_start(GTK_BOX(row), l_anti, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(row), l_x, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(row), btn, FALSE, FALSE, 4);
    gtk_box_pack_end(GTK_BOX(row), customize_btn, FALSE, FALSE, 4);

    close_btn = btn;
    return row;
}

GtkWidget* Sidebar::make_resize_grip()
{
    GtkWidget* grip = gtk_event_box_new();
    gtk_widget_set_name(grip, "resize-grip");
    gtk_widget_set_size_request(grip, GRIP_WIDTH, -1);
    gtk_widget_set_vexpand(grip, TRUE);
    gtk_widget_add_events(grip,
        GDK_BUTTON_PRESS_MASK   | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK    |
        GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect(grip, "realize",
        G_CALLBACK(+[](GtkWidget* w, gpointer) {
            GdkWindow* gw = gtk_widget_get_window(w);
            if (!gw) return;
            GdkCursor* cur = gdk_cursor_new_for_display(
                gdk_window_get_display(gw), GDK_SB_H_DOUBLE_ARROW);
            gdk_window_set_cursor(gw, cur);
            g_object_unref(cur);
        }), nullptr);

    return grip;
}

// ═══════════════════════════════════════════════════════════════════════════
// Chiusura
// ═══════════════════════════════════════════════════════════════════════════

static gboolean finish_close(gpointer data)
{
    auto* self = static_cast<Sidebar*>(data);
    self->close_source = 0;

    if (self->audio) {
        self->audio->free_resources();
        self->audio = nullptr;
    }

    if (self->window && GTK_IS_WIDGET(self->window)) {
        GdkDisplay* display = gtk_widget_get_display(self->window);
        self->clear_strut();
        gtk_widget_destroy(self->window);
        self->window = nullptr;
        if (display) gdk_display_sync(display);
    }
    return G_SOURCE_REMOVE;
}

void Sidebar::request_close()
{
    if (!window || !GTK_IS_WIDGET(window) || close_source != 0) return;
    closing = true;

    save_layout();

    if (guard_source) {
        g_source_remove(guard_source);
        guard_source = 0;
    }

    GdkDisplay* display = gtk_widget_get_display(window);
    clear_strut();
    gdk_display_flush(display);
    gtk_widget_hide(window);
    gdk_display_sync(display);

    close_source = g_timeout_add(CLOSE_UNMAP_DELAY_MS, finish_close, this);
}

// ═══════════════════════════════════════════════════════════════════════════
// Guard timer
// ═══════════════════════════════════════════════════════════════════════════

static gboolean above_guard_tick(gpointer data)
{
    auto* self = static_cast<Sidebar*>(data);
    if (!self || !self->window || !GTK_IS_WIDGET(self->window))
        return G_SOURCE_REMOVE;

    GdkWindow* gw = gtk_widget_get_window(self->window);
    if (!gw) return G_SOURCE_CONTINUE;

    if (!(gdk_window_get_state(gw) & GDK_WINDOW_STATE_ABOVE)) {
        gtk_window_set_keep_above(GTK_WINDOW(self->window), TRUE);
        self->apply_always_on_top();
    }
    return G_SOURCE_CONTINUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// Tema
// ═══════════════════════════════════════════════════════════════════════════

void Sidebar::apply_theme(const std::string& theme_id)
{
    if (ThemeManager::apply_theme(theme_id)) {
        current_theme = theme_id;
    }
}

void Sidebar::apply_css_file(const char* path)
{
    if (ThemeManager::apply_css_file(path)) {
        current_theme = "custom";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Lingua
// ═══════════════════════════════════════════════════════════════════════════

void Sidebar::apply_language()
{
    set_locale(language);

    if (customize_btn)
        gtk_widget_set_tooltip_text(customize_btn, _("Customize sidebar"));

    if (close_btn)
        gtk_widget_set_tooltip_text(close_btn, _("Close sidebar"));

    if (audio)    audio->set_language(language);
    if (calendar) calendar->set_language(language);
    if (weather)  weather->set_language(language);
    if (news)     news->set_language(language);
}

// ═══════════════════════════════════════════════════════════════════════════
// Dialogo personalizzazione
// ═══════════════════════════════════════════════════════════════════════════

void Sidebar::on_customize(GtkButton*, gpointer data)
{
    auto* self = static_cast<Sidebar*>(data);
    if (!self)
        return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Customize sidebar"),
        GTK_WINDOW(self->window),
        GTK_DIALOG_MODAL,
        _("_Cancel"),
        GTK_RESPONSE_CANCEL,
        _("_Apply"),
        GTK_RESPONSE_APPLY,
        nullptr);

    gtk_widget_set_name(dialog, "dialog-window");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(content), box);

    // ── Lingua ──────────────────────────────────────────────────────────────
    gtk_box_pack_start(
        GTK_BOX(box),
        gtk_label_new(_("Language")),
        FALSE, FALSE, 0);

    GtkWidget* language_combo = gtk_combo_box_text_new();
    gtk_widget_set_name(language_combo, "language-combo");

    for (const auto& lang : LANGS) {
        gtk_combo_box_text_append(
            GTK_COMBO_BOX_TEXT(language_combo),
            lang.id,
            lang.name);
    }

    // Se la lingua salvata non è tra quelle offerte, ripiega su "it"
    if (!gtk_combo_box_set_active_id(
            GTK_COMBO_BOX(language_combo), self->language.c_str()))
        gtk_combo_box_set_active_id(
            GTK_COMBO_BOX(language_combo), "it");

    gtk_box_pack_start(GTK_BOX(box), language_combo, FALSE, FALSE, 0);

    // ── Tema CSS ────────────────────────────────────────────────────────────
    gtk_box_pack_start(
        GTK_BOX(box),
        gtk_label_new(_("Sidebar Theme (CSS)")),
        FALSE, FALSE, 0);

    GtkWidget* theme_combo = gtk_combo_box_text_new();
    gtk_widget_set_name(theme_combo, "theme-combo");
    const auto& themes = ThemeManager::get_available_themes();
    for (const auto& th : themes) {
        gtk_combo_box_text_append(
            GTK_COMBO_BOX_TEXT(theme_combo),
            th.id.c_str(),
            th.name.c_str());
    }
    gtk_combo_box_text_append(
        GTK_COMBO_BOX_TEXT(theme_combo),
        "custom",
        _("Custom local file…"));

    gtk_combo_box_set_active_id(
        GTK_COMBO_BOX(theme_combo), self->current_theme.c_str());
    if (!gtk_combo_box_get_active_id(GTK_COMBO_BOX(theme_combo))) {
        gtk_combo_box_set_active_id(
            GTK_COMBO_BOX(theme_combo),
            ThemeManager::get_default_theme_id().c_str());
    }
    gtk_box_pack_start(GTK_BOX(box), theme_combo, FALSE, FALSE, 0);

    GtkWidget* css_button = gtk_button_new_with_label(_("Browse CSS files…"));
    gtk_widget_set_no_show_all(css_button, TRUE);
    gtk_box_pack_start(GTK_BOX(box), css_button, FALSE, FALSE, 0);

    const char* active_theme_id =
        gtk_combo_box_get_active_id(GTK_COMBO_BOX(theme_combo));
    bool is_custom = (active_theme_id && std::string(active_theme_id) == "custom");
    gtk_widget_set_visible(css_button, is_custom);

    g_signal_connect(
        theme_combo, "changed",
        G_CALLBACK(+[](GtkComboBox* combo, gpointer user_data) {
            GtkWidget* btn = GTK_WIDGET(user_data);
            const gchar* id = gtk_combo_box_get_active_id(combo);
            gtk_widget_set_visible(btn, id && std::string(id) == "custom");
        }),
        css_button);

    // ── Widget: visibilità e ordine ─────────────────────────────────────────
    gtk_box_pack_start(
        GTK_BOX(box),
        gtk_label_new(_("Visible widgets and order")),
        FALSE, FALSE, 0);

    GtkWidget* list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_box_pack_start(GTK_BOX(box), list, TRUE, TRUE, 0);

    GtkWidget* widgets[4] = {
        self->audio_widget(),
        self->calendar_widget(),
        self->weather_widget(),
        self->news_widget()
    };

    const char* names[4] = {
        _("Audio"), _("Calendar"), _("Weather"), _("News")
    };

    const bool visible[4] = {
        self->audio_visible,
        self->calendar_visible,
        self->weather_visible,
        self->news_visible
    };

    for (int i = 0; i < 4; ++i) {
        if (!widgets[i])
            continue;

        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* check = gtk_check_button_new_with_label(names[i]);

        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(check), visible[i]);

        g_object_set_data(
            G_OBJECT(row), "sidebar-widget-index", GINT_TO_POINTER(i));
        g_object_set_data(
            G_OBJECT(row), "sidebar-check-button", check);

        gtk_box_pack_start(GTK_BOX(line), check, TRUE, TRUE, 0);

        GtkWidget* up = gtk_button_new_with_label("↑");
        GtkWidget* down = gtk_button_new_with_label("↓");

        gtk_widget_set_tooltip_text(up, _("Move up"));
        gtk_widget_set_tooltip_text(down, _("Move down"));

        gtk_box_pack_start(GTK_BOX(line), up, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(line), down, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(row), line);
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);

        g_signal_connect(
            up, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer user_data) {
                GtkWidget* row = GTK_WIDGET(user_data);
                GtkWidget* lst = gtk_widget_get_parent(row);
                int index = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row));

                if (!lst || index <= 0)
                    return;

                g_object_ref(row);
                gtk_container_remove(GTK_CONTAINER(lst), row);
                gtk_list_box_insert(GTK_LIST_BOX(lst), row, index - 1);
                g_object_unref(row);
                gtk_widget_show_all(lst);
            }),
            row);

        g_signal_connect(
            down, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer user_data) {
                GtkWidget* row = GTK_WIDGET(user_data);
                GtkWidget* lst = gtk_widget_get_parent(row);

                if (!lst)
                    return;

                int index = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row));
                GList* children =
                    gtk_container_get_children(GTK_CONTAINER(lst));
                int count = g_list_length(children);
                g_list_free(children);

                if (index < 0 || index >= count - 1)
                    return;

                g_object_ref(row);
                gtk_container_remove(GTK_CONTAINER(lst), row);
                gtk_list_box_insert(GTK_LIST_BOX(lst), row, index + 1);
                g_object_unref(row);
                gtk_widget_show_all(lst);
            }),
            row);
    }

    // ── Scelta file CSS custom ──────────────────────────────────────────────
    g_signal_connect(
        css_button, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            GtkWidget* dlg = GTK_WIDGET(user_data);

            auto* sb = static_cast<Sidebar*>(
                g_object_get_data(G_OBJECT(dlg), "sidebar-instance"));

            if (!sb) {
                g_warning("sidebar: impossibile recuperare Sidebar dal dialogo");
                return;
            }

            static gchar* last_path = nullptr;

            // Sospende il tema CSS della sidebar: il file chooser
            // torna al tema di sistema.
            ThemeManager::detach_screen_provider();

            GtkWidget* chooser = gtk_file_chooser_dialog_new(
                _("Select CSS file"),
                GTK_WINDOW(sb->window),
                GTK_FILE_CHOOSER_ACTION_OPEN,
                _("_Cancel"), GTK_RESPONSE_CANCEL,
                _("_Open"),   GTK_RESPONSE_ACCEPT,
                nullptr);

            GtkFileFilter* filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, "CSS (*.css)");
            gtk_file_filter_add_pattern(filter, "*.css");
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

            if (last_path) {
                gchar* dir = g_path_get_dirname(last_path);
                gtk_file_chooser_set_current_folder(
                    GTK_FILE_CHOOSER(chooser), dir);
                g_free(dir);
                gtk_file_chooser_set_filename(
                    GTK_FILE_CHOOSER(chooser), last_path);
            } else {
                gchar* cwd = g_get_current_dir();
                gtk_file_chooser_set_current_folder(
                    GTK_FILE_CHOOSER(chooser), cwd);
                g_free(cwd);
            }

            gint response = gtk_dialog_run(GTK_DIALOG(chooser));

            gchar* path = (response == GTK_RESPONSE_ACCEPT)
                ? gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser))
                : nullptr;

            gtk_widget_destroy(chooser);

            // Ripristina il tema CSS della sidebar
            ThemeManager::reattach_screen_provider();

            if (path) {
                g_debug("sidebar: CSS selezionato: %s", path);

                g_free(last_path);
                last_path = g_strdup(path);

                g_object_set_data_full(
                    G_OBJECT(dlg),
                    "selected-css-path",
                    path,
                    g_free);
            }
        }),
        dialog);

    g_object_set_data(G_OBJECT(dialog), "sidebar-instance", self);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY) {

        const gchar* language_id =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(language_combo));
        self->language = language_id ? language_id : "it";
        self->apply_language();

        GtkWidget* checks[4] = {nullptr, nullptr, nullptr, nullptr};
        int order[4] = {-1, -1, -1, -1};
        int order_count = 0;

        GList* rows = gtk_container_get_children(GTK_CONTAINER(list));

        for (GList* node = rows; node; node = node->next) {
            GtkWidget* row = GTK_WIDGET(node->data);
            int index = GPOINTER_TO_INT(
                g_object_get_data(G_OBJECT(row), "sidebar-widget-index"));

            GtkWidget* check = GTK_WIDGET(
                g_object_get_data(G_OBJECT(row), "sidebar-check-button"));

            if (index >= 0 && index < 4) {
                checks[index] = check;
                order[order_count++] = index;
            }
        }

        g_list_free(rows);

        self->audio_visible =
            checks[0] && gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checks[0]));
        self->calendar_visible =
            checks[1] && gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checks[1]));
        self->weather_visible =
            checks[2] && gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checks[2]));
        self->news_visible =
            checks[3] && gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checks[3]));

        if (widgets[0])
            gtk_widget_set_visible(widgets[0], self->audio_visible);
        if (widgets[1])
            gtk_widget_set_visible(widgets[1], self->calendar_visible);
        if (widgets[2])
            gtk_widget_set_visible(widgets[2], self->weather_visible);
        if (widgets[3])
            gtk_widget_set_visible(widgets[3], self->news_visible);

        if (order_count == 4)
            self->apply_widget_order(order);

        const gchar* chosen_theme =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(theme_combo));

        if (chosen_theme) {
            if (std::string(chosen_theme) == "custom") {
                const gchar* css_path = static_cast<const gchar*>(
                    g_object_get_data(G_OBJECT(dialog), "selected-css-path"));
                if (css_path) {
                    self->apply_css_file(css_path);
                    g_message("CSS applicato da file: %s", css_path);
                }
            } else {
                self->apply_theme(chosen_theme);
                ThemeManager::save_theme_id(chosen_theme);
                g_message("Tema CSS integrato applicato: %s", chosen_theme);
            }
        }

        self->save_layout();
    }

    gtk_widget_destroy(dialog);
}

// ═══════════════════════════════════════════════════════════════════════════
// Creazione finestra
// ═══════════════════════════════════════════════════════════════════════════

Sidebar* Sidebar::create(GdkRectangle work_area, bool struts_enabled)
{
    auto* sb = new Sidebar{};
    sb->work_area      = work_area;
    sb->struts_enabled = struts_enabled;
    sb->current_width  = SIDEBAR_WIDTH_INITIAL;
    sb->current_theme  = ThemeManager::get_current_theme_id();
    sb->language       = ThemeManager::current_language();

    sb->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(sb->window, "sidebar-window");
    gtk_window_set_title(GTK_WINDOW(sb->window), "antiX Sidebar");

    gtk_window_set_decorated(GTK_WINDOW(sb->window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(sb->window), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(sb->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(sb->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(sb->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(sb->window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_stick(GTK_WINDOW(sb->window));

    // ◄── MODIFICA: niente focus al momento del mapping
    gtk_window_set_focus_on_map(GTK_WINDOW(sb->window), FALSE);

    // ◄── MODIFICA: la finestra non accetta il focus del WM affatto.
    // Se vuoi poter usare la tastiera sulla sidebar (es. navigare
    // i controlli audio con Tab/frecce), COMMENTA questa riga: il
    // focus_on_map(FALSE) qui sopra resta comunque attivo.
    gtk_window_set_accept_focus(GTK_WINDOW(sb->window), FALSE);

    gtk_window_set_default_size(GTK_WINDOW(sb->window),
                                SIDEBAR_WIDTH_INITIAL, work_area.height);
    gtk_widget_set_size_request(sb->window, SIDEBAR_WIDTH_MIN, -1);
    gtk_window_move(GTK_WINDOW(sb->window), work_area.x, work_area.y);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(root, "sidebar-root");

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    sb->inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(sb->inner, "sidebar-inner");
    gtk_container_set_border_width(GTK_CONTAINER(sb->inner),
                                   INNER_BORDER_WIDTH);   // ◄── MODIFICA

    GtkWidget* hdr = sb->make_header();
    pack_top(GTK_BOX(sb->inner), hdr, 0);

    if (sb->close_btn) {
        g_signal_connect(sb->close_btn, "clicked",
            G_CALLBACK(+[](GtkWidget*, gpointer data) {
                auto* self = static_cast<Sidebar*>(data);
                if (self) self->request_close();
            }), sb);
    }

    if (sb->customize_btn) {
        g_signal_connect(sb->customize_btn, "clicked",
                     G_CALLBACK(Sidebar::on_customize), sb);
        g_object_set_data(G_OBJECT(sb->customize_btn), "sidebar-instance", sb);
    }

    // ── Widget Audio ────────────────────────────────────────────────────────
    try {
        sb->audio = AudioControlWidget::create();
        if (sb->audio)
            pack_top(GTK_BOX(sb->inner), sb->audio->widget(),
                     CHILD_SPACING);                          // ◄── MODIFICA
    } catch (...) {
        g_warning("sidebar: AudioControlWidget::create() ha lanciato un'eccezione");
    }

    // ── Widget Calendario ───────────────────────────────────────────────────
    try {
        sb->calendar = CalendarWidget::create();
        if (sb->calendar)
            pack_top(GTK_BOX(sb->inner), sb->calendar->get_container(),
                     CHILD_SPACING);                          // ◄── MODIFICA
    } catch (...) {
        g_warning("sidebar: CalendarWidget::create() ha lanciato un'eccezione");
    }

    // ── Widget Meteo ────────────────────────────────────────────────────────
    try {
        sb->weather = WeatherWidget::new_widget();
        if (sb->weather)
            pack_top(GTK_BOX(sb->inner), sb->weather->get_container(),
                     CHILD_SPACING);                          // ◄── MODIFICA
    } catch (...) {
        g_warning("sidebar: WeatherWidget::new_widget() ha lanciato un'eccezione");
    }

    // ── Widget Notizie ──────────────────────────────────────────────────────
    try {
        sb->news = NewsWidget::create();
        if (sb->news)
            pack_top(GTK_BOX(sb->inner), sb->news->get_widget(),
                     CHILD_SPACING);                          // ◄── MODIFICA
    } catch (...) {
        g_warning("sidebar: NewsWidget::create() ha lanciato un'eccezione");
    }

    // ◄── MODIFICA: applica la policy di focus a TUTTI i figli di inner,
    // ricorsivamente, compresi i widget audio e i discendenti dei container.
    restrict_focus_recursive(GTK_WIDGET(sb->inner));

    gtk_container_add(GTK_CONTAINER(scroll), sb->inner);
    sb->grip = sb->make_resize_grip();

    gtk_box_pack_start(GTK_BOX(root), scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(root), sb->grip, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(sb->window), root);

    g_signal_connect_after(sb->window, "realize",
        G_CALLBACK(+[](GtkWidget* w, gpointer data) {
            auto* self = static_cast<Sidebar*>(data);
            self->current_width    = current_window_width(w);
            self->last_strut_width = -1;
            self->setup_window_hints(w, self->work_area);
            self->update_layout_classes(self->current_width);

            if (self->guard_source == 0) {
                self->guard_source = g_timeout_add(
                    ABOVE_GUARD_INTERVAL_MS, above_guard_tick, self);
            }
        }), sb);

    g_signal_connect_after(sb->window, "map-event",
        G_CALLBACK(+[](GtkWidget* w, GdkEvent*, gpointer data) -> gboolean {
            auto* self = static_cast<Sidebar*>(data);
            gtk_window_move(GTK_WINDOW(w), self->work_area.x, self->work_area.y);
            self->apply_always_on_top();
            self->update_strut(w);
            return FALSE;
        }), sb);

    g_signal_connect(sb->window, "configure-event",
        G_CALLBACK(+[](GtkWidget* w, GdkEventConfigure* ev, gpointer data) -> gboolean {
            auto* self = static_cast<Sidebar*>(data);
            if (ev->width >= SIDEBAR_WIDTH_MIN)
                self->current_width = ev->width;

            if (!self->closing)
                self->update_strut(w);

            self->update_layout_classes(self->current_width);
            return FALSE;
        }), sb);

    g_signal_connect(sb->window, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* self = static_cast<Sidebar*>(data);
            if (self->guard_source) {
                g_source_remove(self->guard_source);
                self->guard_source = 0;
            }
            if (self->close_source) {
                g_source_remove(self->close_source);
                self->close_source = 0;
            }
        }), sb);

    gtk_widget_show_all(sb->window);

    // ◄── MODIFICA: show_all può rimettere can_focus sui widget appena
    // visibili, quindi riapplichiamo la policy di focus DOPO show_all.
    restrict_focus_recursive(GTK_WIDGET(sb->inner));

    // Carica layout salvato DOPO show_all: l'ordine e la visibilità
    // vengono applicati su widget già costruiti. load_layout() usa
    // set_no_show_all(TRUE) per impedire a future show_all di riaprirli.
    sb->load_layout();
    sb->apply_language();

    // Riapplica larghezza caricata se diversa dall'iniziale
    if (sb->current_width != SIDEBAR_WIDTH_INITIAL) {
        gtk_window_resize(GTK_WINDOW(sb->window),
                          sb->current_width, sb->work_area.height);
        sb->update_layout_classes(sb->current_width);
    }

    return sb;
}
