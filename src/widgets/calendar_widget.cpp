#include "calendar_widget.h"

#include <curl/curl.h>
#include <libintl.h>

#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#define _(s) dgettext("antix-sidebar", s)
#define N_(s) s

static const char* MONTH_MSGIDS[12] = {
    N_("January"), N_("February"), N_("March"), N_("April"),
    N_("May"), N_("June"), N_("July"), N_("August"),
    N_("September"), N_("October"), N_("November"), N_("December")
};

// Prima Settimana che inizia col lunedì
static const char* WEEKDAY_MSGIDS[7] = {
    N_("Mon"), N_("Tue"), N_("Wed"), N_("Thu"), N_("Fri"), N_("Sat"), N_("Sun")
};

namespace {

struct DownloadTask {
    CalendarWidget* widget;
    std::vector<std::pair<std::string, std::string>> sources; // (nome, url)
    std::vector<CalendarWidget::Event> events;
};

struct CurlBuffer {
    std::string data;
};

struct SourceRow {
    CalendarWidget* cw;
    int index;
    GtkWidget* dialog;
};

std::string sources_config_path()
{
    return std::string(g_get_user_config_dir()) +
           G_DIR_SEPARATOR_S + "antix-sidebar" +
           G_DIR_SEPARATOR_S + "calendars.conf";
}

GtkWidget* make_icon_button(const char* icon_name, const char* tooltip)
{
    GtkWidget* btn = gtk_button_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_name(btn, "calendar-icon-btn");
    // Anche come classe: alcuni temi rispettano solo i selettori di classe
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn), "calendar-icon-btn");
    gtk_widget_set_tooltip_text(btn, tooltip);
    return btn;
}

void install_icon_css()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(
        provider,
        // Icone symbolic: il colore viene dal "color" del contesto.
        // Doppio selettore (nome e classe) + image interna per massima compatibilità
        "#calendar-icon-btn, .calendar-icon-btn { color: #ffffff; }\n"
        "#calendar-icon-btn image, .calendar-icon-btn image\n"
        "{ color: #ffffff; -gtk-icon-shadow: none; }\n",
        -1,
        nullptr
    );

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

size_t write_to_curl_buffer(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buffer = static_cast<CurlBuffer*>(userdata);
    const size_t total = size * nmemb;
    buffer->data.append(ptr, total);
    return total;
}

std::string unescape_ics_text(std::string value)
{
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            ++i;
            switch (value[i]) {
            case 'n':
            case 'N':
                result.push_back('\n');
                break;
            case ',':
            case ';':
            case '\\':
                result.push_back(value[i]);
                break;
            default:
                result.push_back(value[i]);
                break;
            }
        } else {
            result.push_back(value[i]);
        }
    }

    return result;
}

// Distruttore compatibile con GClosureNotify: firma esatta, nessun cast
void free_source_row(gpointer data, GClosure*)
{
    g_free(data);
}

// Prima lettera (UTF-8) dell'abbreviazione tradotta: "Lun" -> "L"
std::string weekday_initial(int i)
{
    const char* abbr = _(WEEKDAY_MSGIDS[i]);
    if (!abbr || !*abbr)
        return "?";
    const char* end = g_utf8_next_char(abbr);
    return std::string(abbr, static_cast<size_t>(end - abbr));
}

// Gestisce DTSTART;VALUE=DATE:20261109 e DTSTART:20261109T120000Z (+ 120000Z).
// Restituisce anche ora/minuti se presenti.
bool parse_ics_datetime(const std::string& line,
                        int& year, int& month, int& day,
                        int& hour, int& minute, bool& has_time)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos || colon + 1 >= line.size())
        return false;

    const std::string value = line.substr(colon + 1);
    if (value.size() < 8)
        return false;

    for (int i = 0; i < 8; ++i) {
        if (value[i] < '0' || value[i] > '9')
            return false;
    }

    year = std::atoi(value.substr(0, 4).c_str());
    month = std::atoi(value.substr(4, 2).c_str());
    day = std::atoi(value.substr(6, 2).c_str());

    has_time = false;
    hour = 0;
    minute = 0;

    // "T" seguita da HHMMSS (eventualmente con "Z" finale)
    if (value.size() >= 15 && (value[8] == 'T' || value[8] == 't')) {
        const std::string hhmmss = value.substr(9, 6);
        bool digits_ok = true;
        for (int i = 0; i < 6; ++i) {
            if (hhmmss[i] < '0' || hhmmss[i] > '9') {
                digits_ok = false;
                break;
            }
        }
        if (digits_ok) {
            hour = std::atoi(hhmmss.substr(0, 2).c_str());
            minute = std::atoi(hhmmss.substr(2, 2).c_str());
            has_time = hour <= 23 && minute <= 59;
        }
    }

    return year > 0 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

