#include "audio_control_widget.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "../i18n.h"

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

void set_label_if_changed(GtkWidget* label,
                          std::string& previous,
                          const std::string& value) {
    if (value == previous)
        return;

    gtk_label_set_text(GTK_LABEL(label), value.c_str());
    previous = value;
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string part;

    for (const char character : text) {
        if (character == delimiter) {
            parts.push_back(part);
            part.clear();
        } else {
            part += character;
        }
    }

    parts.push_back(part);
    return parts;
}

bool parse_double(const std::string& text, double& value) {
    if (text.empty())
        return false;

    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end != text.c_str();
}

} // namespace

AudioControlWidget* AudioControlWidget::create() {
    auto* self = new AudioControlWidget();

    self->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_name(self->container, "audio-widget");

    GtkWidget* heading = gtk_label_new(nullptr);
    const std::string heading_markup =
        std::string("<span weight=\"bold\">") +
        _("Audio Control") +
        "</span>";

    gtk_label_set_markup(GTK_LABEL(heading), heading_markup.c_str());
    gtk_widget_set_name(heading, "audio-heading");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(self->container), heading, FALSE, FALSE, 0);

    // Copertina simbolica e metadati.
    GtkWidget* track_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(track_row, "audio-track-row");
    gtk_widget_set_valign(track_row, GTK_ALIGN_CENTER);

    GtkWidget* artwork = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(artwork, "audio-artwork");
    gtk_widget_set_size_request(artwork, 50, 50);
    gtk_widget_set_valign(artwork, GTK_ALIGN_CENTER);

    GtkWidget* artwork_label = gtk_label_new("♫");
    gtk_widget_set_name(artwork_label, "audio-artwork-label");
    gtk_widget_set_halign(artwork_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(artwork_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(artwork), artwork_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(track_row), artwork, FALSE, FALSE, 0);

    GtkWidget* metadata = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_name(metadata, "audio-metadata");
    gtk_widget_set_valign(metadata, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(metadata, TRUE);

    self->lbl_song = gtk_label_new(_("No track"));
    self->lbl_artist = gtk_label_new("");
    self->lbl_album = gtk_label_new("");

    gtk_widget_set_name(self->lbl_song, "audio-song");
    gtk_widget_set_name(self->lbl_artist, "audio-artist");
    gtk_widget_set_name(self->lbl_album, "audio-album");

    const std::vector<GtkWidget*> metadata_labels = {
        self->lbl_song,
        self->lbl_artist,
        self->lbl_album
    };

    for (GtkWidget* label : metadata_labels) {
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(metadata), label, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(track_row), metadata, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(self->container), track_row, FALSE, FALSE, 0);

    self->progress_playback = gtk_progress_bar_new();
    gtk_widget_set_name(self->progress_playback, "audio-progress");
    gtk_progress_bar_set_show_text(
        GTK_PROGRESS_BAR(self->progress_playback), FALSE);
    gtk_box_pack_start(
        GTK_BOX(self->container), self->progress_playback, FALSE, FALSE, 0);

    GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(controls, "audio-controls");
    gtk_widget_set_halign(controls, GTK_ALIGN_CENTER);

    GtkWidget* previous = gtk_button_new_with_label("⏮");
    self->btn_play_pause = gtk_button_new_with_label("▶");
    GtkWidget* stop = gtk_button_new_with_label("■");
    GtkWidget* next = gtk_button_new_with_label("⏭");

    gtk_widget_set_name(previous, "audio-prev-button");
    gtk_widget_set_name(self->btn_play_pause, "audio-play-button");
    gtk_widget_set_name(stop, "audio-stop-button");
    gtk_widget_set_name(next, "audio-next-button");

    const std::vector<GtkWidget*> buttons = {
        previous,
        self->btn_play_pause,
        stop,
        next
    };

    for (GtkWidget* button : buttons) {
        gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
        gtk_box_pack_start(GTK_BOX(controls), button, FALSE, FALSE, 0);
    }

    gtk_widget_set_size_request(previous, 34, 34);
    gtk_widget_set_size_request(stop, 34, 34);
    gtk_widget_set_size_request(next, 34, 34);
    gtk_widget_set_size_request(self->btn_play_pause, 42, 42);

    gtk_widget_set_tooltip_text(previous, _("Previous track"));
    gtk_widget_set_tooltip_text(self->btn_play_pause, _("Play or pause"));
    gtk_widget_set_tooltip_text(stop, _("Stop playback"));
    gtk_widget_set_tooltip_text(next, _("Next track"));

    g_signal_connect(
        previous, "clicked",
        G_CALLBACK(on_previous_clicked), self);
    g_signal_connect(
        self->btn_play_pause, "clicked",
        G_CALLBACK(on_play_pause_clicked), self);
    g_signal_connect(
        stop, "clicked",
        G_CALLBACK(on_stop_clicked), self);
    g_signal_connect(
        next, "clicked",
        G_CALLBACK(on_next_clicked), self);

    gtk_box_pack_start(GTK_BOX(self->container), controls, FALSE, FALSE, 0);

    GtkWidget* volume_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* volume_icon = gtk_label_new("🔊");

    gtk_widget_set_name(volume_icon, "audio-volume-icon");
    gtk_widget_set_valign(volume_icon, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(volume_icon, _("Volume"));
    gtk_box_pack_start(GTK_BOX(volume_row), volume_icon, FALSE, FALSE, 0);

    self->progress_volume = gtk_scale_new_with_range(
        GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);

    gtk_widget_set_name(self->progress_volume, "audio-volume");
    gtk_widget_set_hexpand(self->progress_volume, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(self->progress_volume), FALSE);
    gtk_widget_set_size_request(self->progress_volume, 100, 24);
    gtk_widget_set_tooltip_text(self->progress_volume, _("Volume"));

    g_signal_connect(
        self->progress_volume, "value-changed",
        G_CALLBACK(on_volume_changed), self);

    gtk_box_pack_start(
        GTK_BOX(volume_row), self->progress_volume, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(self->container), volume_row, FALSE, FALSE, 0);

    // Aggiorna i metadati ogni 3 secondi.
    self->update_timer_id = g_timeout_add_seconds(3, on_update_tick, self);

    self->update();
    return self;
}

std::string AudioControlWidget::read_playerctl(const char* command) {
    gchar* output = nullptr;
    gchar* error_output = nullptr;
    gint exit_status = 0;

    const gboolean ok = g_spawn_command_line_sync(
        command,
        &output,
        &error_output,
        &exit_status,
        nullptr);

    std::string result;
    if (ok && exit_status == 0 && output)
        result = trim(output);

    g_free(output);
    g_free(error_output);
    return result;
}

void AudioControlWidget::set_language(const std::string& lang) {
    language = lang;

    // Forza l'aggiornamento della label alla prossima chiamata a update().
    last_title.clear();
    update();
}

void AudioControlWidget::run_playerctl(const char* command) {
    gchar* error_output = nullptr;
    gint exit_status = 0;

    const gboolean ok = g_spawn_command_line_sync(
        command,
        nullptr,
        &error_output,
        &exit_status,
        nullptr);

    if (!ok || exit_status != 0) {
        g_warning(
            "Comando playerctl non riuscito%s%s",
            error_output ? ": " : "",
            error_output ? error_output : "");
    }

    g_free(error_output);
}

void AudioControlWidget::on_play_pause_clicked(
    GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl play-pause");
}

void AudioControlWidget::on_previous_clicked(
    GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl previous");
}

void AudioControlWidget::on_next_clicked(
    GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl next");
}

void AudioControlWidget::on_stop_clicked(
    GtkButton*, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (self && self->alive)
        self->run_playerctl("playerctl stop");
}

void AudioControlWidget::on_volume_changed(
    GtkRange* range, gpointer data) {
    auto* self = static_cast<AudioControlWidget*>(data);
    if (!self || !self->alive || self->updating_controls)
        return;

    const double volume = gtk_range_get_value(range);

    gchar command[64];
    g_snprintf(command, sizeof(command), "playerctl volume %.2f", volume);
    self->run_playerctl(command);
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
        "'{{status}}|{{title}}|{{artist}}|{{album}}|{{volume}}|"
        "{{position}}|{{mpris:length}}'");

    if (metadata.empty()) {
        set_label_if_changed(lbl_song, last_title, _("No track"));
        set_label_if_changed(lbl_artist, last_artist, "");
        set_label_if_changed(lbl_album, last_album, "");

        if (last_status != "Stopped") {
            gtk_button_set_label(GTK_BUTTON(btn_play_pause), "▶");
            last_status = "Stopped";
        }

        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(progress_playback), 0.0);
        return;
    }

    const std::vector<std::string> parts = split(metadata, '|');

    const std::string status =
        parts.size() > 0 ? parts[0] : "";
    const std::string title =
        parts.size() > 1 ? parts[1] : "";
    const std::string artist =
        parts.size() > 2 ? parts[2] : "";
    const std::string album =
        parts.size() > 3 ? parts[3] : "";
    const std::string volume_text =
        parts.size() > 4 ? parts[4] : "";
    const std::string position_text =
        parts.size() > 5 ? parts[5] : "";
    const std::string length_text =
        parts.size() > 6 ? parts[6] : "";

    set_label_if_changed(
        lbl_song, last_title, title.empty() ? _("No track") : title);
    set_label_if_changed(lbl_artist, last_artist, artist);
    set_label_if_changed(lbl_album, last_album, album);

    if (status != last_status) {
        gtk_button_set_label(
            GTK_BUTTON(btn_play_pause),
            status == "Playing" ? "⏸" : "▶");
        last_status = status;
    }

    // Volume.
    if (!volume_text.empty() && volume_text != last_volume) {
        double volume = 0.0;

        if (parse_double(volume_text, volume)) {
            updating_controls = true;
            gtk_range_set_value(
                GTK_RANGE(progress_volume),
                std::clamp(volume, 0.0, 1.0));
            updating_controls = false;
        }

        last_volume = volume_text;
    }

    // Avanzamento del brano, solo durante la riproduzione.
    if (status == "Playing" &&
        !position_text.empty() &&
        !length_text.empty()) {
        double position = 0.0;
        double length_us = 0.0;

        if (parse_double(position_text, position) &&
            parse_double(length_text, length_us)) {
            const double length_seconds = length_us / 1000000.0;

            if (length_seconds > 0.0) {
                gtk_progress_bar_set_fraction(
                    GTK_PROGRESS_BAR(progress_playback),
                    std::clamp(position / length_seconds, 0.0, 1.0));
            }
        }
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
