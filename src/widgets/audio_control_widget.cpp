#include "audio_control_widget.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <cstring>

#include "../i18n.h"

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;

    while (std::getline(stream, part, delimiter))
        parts.push_back(part);

    return parts;
}

bool parse_double(const std::string& text, double& value) {
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0';
}

void set_label(GtkWidget* label,
               std::string& previous,
               const std::string& value) {
    if (value != previous) {
        gtk_label_set_text(GTK_LABEL(label), value.c_str());
        previous = value;
    }
}

void set_volume_label(GtkWidget* label, double volume) {
    const int percent = static_cast<int>(
        std::clamp(volume, 0.0, 1.0) * 100.0 + 0.5);

    const std::string text = std::to_string(percent) + "%";
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
}

bool get_current_player_volume(double& volume) {
    gchar* output = nullptr;
    gint status = 0;

    if (!g_spawn_command_line_sync(
            "pactl list short sink-inputs",
            &output, nullptr, &status, nullptr) ||
        status != 0 || !output) {
        g_free(output);
        return false;
    }

    std::istringstream rows(output);
    g_free(output);

    unsigned id = 0;
    std::string line;
    bool found = false;

    while (std::getline(rows, line)) {
        std::istringstream columns(line);
        if (columns >> id) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    gchar* command = g_strdup_printf(
        "pactl get-sink-input-volume %u", id);

    output = nullptr;
    status = 0;
    const gboolean ok = g_spawn_command_line_sync(
        command, &output, nullptr, &status, nullptr);
    g_free(command);

    if (!ok || status != 0 || !output) {
        g_free(output);
        return false;
    }

    // Esempio di output: "Volume: front-left: ... / 65% / ..."
    char* percent = std::strchr(output, '%');
    if (!percent) {
        g_free(output);
        return false;
    }

    char* start = percent;
    while (start > output && *(start - 1) >= '0' && *(start - 1) <= '9')
        --start;

    if (start == percent) {
        g_free(output);
        return false;
    }

    const double value = std::strtod(start, nullptr);
    g_free(output);

    volume = std::clamp(value / 100.0, 0.0, 1.0);
    return true;
}


bool set_all_players_volume(double volume) {
    gchar* output = nullptr;
    gint status = 0;

    if (!g_spawn_command_line_sync(
            "pactl list short sink-inputs",
            &output, nullptr, &status, nullptr) ||
        status != 0 || !output) {
        g_free(output);
        return false;
    }

    std::istringstream rows(output);
    g_free(output);

    const int percent = static_cast<int>(
        std::clamp(volume, 0.0, 1.0) * 100.0 + 0.5);

    bool found = false;
    bool succeeded = true;
    std::string line;

    while (std::getline(rows, line)) {
        std::istringstream columns(line);
        unsigned id = 0;

        if (!(columns >> id))
            continue;

        found = true;
        gchar* command = g_strdup_printf(
            "pactl set-sink-input-volume %u %d%%", id, percent);

        gint command_status = 0;
        const gboolean ok = g_spawn_command_line_sync(
            command, nullptr, nullptr, &command_status, nullptr);

        g_free(command);

        if (!ok || command_status != 0)
            succeeded = false;
    }

    return found && succeeded;
}

} // namespace