std::vector<std::string> unfold_ics_lines(const std::string& input)
{
    std::vector<std::string> lines;
    size_t pos = 0;

    while (pos < input.size()) {
        size_t end = input.find('\n', pos);
        if (end == std::string::npos)
            end = input.size();

        std::string line = input.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (!line.empty() && (line[0] == ' ' || line[0] == '\t') && !lines.empty()) {
            lines.back() += line.substr(1);
        } else {
            lines.push_back(line);
        }

        pos = end + 1;
    }

    return lines;
}

void parse_ics(const std::string& data,
               const std::string& source_name,
               std::vector<CalendarWidget::Event>& events)
{
    const auto lines = unfold_ics_lines(data);

    bool in_event = false;
    bool have_start = false;
    int year = 0, month = 0, day = 0;
    int hour = 0, minute = 0;
    bool has_time = false;
    CalendarWidget::Event current;

    for (const std::string& line : lines) {
        if (line == "BEGIN:VEVENT") {
            in_event = true;
            have_start = false;
            current = CalendarWidget::Event{};
            current.source = source_name;
            continue;
        }

        if (line == "END:VEVENT") {
            if (in_event && have_start) {
                if (current.summary.empty())
                    current.summary = _("(Untitled event)");

                current.year = year;
                current.month = month;
                current.day = day;
                current.hour = hour;
                current.minute = minute;
                current.has_time = has_time;

                events.push_back(current);
            }

            in_event = false;
            continue;
        }

        if (!in_event)
            continue;

        if (line.rfind("DTSTART", 0) == 0) {
            have_start = parse_ics_datetime(
                line, year, month, day, hour, minute, has_time);
        } else if (line.rfind("SUMMARY", 0) == 0) {
            const size_t colon = line.find(':');
            if (colon != std::string::npos)
                current.summary = unescape_ics_text(line.substr(colon + 1));
        } else if (line.rfind("DESCRIPTION", 0) == 0) {
            const size_t colon = line.find(':');
            if (colon != std::string::npos)
                current.description =
                    unescape_ics_text(line.substr(colon + 1));
        }
    }
}

std::string trim_copy(const std::string& s)
{
    const size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// ---------- utilità calendario ----------

bool is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) // month 1-12
{
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year))
        return 29;
    return mdays[month - 1];
}

// 0 = lunedì ... 6 = domenica
int weekday_mon0(int year, int month, int day)
{
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    std::mktime(&t);
    return (t.tm_wday + 6) % 7;
}

void local_today(int& year, int& month, int& day) // month 1-12
{
    const std::time_t now = std::time(nullptr);
    std::tm lt{};
    localtime_r(&now, &lt);
    year = lt.tm_year + 1900;
    month = lt.tm_mon + 1;
    day = lt.tm_mday;
}

// Colori del calendario, decisi dal tema CSS:
// @define-color calendar_accent     ...  (oggi + cerchio di selezione)
// @define-color calendar_event_day  ...  (numero dei giorni con eventi)
// Fallback: theme_selected_bg_color, poi un viola fisso.
GdkRGBA calendar_accent(GtkStyleContext* sc)
{
    GdkRGBA c;
    if (gtk_style_context_lookup_color(sc, "calendar_accent", &c) ||
        gtk_style_context_lookup_color(sc, "theme_selected_bg_color", &c))
        return c;
    return {0.42, 0.29, 1.0, 1.0}; // viola di riserva, coerente col tema
}

GdkRGBA calendar_event_day(GtkStyleContext* sc, const GdkRGBA& accent)
{
    GdkRGBA c;
    if (gtk_style_context_lookup_color(sc, "calendar_event_day", &c))
        return c;
    return accent; // in assenza di definizione usa l'accento
}

} // namespace

