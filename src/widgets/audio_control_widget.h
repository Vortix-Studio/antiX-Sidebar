#pragma once

#include <gtk/gtk.h>
#include <string>

class AudioControlWidget {
public:
    static AudioControlWidget* create();
    GtkWidget* widget() const { return container; }
    void free_resources();
    void set_language(const std::string& lang);

private:
    AudioControlWidget() = default;
    ~AudioControlWidget() = default;

    static gboolean on_update_tick(gpointer data);
    static void on_play_pause_clicked(GtkButton*, gpointer data);
    static void on_previous_clicked(GtkButton*, gpointer data);
    static void on_next_clicked(GtkButton*, gpointer data);
    static void on_stop_clicked(GtkButton*, gpointer data);
    static void on_volume_changed(GtkRange*, gpointer data);

    static void run_playerctl(const char* command);
    static std::string read_playerctl(const char* command);

    void update();

    GtkWidget* container = nullptr;
    GtkWidget* lbl_song = nullptr;
    GtkWidget* lbl_artist = nullptr;
    GtkWidget* lbl_album = nullptr;
    GtkWidget* progress_playback = nullptr;
    GtkWidget* progress_volume = nullptr;
    GtkWidget* btn_play_pause = nullptr;
    GtkWidget* lbl_volume = nullptr;

    guint update_timer_id = 0;
    bool alive = true;

    std::string language = "it";
    std::string last_title;
    std::string last_artist;
    std::string last_album;
    std::string last_status;
};