AudioControlWidget* AudioControlWidget::create() {
    auto* self = new AudioControlWidget();

    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_name(self->container, "audio-widget");

    GtkWidget* heading = gtk_label_new(_("Audio Control"));
    self->lbl_heading = heading;
    gtk_widget_set_name(heading, "audio-heading");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(self->container), heading, FALSE, FALSE, 0);

    GtkWidget* track_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(track_row, "audio-track-row");

    GtkWidget* artwork = gtk_label_new("♫");
    gtk_widget_set_size_request(artwork, 50, 50);
    gtk_box_pack_start(GTK_BOX(track_row), artwork, FALSE, FALSE, 0);

    GtkWidget* metadata = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(metadata, TRUE);

    self->lbl_song = gtk_label_new(_("No track"));
    self->lbl_artist = gtk_label_new("");
    self->lbl_album = gtk_label_new("");

    GtkWidget* labels[] = {
        self->lbl_song,
        self->lbl_artist,
        self->lbl_album
    };

    for (GtkWidget* label : labels) {
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(metadata), label, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(track_row), metadata, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(self->container), track_row, FALSE, FALSE, 0);

    self->progress_playback = gtk_progress_bar_new();
    gtk_widget_set_name(self->progress_playback, "audio-progress");
    gtk_box_pack_start(
        GTK_BOX(self->container),
        self->progress_playback,
        FALSE,
        FALSE,
        0);

    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(controls, GTK_ALIGN_CENTER);

    self->btn_prev = gtk_button_new_with_label("⏮");
    self->btn_play_pause = gtk_button_new_with_label("▶");
    self->btn_stop = gtk_button_new_with_label("■");
    self->btn_next = gtk_button_new_with_label("⏭");

    gtk_widget_set_name(self->btn_prev, "audio-prev-button");
    gtk_widget_set_name(self->btn_play_pause, "audio-play-button");
    gtk_widget_set_name(self->btn_stop, "audio-stop-button");
    gtk_widget_set_name(self->btn_next, "audio-next-button");

    GtkWidget* buttons[] = {
        self->btn_prev,
        self->btn_play_pause,
        self->btn_stop,
        self->btn_next
    };

    for (GtkWidget* button : buttons) {
        gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        gtk_box_pack_start(GTK_BOX(controls), button, FALSE, FALSE, 0);
    }

    gtk_widget_set_tooltip_text(self->btn_prev, _("Previous track"));
    gtk_widget_set_tooltip_text(self->btn_play_pause, _("Play or pause"));
    gtk_widget_set_tooltip_text(self->btn_stop, _("Stop playback"));
    gtk_widget_set_tooltip_text(self->btn_next, _("Next track"));

    g_signal_connect(
        self->btn_prev, "clicked", G_CALLBACK(on_previous_clicked), self);
    g_signal_connect(
        self->btn_play_pause, "clicked",
        G_CALLBACK(on_play_pause_clicked), self);
    g_signal_connect(
        self->btn_stop, "clicked", G_CALLBACK(on_stop_clicked), self);
    g_signal_connect(
        self->btn_next, "clicked", G_CALLBACK(on_next_clicked), self);

    gtk_box_pack_start(GTK_BOX(self->container), controls, FALSE, FALSE, 0);

    GtkWidget* volume_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    self->icon_volume = gtk_label_new("🔊");
    gtk_widget_set_tooltip_text(self->icon_volume, _("Volume"));
    gtk_box_pack_start(GTK_BOX(volume_row), self->icon_volume, FALSE, FALSE, 0);

    self->progress_volume = gtk_scale_new_with_range(
        GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_widget_set_name(self->progress_volume, "audio-volume");
    gtk_widget_set_hexpand(self->progress_volume, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(self->progress_volume), FALSE);

    self->lbl_volume = gtk_label_new("0%");
    gtk_widget_set_size_request(self->lbl_volume, 40, -1);
    gtk_widget_set_halign(self->lbl_volume, GTK_ALIGN_END);

    // Legge il volume prima di collegare il segnale, così l'inizializzazione
    // dello slider non richiama on_volume_changed().
    double initial_volume = 0.0;
    get_current_player_volume(initial_volume);

    gtk_range_set_value(
        GTK_RANGE(self->progress_volume), initial_volume);
    set_volume_label(self->lbl_volume, initial_volume);

    g_signal_connect(
        self->progress_volume,
        "value-changed",
        G_CALLBACK(on_volume_changed),
        self);

    gtk_box_pack_start(
        GTK_BOX(volume_row), self->progress_volume, TRUE, TRUE, 0);
    gtk_box_pack_start(
        GTK_BOX(volume_row), self->lbl_volume, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(self->container), volume_row, FALSE, FALSE, 0);

    self->update_timer_id = g_timeout_add_seconds(
        3, on_update_tick, self);

    self->update();
    return self;
}