CalendarWidget* CalendarWidget::create()
{
    auto* cw = new CalendarWidget{};

    install_icon_css(); // icone bianche su tutti i calendar-icon-btn

    local_today(cw->view_year, cw->view_month, cw->selected_day);
    --cw->view_month; // interno 0-11

    cw->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_name(cw->container, "calendar-box");
    gtk_widget_set_halign(cw->container, GTK_ALIGN_FILL);

    // ---------- header ----------
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(header, "calendar-header");
    gtk_widget_set_halign(header, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(header, TRUE);

    // --- navigazione mese, centrata ---
    cw->btn_prev = make_icon_button("go-previous-symbolic", _("Previous month"));
    gtk_widget_set_size_request(cw->btn_prev, 28, 28);
    g_signal_connect(
        cw->btn_prev, "clicked",
        G_CALLBACK(CalendarWidget::on_previous_clicked), cw
    );

    cw->lbl_month = gtk_label_new(nullptr);
    gtk_widget_set_name(cw->lbl_month, "calendar-month");
    gtk_widget_set_halign(cw->lbl_month, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(cw->lbl_month, GTK_ALIGN_CENTER);

    cw->btn_next = make_icon_button("go-next-symbolic", _("Next month"));
    gtk_widget_set_size_request(cw->btn_next, 28, 28);
    g_signal_connect(
        cw->btn_next, "clicked",
        G_CALLBACK(CalendarWidget::on_next_clicked), cw
    );

    GtkWidget* nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(nav_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(nav_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(nav_box), cw->btn_prev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), cw->lbl_month, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), cw->btn_next, FALSE, FALSE, 0);

    // --- pulsanti in alto a destra: Calendars, Today, Refresh ---
    GtkWidget* actions_right = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_valign(actions_right, GTK_ALIGN_CENTER);

    cw->btn_calendars =
        make_icon_button("x-office-calendar-symbolic", _("Calendars"));
    g_signal_connect(
        cw->btn_calendars, "clicked",
        G_CALLBACK(CalendarWidget::on_calendars_clicked), cw
    );
    gtk_box_pack_start(GTK_BOX(actions_right), cw->btn_calendars, FALSE, FALSE, 0);

    cw->btn_today = make_icon_button("go-today-symbolic", _("Today"));
    g_signal_connect(
        cw->btn_today, "clicked",
        G_CALLBACK(CalendarWidget::on_today_clicked), cw
    );
    gtk_box_pack_start(GTK_BOX(actions_right), cw->btn_today, FALSE, FALSE, 0);

    cw->btn_refresh = make_icon_button("view-refresh-symbolic", _("Refresh"));
    g_signal_connect(
        cw->btn_refresh, "clicked",
        G_CALLBACK(CalendarWidget::on_refresh_clicked), cw
    );
    gtk_box_pack_start(GTK_BOX(actions_right), cw->btn_refresh, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header), nav_box, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), actions_right, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cw->container), header, FALSE, FALSE, 0);

    // Griglia disegnata a mano: permette di colorare i giorni con eventi
    cw->grid = gtk_drawing_area_new();
    gtk_widget_set_name(cw->grid, "calendar-grid");
    gtk_widget_set_size_request(cw->grid, 250, 200);
    gtk_widget_add_events(cw->grid, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(cw->grid, "draw",
                     G_CALLBACK(CalendarWidget::on_grid_draw), cw);
    g_signal_connect(cw->grid, "button-press-event",
                     G_CALLBACK(CalendarWidget::on_grid_button_press), cw);

    gtk_box_pack_start(GTK_BOX(cw->container), cw->grid, FALSE, FALSE, 0);

    cw->lbl_hint = gtk_label_new(_("Double-click a day to see its events"));
    gtk_widget_set_name(cw->lbl_hint, "calendar-hint-label");
    gtk_widget_set_halign(cw->lbl_hint, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(cw->container), cw->lbl_hint, FALSE, FALSE, 0);

    // Carica i calendari salvati da ~/.config/antix-sidebar/calendars.conf
    cw->load_sources();

    cw->update_calendar();

    return cw;
}

void CalendarWidget::set_language(const std::string& lang)
{
    language = lang;

    if (btn_prev)
        gtk_widget_set_tooltip_text(btn_prev, _("Previous month"));
    if (btn_next)
        gtk_widget_set_tooltip_text(btn_next, _("Next month"));
    if (btn_today)
        gtk_widget_set_tooltip_text(btn_today, _("Today"));
    if (btn_refresh)
        gtk_widget_set_tooltip_text(btn_refresh, _("Refresh"));
    if (btn_calendars)
        gtk_widget_set_tooltip_text(btn_calendars, _("Calendars"));
    if (lbl_hint)
        gtk_label_set_text(GTK_LABEL(lbl_hint), _("Double-click a day to see its events"));

    update_calendar();
}

void CalendarWidget::update_calendar()
{
    if (!lbl_month)
        return;

    char month_buf[64];
    g_snprintf(
        month_buf,
        sizeof(month_buf),
        "%s %d",
        _(MONTH_MSGIDS[view_month]),
        view_year
    );

    gtk_label_set_text(GTK_LABEL(lbl_month), month_buf);
    refresh_calendar_marks();
}

// ---------- persistenza sorgenti ICS ----------

