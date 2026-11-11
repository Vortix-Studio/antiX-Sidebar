#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <gtk/gtk.h>
#include <string>
#include <fstream>
#include <filesystem>

// Forward declarations per evitare inclusioni circolari
class AudioControlWidget;
class CalendarWidget;
class WeatherWidget;
class NewsWidget;

class Sidebar {
public:
    GtkWidget* window = nullptr;
    GtkWidget* inner = nullptr;
    GtkWidget* grip = nullptr;
    GtkWidget* close_btn = nullptr;

    GtkCssProvider* css_provider = nullptr;
    GtkWidget* customize_btn = nullptr;

    GtkWidget* audio_widget() const;
    GtkWidget* calendar_widget() const;
    GtkWidget* weather_widget() const;
    GtkWidget* news_widget() const;
    void apply_widget_order(const int order[4]);

    bool audio_visible = true;
    bool calendar_visible = true;
    bool weather_visible = true;
    bool news_visible = true;

    std::string language = "it";
    std::string current_theme = "dark-navy";

    void apply_theme(const std::string& theme_id);
    void apply_css_file(const char* path);
    void apply_language();
    static void on_customize(GtkButton*, gpointer data);

    AudioControlWidget* audio = nullptr;
    CalendarWidget* calendar = nullptr;
    WeatherWidget* weather = nullptr;
    NewsWidget* news = nullptr;

    GdkRectangle work_area = {0, 0, 0, 0};
    bool struts_enabled = false;
    int current_width = 230;
    int last_strut_width = -1;
    bool closing = false;

    guint guard_source = 0;
    guint close_source = 0;

    static Sidebar* create(GdkRectangle work_area, bool struts_enabled);
    void request_close();
    void clear_strut();
    void update_strut(GtkWidget* window);
    void apply_always_on_top();
    void setup_window_hints(GtkWidget* window, GdkRectangle work_area);
    void apply_width(int width);
    void update_layout_classes(int width);

    GtkWidget* make_header();
    GtkWidget* make_separator();
    GtkWidget* make_resize_grip();
    void save_layout();
    void load_layout();
    std::string layout_path() const;
};

#endif // SIDEBAR_H