std::string AudioControlWidget::read_playerctl(const char* command) {
    gchar* output = nullptr;
    gint status = 0;

    const gboolean ok = g_spawn_command_line_sync(
        command, &output, nullptr, &status, nullptr);

    std::string result;
    if (ok && status == 0 && output)
        result = trim(output);

    g_free(output);
    return result;
}

void AudioControlWidget::run_playerctl(const char* command) {
    gint status = 0;

    if (!g_spawn_command_line_sync(
            command, nullptr, nullptr, &status, nullptr) ||
        status != 0) {
        g_warning("Comando playerctl non riuscito: %s", command);
    }
}

void AudioControlWidget::set_language(const std::string& lang) {
    language = lang;

    if (lbl_heading)
        gtk_label_set_text(GTK_LABEL(lbl_heading), _("Audio Control"));

    if (btn_prev)
        gtk_widget_set_tooltip_text(btn_prev, _("Previous track"));
    if (btn_play_pause)
        gtk_widget_set_tooltip_text(btn_play_pause, _("Play or pause"));
    if (btn_stop)
        gtk_widget_set_tooltip_text(btn_stop, _("Stop playback"));
    if (btn_next)
        gtk_widget_set_tooltip_text(btn_next, _("Next track"));
    if (icon_volume)
        gtk_widget_set_tooltip_text(icon_volume, _("Volume"));

    last_title.clear();
    update();
}

void AudioControlWidget::on_play_pause_clicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl play-pause");
}

void AudioControlWidget::on_previous_clicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl previous");
}

void AudioControlWidget::on_next_clicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl next");
}

void AudioControlWidget::on_stop_clicked(GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl stop");
}

void AudioControlWidget::on_volume_changed(GtkRange* range, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (!self || !self->alive)
        return;

    const double volume = gtk_range_get_value(range);
    set_volume_label(self->lbl_volume, volume);

    if (!set_all_players_volume(volume))
        g_warning("Impossibile impostare il volume con pactl");
}

gboolean AudioControlWidget::on_update_tick(gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (!self || !self->alive)
        return G_SOURCE_REMOVE;

    self->update();
    return G_SOURCE_CONTINUE;
}

void AudioControlWidget::update() {
    const std::string metadata = read_playerctl(
        "playerctl metadata --format "
        "'{{status}}|{{title}}|{{artist}}|{{album}}|"
        "{{position}}|{{mpris:length}}'");

    if (metadata.empty()) {
        set_label(lbl_song, last_title, _("No track"));
        set_label(lbl_artist, last_artist, "");
        set_label(lbl_album, last_album, "");

        if (last_status != "Stopped") {
            gtk_button_set_label(GTK_BUTTON(btn_play_pause), "▶");
            last_status = "Stopped";
        }

        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(progress_playback), 0.0);
        return;
    }

    const auto parts = split(metadata, '|');
    const auto field = [&parts](std::size_t index) -> std::string {
        return index < parts.size() ? parts[index] : "";
    };

    const std::string status = field(0);
    const std::string title = field(1);
    const std::string artist = field(2);
    const std::string album = field(3);

    set_label(lbl_song, last_title, title.empty() ? _("No track") : title);
    set_label(lbl_artist, last_artist, artist);
    set_label(lbl_album, last_album, album);

    if (status != last_status) {
        gtk_button_set_label(
            GTK_BUTTON(btn_play_pause),
            status == "Playing" ? "⏸" : "▶");
        last_status = status;
    }

    double position = 0.0;
    double length_us = 0.0;

    if (status == "Playing" &&
        parse_double(field(4), position) &&
        parse_double(field(5), length_us) &&
        length_us > 0.0) {
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(progress_playback),
            std::clamp(position / (length_us / 1000000.0), 0.0, 1.0));
    } else if (status != "Playing") {
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(progress_playback), 0.0);
    }
}

void AudioControlWidget::free_resources() {
    if (!alive)
        return;

    alive = false;

    if (update_timer_id != 0) {
        g_source_remove(update_timer_id);
        update_timer_id = 0;
    }

    delete this;
}