void CalendarWidget::load_sources()
{
    GKeyFile* kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, sources_config_path().c_str(),
                                   G_KEY_FILE_NONE, nullptr)) {
        g_key_file_free(kf);
        return;
    }

    gsize count = 0;
    gchar** groups = g_key_file_get_groups(kf, &count);
    for (gsize i = 0; i < count; ++i) {
        gchar* name = g_key_file_get_string(kf, groups[i], "name", nullptr);
        gchar* url  = g_key_file_get_string(kf, groups[i], "url", nullptr);
        if (name && url && *url) {
            sources.push_back({name, url});
        }
        g_free(name);
        g_free(url);
    }
    g_strfreev(groups);
    g_key_file_free(kf);

    if (!sources.empty())
        download_all_sources();
}

void CalendarWidget::save_sources()
{
    GKeyFile* kf = g_key_file_new();
    for (size_t i = 0; i < sources.size(); ++i) {
        char group[32];
        g_snprintf(group, sizeof(group), "source%zu", i);
        g_key_file_set_string(kf, group, "name", sources[i].name.c_str());
        g_key_file_set_string(kf, group, "url",  sources[i].url.c_str());
    }

    const std::string path = sources_config_path();
    gchar* dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    gsize len = 0;
    gchar* data = g_key_file_to_data(kf, &len, nullptr);
    if (data) {
        GError* err = nullptr;
        g_file_set_contents(path.c_str(), data, static_cast<gssize>(len), &err);
        if (err) {
            g_printerr("Calendar: salvataggio calendari fallito: %s\n",
                       err->message);
            g_error_free(err);
        }
        g_free(data);
    }
    g_key_file_free(kf);
}

void CalendarWidget::add_ics_source(const std::string& name, const std::string& url)
{
    const std::string clean_url = trim_copy(url);
    if (clean_url.empty())
        return;

    for (const auto& source : sources) {
        if (source.url == clean_url)
            return;
    }

    sources.push_back({name, clean_url});
    save_sources();
    download_all_sources();
}

void CalendarWidget::download_all_sources()
{
    if (download_in_progress || sources.empty())
        return;

    download_in_progress = true;

    auto* task = new DownloadTask;
    task->widget = this;

    for (const auto& source : sources)
        task->sources.emplace_back(source.name, source.url);

    g_thread_new(
        "calendar-download",
        CalendarWidget::download_worker,
        task
    );
}

gpointer CalendarWidget::download_worker(gpointer data)
{
    auto* task = static_cast<DownloadTask*>(data);

    CURL* curl = curl_easy_init();
    if (!curl) {
        g_printerr("Calendar: impossibile inizializzare libcurl "
                   "(curl_global_init è stato chiamato nel main?)\n");
        g_idle_add(CalendarWidget::finish_download, task);
        return nullptr;
    }

    for (const auto& source : task->sources) {
        CurlBuffer buffer;

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, source.second.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_curl_buffer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CalendarWidget/1.0");

        const CURLcode result = curl_easy_perform(curl);

        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (result == CURLE_OK &&
            response_code >= 200 &&
            response_code < 300) {
            g_printerr("Calendar: scaricato '%s' (%zu byte)\n",
                       source.second.c_str(), buffer.data.size());
            parse_ics(buffer.data, source.first, task->events);
        } else {
            g_printerr("Calendar: FALLITO '%s' -> curl: %s, HTTP %ld\n",
                       source.second.c_str(),
                       curl_easy_strerror(result),
                       response_code);
        }
    }

    curl_easy_cleanup(curl);
    g_idle_add(CalendarWidget::finish_download, task);
    return nullptr;
}

gboolean CalendarWidget::finish_download(gpointer data)
{
    auto* task = static_cast<DownloadTask*>(data);
    CalendarWidget* cw = task->widget;

    cw->events = std::move(task->events);
    cw->download_in_progress = false;
    cw->refresh_calendar_marks();

    g_printerr("Calendar: %zu eventi caricati\n", cw->events.size());

    delete task;
    return G_SOURCE_REMOVE;
}

void CalendarWidget::refresh_calendar_marks()
{
    // Niente più "marks" di GtkCalendar: basta ridisegnare la griglia
    if (grid)
        gtk_widget_queue_draw(grid);
}

// ---------- dialoghi ----------

