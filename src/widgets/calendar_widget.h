#ifndef CALENDAR_WIDGET_H
#define CALENDAR_WIDGET_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

class CalendarWidget {
public:
    struct Event {
        std::string summary;
        std::string source;     // nome del calendario di origine
        std::string description;
        int year;
        int month;
        int day;
        int hour = 0;           // validi se has_time
        int minute = 0;
        bool has_time = false;  // DTSTART con orario (T...)
    };

    static CalendarWidget* create(); 

    GtkWidget* get_widget() const { return container; }
    GtkWidget* get_container() const { return container; }

    void set_language(const std::string& lang);
    void update_calendar();
    void add_ics_source(const std::string& name, const std::string& url);

private:
    struct Source {
        std::string name;
        std::string url;
    };

    CalendarWidget() = default;

    static void on_previous_clicked(GtkButton*, gpointer data);
    static void on_next_clicked(GtkButton*, gpointer data);
    static void on_today_clicked(GtkButton*, gpointer data);
    static void on_refresh_clicked(GtkButton*, gpointer data);
    static void on_calendars_clicked(GtkButton*, gpointer data);
    static void on_add_calendar_clicked(GtkButton*, gpointer data);
    static void on_add_source_clicked(GtkButton*, gpointer data);
    static void on_remove_source_clicked(GtkButton*, gpointer data);
    static void on_rename_source_clicked(GtkButton*, gpointer data);
    static gboolean on_grid_draw(GtkWidget*, cairo_t*, gpointer data);
    static gboolean on_grid_button_press(GtkWidget*, GdkEventButton*, gpointer data);

    static gpointer download_worker(gpointer data);
    static gboolean finish_download(gpointer data);

    void show_events_dialog();
    void show_calendars_dialog();
    void refresh_calendar_marks();
    void download_all_sources();
    void load_sources();
    void save_sources();

    GtkWidget* container = nullptr;
    GtkWidget* btn_prev = nullptr;
    GtkWidget* btn_next = nullptr;
    GtkWidget* btn_today = nullptr;
    GtkWidget* btn_refresh = nullptr;
    GtkWidget* btn_calendars = nullptr;
    GtkWidget* lbl_month = nullptr;
    GtkWidget* lbl_hint = nullptr;
    GtkWidget* grid = nullptr; // sostituisce GtkCalendar

    // Mese visibile e giorno selezionato (0 = nessuno)
    int view_year = 0;
    int view_month = 0; // 0-11
    int selected_day = 0;

    std::vector<Event> events;
    std::vector<Source> sources;
    std::string language;
    bool download_in_progress = false;
};

#endif