void CalendarWidget::show_events_dialog()
{
    if (selected_day <= 0)
        return;

    const int year = view_year;
    const int month = view_month + 1;
    const int day = selected_day;

    std::vector<const Event*> day_events;
    for (const auto& event : events)
        if (event.year == year && event.month == month && event.day == day)
            day_events.push_back(&event);

    // Ordina per orario se disponibile, gli eventi a giornata intera in fondo
    std::sort(day_events.begin(), day_events.end(),
              [](const Event* a, const Event* b) {
                  if (a->has_time != b->has_time)
                      return a->has_time; // false (tutto il giorno) dopo
                  if (a->hour != b->hour)
                      return a->hour < b->hour;
                  return a->minute < b->minute;
              });

    char title_buf[128];
    g_snprintf(
        title_buf,
        sizeof(title_buf),
        _("Events for %02d/%02d/%04d"),
        day, month, year
    );

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 260);

    GtkWidget* close_btn =
        make_icon_button("window-close-symbolic", _("Close"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), close_btn, GTK_RESPONSE_CLOSE
    );
    gtk_widget_show(close_btn);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    if (day_events.empty()) {
        GtkWidget* label = gtk_label_new(_("No events"));
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 0);
    } else {
        GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_NEVER,
            GTK_POLICY_AUTOMATIC
        );
        gtk_widget_set_vexpand(scroll, TRUE);

        GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        for (const Event* event : day_events) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

            // Titolo: "HH:MM — Titolo" quando c'è l'orario
            char title_line[1024];
            if (event->has_time) {
                g_snprintf(title_line, sizeof(title_line),
                           "%02d:%02d — %s",
                           event->hour, event->minute,
                           event->summary.c_str());
            } else {
                g_snprintf(title_line, sizeof(title_line),
                           "%s", event->summary.c_str());
            }

            GtkWidget* lbl_title = gtk_label_new(title_line);
            gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(lbl_title), TRUE);
            gtk_widget_set_halign(lbl_title, GTK_ALIGN_FILL);
            gtk_box_pack_start(GTK_BOX(row), lbl_title, FALSE, FALSE, 0);

            // Descrizione, in piccolo e attenuata
            if (!event->description.empty()) {
                GtkWidget* lbl_desc =
                    gtk_label_new(event->description.c_str());
                gtk_label_set_xalign(GTK_LABEL(lbl_desc), 0.0f);
                gtk_label_set_line_wrap(GTK_LABEL(lbl_desc), TRUE);
                gtk_label_set_ellipsize(GTK_LABEL(lbl_desc),
                                        PANGO_ELLIPSIZE_END);
                PangoAttrList* attrs = pango_attr_list_new();
                pango_attr_list_insert(
                    attrs, pango_attr_scale_new(PANGO_SCALE_SMALL));
                pango_attr_list_insert(
                    attrs, pango_attr_foreground_alpha_new(45000));
                gtk_label_set_attributes(GTK_LABEL(lbl_desc), attrs);
                pango_attr_list_unref(attrs);
                gtk_box_pack_start(GTK_BOX(row), lbl_desc, FALSE, FALSE, 0);
            }

            // Origine in piccolo sotto
            if (!event->source.empty()) {
                GtkWidget* lbl_source =
                    gtk_label_new(event->source.c_str());
                gtk_label_set_xalign(GTK_LABEL(lbl_source), 0.0f);
                gtk_label_set_ellipsize(GTK_LABEL(lbl_source),
                                        PANGO_ELLIPSIZE_END);
                PangoAttrList* attrs = pango_attr_list_new();
                pango_attr_list_insert(
                    attrs, pango_attr_scale_new(PANGO_SCALE_SMALL));
                pango_attr_list_insert(
                    attrs, pango_attr_foreground_alpha_new(35000));
                gtk_label_set_attributes(GTK_LABEL(lbl_source), attrs);
                pango_attr_list_unref(attrs);
                gtk_box_pack_start(GTK_BOX(row), lbl_source, FALSE, FALSE, 0);
            }

            gtk_box_pack_start(GTK_BOX(list), row, FALSE, FALSE, 0);
        }

        gtk_container_add(GTK_CONTAINER(scroll), list);
        gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    }

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void CalendarWidget::on_remove_source_clicked(GtkButton*, gpointer data)
{
    auto* rd = static_cast<SourceRow*>(data);
    CalendarWidget* cw = rd->cw;

    if (rd->index >= 0 && rd->index < static_cast<int>(cw->sources.size())) {
        const std::string removed_name = cw->sources[rd->index].name;
        cw->sources.erase(cw->sources.begin() + rd->index);

        // Rimuovi anche gli eventi già scaricati da questa fonte
        cw->events.erase(
            std::remove_if(cw->events.begin(), cw->events.end(),
                           [&](const CalendarWidget::Event& e) {
                               return e.source == removed_name;
                           }),
            cw->events.end());

        cw->save_sources();
        cw->refresh_calendar_marks();
    }

    // NON distruggere il dialog qui: show_calendars_dialog() è dentro
    // gtk_dialog_run(). Rispondi e lascia che sia lei a gestire la riapertura.
    gtk_dialog_response(GTK_DIALOG(rd->dialog), GTK_RESPONSE_APPLY);
}

void CalendarWidget::on_rename_source_clicked(GtkButton*, gpointer data)
{
    auto* rd = static_cast<SourceRow*>(data);
    CalendarWidget* cw = rd->cw;

    if (rd->index < 0 || rd->index >= static_cast<int>(cw->sources.size()))
        return;

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("Rename calendar"));
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, -1);

    GtkWidget* cancel_btn =
        make_icon_button("process-stop-symbolic", _("Cancel"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), cancel_btn, GTK_RESPONSE_CANCEL
    );
    gtk_widget_show(cancel_btn);

    GtkWidget* accept_btn =
        make_icon_button("emblem-ok-symbolic", _("OK"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), accept_btn, GTK_RESPONSE_ACCEPT
    );
    gtk_widget_show(accept_btn);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), cw->sources[rd->index].name.c_str());
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const std::string name =
            trim_copy(gtk_entry_get_text(GTK_ENTRY(entry)));
        if (!name.empty()) {
            const std::string old_name =
                cw->sources[rd->index].name;

            cw->sources[rd->index].name = name;
            cw->save_sources();

            // Aggiorna al volo anche gli eventi già scaricati
            for (auto& event : cw->events) {
                if (event.source == old_name)
                    event.source = name;
            }
            cw->refresh_calendar_marks();
        }
    }

    gtk_widget_destroy(dialog);
}

void CalendarWidget::show_calendars_dialog()
{
    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("Saved calendars"));
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 220);

    GtkWidget* close_btn =
        make_icon_button("window-close-symbolic", _("Close"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), close_btn, GTK_RESPONSE_CLOSE
    );
    gtk_widget_show(close_btn);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 6);
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    // Riga "+" in alto a destra della lista
    GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget* add_btn =
        make_icon_button("list-add-symbolic", _("Add calendar"));
    auto* ad = static_cast<SourceRow*>(g_malloc(sizeof(SourceRow)));
    ad->cw = this;
    ad->index = -1;         // non usato da questo handler
    ad->dialog = dialog;
    g_signal_connect_data(
        add_btn, "clicked",
        G_CALLBACK(on_add_source_clicked),
        ad, free_source_row, (GConnectFlags)0
    );
    gtk_box_pack_end(GTK_BOX(top_row), add_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content), top_row, FALSE, FALSE, 0);

    if (sources.empty()) {
        GtkWidget* empty = gtk_label_new(_("No calendars configured"));
        gtk_widget_set_halign(empty, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(content), empty, FALSE, FALSE, 0);
    }

    for (size_t i = 0; i < sources.size(); ++i) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

        char text[1024];
        g_snprintf(text, sizeof(text), "%s — %s",
                   sources[i].name.c_str(), sources[i].url.c_str());

        GtkWidget* lbl = gtk_label_new(text);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);

        GtkWidget* rn = make_icon_button("document-edit-symbolic", _("Rename"));
        auto* rd1 = static_cast<SourceRow*>(g_malloc(sizeof(SourceRow)));
        rd1->cw = this;
        rd1->index = static_cast<int>(i);
        rd1->dialog = dialog;
        g_signal_connect_data(
            rn, "clicked",
            G_CALLBACK(on_rename_source_clicked),
            rd1, free_source_row, (GConnectFlags)0
        );
        gtk_box_pack_start(GTK_BOX(row), rn, FALSE, FALSE, 0);

        GtkWidget* rm = make_icon_button("list-remove-symbolic", _("Remove"));
        auto* rd2 = static_cast<SourceRow*>(g_malloc(sizeof(SourceRow)));
        rd2->cw = this;
        rd2->index = static_cast<int>(i);
        rd2->dialog = dialog;
        g_signal_connect_data(
            rm, "clicked",
            G_CALLBACK(on_remove_source_clicked),
            rd2, free_source_row, (GConnectFlags)0
        );
        gtk_box_pack_start(GTK_BOX(row), rm, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(dialog);

    // "Remove" e "+" rispondono APPLY: il sorgente è già stato aggiornato
    // dal gestore del segnale, riapri la finestra aggiornata.
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_APPLY)
        show_calendars_dialog();
}

void CalendarWidget::on_calendars_clicked(GtkButton*, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);
    cw->show_calendars_dialog();
}

// Il "+" nella dialog calendari: apre la dialog di aggiunta,
// poi riapre la lista aggiornata tramite la risposta APPLY.
void CalendarWidget::on_add_source_clicked(GtkButton*, gpointer data)
{
    auto* rd = static_cast<SourceRow*>(data);
    CalendarWidget::on_add_calendar_clicked(nullptr, rd->cw);
    gtk_dialog_response(GTK_DIALOG(rd->dialog), GTK_RESPONSE_APPLY);
}

void CalendarWidget::on_add_calendar_clicked(GtkButton*, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("Add network calendar"));
    gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, -1);

    GtkWidget* cancel_btn =
        make_icon_button("process-stop-symbolic", _("Cancel"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), cancel_btn, GTK_RESPONSE_CANCEL
    );
    gtk_widget_show(cancel_btn);

    GtkWidget* accept_btn =
        make_icon_button("list-add-symbolic", _("Add"));
    gtk_dialog_add_action_widget(
        GTK_DIALOG(dialog), accept_btn, GTK_RESPONSE_ACCEPT
    );
    gtk_widget_show(accept_btn);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget* lbl_name = gtk_label_new(_("Calendar name:"));
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);

    GtkWidget* name_entry = gtk_entry_new();
    gtk_widget_set_hexpand(name_entry, TRUE);
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(name_entry),
        _("My calendar")
    );

    GtkWidget* lbl_url = gtk_label_new(_("Paste the URL of an .ics calendar:"));
    gtk_widget_set_halign(lbl_url, GTK_ALIGN_START);

    GtkWidget* url_entry = gtk_entry_new();
    gtk_widget_set_hexpand(url_entry, TRUE);
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(url_entry),
        "https://example.org/calendar.ics"
    );

    gtk_box_pack_start(GTK_BOX(content), lbl_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), name_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), lbl_url, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), url_entry, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(name_entry);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar* url = gtk_entry_get_text(GTK_ENTRY(url_entry));
        const gchar* name = gtk_entry_get_text(GTK_ENTRY(name_entry));

        if (url && *url) {
            const std::string chosen = trim_copy(name ? name : "");
            cw->add_ics_source(
                chosen.empty() ? _("Network calendar") : chosen,
                url
            );
        }
    }

    gtk_widget_destroy(dialog);
}

// ---------- navigazione ----------

void CalendarWidget::on_previous_clicked(GtkButton*, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);

    if (cw->view_month == 0) {
        cw->view_month = 11;
        if (cw->view_year > 1)
            --cw->view_year;
    } else {
        --cw->view_month;
    }

    cw->selected_day = 0;
    cw->update_calendar();
}

void CalendarWidget::on_next_clicked(GtkButton*, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);

    if (cw->view_month == 11) {
        cw->view_month = 0;
        ++cw->view_year;
    } else {
        ++cw->view_month;
    }

    cw->selected_day = 0;
    cw->update_calendar();
}

void CalendarWidget::on_today_clicked(GtkButton*, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);

    int ty, tm, td;
    local_today(ty, tm, td);

    cw->view_year = ty;
    cw->view_month = tm - 1; // interno 0-11
    cw->selected_day = td;

    cw->update_calendar();
    gtk_widget_queue_draw(cw->grid);
}

void CalendarWidget::on_refresh_clicked(GtkButton*, gpointer data)
{
    // Riescarica tutti i sorgenti; se un download è già in corso
    // download_all_sources() esce senza fare nulla (guard interno)
    static_cast<CalendarWidget*>(data)->download_all_sources();
}

// ---------- disegno griglia ----------

gboolean CalendarWidget::on_grid_draw(GtkWidget* widget, cairo_t* cr, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    GtkStyleContext* sc = gtk_widget_get_style_context(widget);

    GdkRGBA fg;
    gtk_style_context_get_color(sc, GTK_STATE_FLAG_NORMAL, &fg);

    GdkRGBA dim = fg;
    dim.alpha *= 0.35;

    // Colori decisi dal tema CSS (@define-color nei file .css)
    const GdkRGBA accent = calendar_accent(sc);
    const GdkRGBA event_color = calendar_event_day(sc, accent);

    const double pad = 4.0;
    const double header_h = 20.0;
    const double cell_w = (alloc.width - 2.0 * pad) / 7.0;
    const double cell_h = (alloc.height - header_h - pad) / 6.0;

    // Riga dei giorni della settimana (solo l'iniziale: L M M G V S D)
    for (int i = 0; i < 7; ++i) {
        const std::string initial = weekday_initial(i);
        PangoLayout* l = gtk_widget_create_pango_layout(widget, initial.c_str());
        pango_layout_set_alignment(l, PANGO_ALIGN_CENTER);
        pango_layout_set_width(l, static_cast<int>(cell_w * PANGO_SCALE));

        cairo_set_source_rgba(cr, dim.red, dim.green, dim.blue, dim.alpha);
        cairo_move_to(cr, pad + i * cell_w, 2.0);
        pango_cairo_show_layout(cr, l);
        g_object_unref(l);
    }

    const int offset = weekday_mon0(cw->view_year, cw->view_month + 1, 1);
    const int dim_month = days_in_month(cw->view_year, cw->view_month + 1);

    int prev_month = cw->view_month;      // per i numeri dei giorni "grigi"
    int prev_year = cw->view_year;
    if (prev_month == 0) { prev_month = 12; --prev_year; }
    const int prev_days = days_in_month(prev_year, prev_month);

    // Data odierna per l'evidenziazione
    int ty, tm, td;
    local_today(ty, tm, td);

    for (int idx = 0; idx < 42; ++idx) {
        const int col = idx % 7;
        const int row = idx / 7;

        const int day_num = idx - offset + 1;
        const bool in_month = day_num >= 1 && day_num <= dim_month;

        const double cx = pad + col * cell_w + cell_w / 2.0;
        const double cy = header_h + row * cell_h + cell_h / 2.0;

        char buf[8];
        if (in_month) {
            g_snprintf(buf, sizeof(buf), "%d", day_num);
        } else if (day_num < 1) {
            g_snprintf(buf, sizeof(buf), "%d", prev_days + day_num);
        } else {
            g_snprintf(buf, sizeof(buf), "%d", day_num - dim_month);
        }

        PangoLayout* l = gtk_widget_create_pango_layout(widget, buf);

        PangoRectangle ext;
        pango_layout_get_extents(l, nullptr, &ext);
        const double tw = ext.width / static_cast<double>(PANGO_SCALE);
        const double th = ext.height / static_cast<double>(PANGO_SCALE);

        GdkRGBA text_color = in_month ? fg : dim;
        const bool has_event = in_month && std::any_of(
            cw->events.begin(), cw->events.end(),
            [&](const CalendarWidget::Event& e) {
                return e.year == cw->view_year &&
                       e.month == cw->view_month + 1 &&
                       e.day == day_num;
            });
        const bool is_sel = in_month && day_num == cw->selected_day;
        const bool is_today = in_month &&
                              cw->view_year == ty &&
                              cw->view_month + 1 == tm &&
                              day_num == td;

        const double r = std::min(cell_w, cell_h) * 0.38;

        // Oggi: cerchio pieno dietro il numero (il numero diventa bianco)
        if (is_today) {
            cairo_new_path(cr);
            cairo_set_source_rgba(cr, accent.red, accent.green,
                                  accent.blue, accent.alpha);
            cairo_arc(cr, cx, cy, r, 0.0, 2.0 * G_PI);
            cairo_fill(cr);
            text_color = {1.0, 1.0, 1.0, 1.0};
        } else if (has_event) {
            // Giorno con eventi: numero colorato dal tema, nessun cerchio
            text_color = event_color;
        }

        // Giorno selezionato: anello vuoto attorno al numero
        // (si sovrappone al cerchio pieno se oggi è anche selezionato)
        if (is_sel) {
            cairo_new_path(cr);
            cairo_set_source_rgba(cr, accent.red, accent.green,
                                  accent.blue, accent.alpha);
            cairo_set_line_width(cr, 1.6);
            cairo_arc(cr, cx, cy, r, 0.0, 2.0 * G_PI);
            cairo_stroke(cr);
        }

        cairo_new_path(cr);
        cairo_set_source_rgba(cr, text_color.red, text_color.green,
                              text_color.blue, text_color.alpha);
        cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0);
        pango_cairo_show_layout(cr, l);
        g_object_unref(l);
    }

    return FALSE;
}

gboolean CalendarWidget::on_grid_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data)
{
    auto* cw = static_cast<CalendarWidget*>(data);
    if (event->button != 1)
        return FALSE;

    const double pad = 4.0;
    const double header_h = 20.0;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    const double cell_w = (alloc.width - 2.0 * pad) / 7.0;
    const double cell_h = (alloc.height - header_h - pad) / 6.0;

    const int col = static_cast<int>((event->x - pad) / cell_w);
    const int row = static_cast<int>((event->y - header_h) / cell_h);
    if (col < 0 || col > 6 || row < 0 || row > 5)
        return TRUE;

    const int offset = weekday_mon0(cw->view_year, cw->view_month + 1, 1);
    const int day = row * 7 + col - offset + 1;

    if (day < 1 || day > days_in_month(cw->view_year, cw->view_month + 1))
        return TRUE;

    cw->selected_day = day;
    gtk_widget_queue_draw(cw->grid);

    if (event->type == GDK_2BUTTON_PRESS)
        cw->show_events_dialog();

    return TRUE;
}
