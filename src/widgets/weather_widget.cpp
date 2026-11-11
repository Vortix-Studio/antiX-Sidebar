#include "weather_widget.h"
#include <glib/gi18n.h>

#include <map>
#include <string>
#include <vector>

#ifndef ICON_SOURCE_DIR
#define ICON_SOURCE_DIR "src/icons"
#endif

#ifndef ICON_INSTALL_DIR
#define ICON_INSTALL_DIR "/usr/local/share/antix-sidebar/icons"
#endif

namespace {

std::string weatherConfigPath() {
    gchar* dir = g_build_filename(
        g_get_user_config_dir(),
        "antix-sidebar",
        nullptr
    );
    g_mkdir_with_parents(dir, 0700);
    gchar* path = g_build_filename(dir, "weather.ini", nullptr);
    std::string result(path);
    g_free(path);
    g_free(dir);
    return result;
}

std::string loadCity() {
    GKeyFile* file = g_key_file_new();
    const std::string path = weatherConfigPath();
    std::string city = "Roma, Italia";

    if (g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, nullptr)) {
        GError* error = nullptr;
        gchar* saved = g_key_file_get_string(file, "weather", "city", &error);
        if (saved && *saved) city = saved;
        g_free(saved);
        if (error) g_error_free(error);
    }
    g_key_file_unref(file);
    return city;
}

void saveCity(const std::string& city) {
    GKeyFile* file = g_key_file_new();
    g_key_file_set_string(file, "weather", "city", city.c_str());
    gsize size = 0;
    GError* error = nullptr;
    gchar* contents = g_key_file_to_data(file, &size, &error);
    const std::string path = weatherConfigPath();

    if (contents) {
        if (!g_file_set_contents(path.c_str(), contents, size, &error)) {
            g_printerr("Errore salvataggio località: %s\n",
                       error ? error->message : "errore sconosciuto");
        }
    }
    g_free(contents);
    if (error) g_error_free(error);
    g_key_file_unref(file);
}

std::string findIconPath(const char* filename) {
    if (!filename || !*filename) return {};
    std::vector<std::string> names;
    names.push_back(filename);

    std::string fn = filename;
    if (fn == "clear.png") names.push_back("sereno.png");
    else if (fn == "sereno.png") names.push_back("clear.png");
    else if (fn == "partly_cloudy.png") names.push_back("nuvoloso_parziale.png");
    else if (fn == "nuvoloso_parziale.png") names.push_back("partly_cloudy.png");
    else if (fn == "cloudy.png") names.push_back("nuvoloso.png");
    else if (fn == "nuvoloso.png") names.push_back("cloudy.png");
    else if (fn == "rain.png") names.push_back("pioggia.png");
    else if (fn == "pioggia.png") names.push_back("rain.png");
    else if (fn == "snow.png") names.push_back("neve.png");
    else if (fn == "neve.png") names.push_back("snow.png");
    else if (fn == "thunderstorm.png") names.push_back("temporale.png");
    else if (fn == "temporale.png") names.push_back("thunderstorm.png");

    std::vector<std::string> searchDirs;
    char* exe_link = g_file_read_link("/proc/self/exe", nullptr);
    if (exe_link) {
        gchar* exe_dir = g_path_get_dirname(exe_link);
        if (exe_dir) {
            searchDirs.push_back(std::string(exe_dir) + "/icons");
            searchDirs.push_back(std::string(exe_dir) + "/../src/icons");
            searchDirs.push_back(std::string(exe_dir) + "/../share/antix-sidebar/icons");
            g_free(exe_dir);
        }
        g_free(exe_link);
    }
    searchDirs.push_back("src/icons");
    searchDirs.push_back("icons");
    searchDirs.push_back(ICON_SOURCE_DIR);
    searchDirs.push_back(ICON_INSTALL_DIR);
    searchDirs.push_back("/usr/local/share/antix-sidebar/icons");
    searchDirs.push_back("/usr/share/antix-sidebar/icons");

    for (const auto& name : names) {
        for (const auto& dir : searchDirs) {
            std::string fullPath = dir + "/" + name;
            if (g_file_test(fullPath.c_str(), G_FILE_TEST_IS_REGULAR)) {
                return fullPath;
            }
        }
    }
    g_printerr("Icona meteo non trovata: %s\n", filename);
    return {};
}

std::string toLowerAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(g_ascii_tolower(static_cast<guchar>(c)));
    }
    return value;
}

void onRefreshClicked(GtkButton*, gpointer user_data) {
    auto* widget = static_cast<WeatherWidget*>(user_data);
    if (widget) widget->refresh();
}

void onCityClicked(GtkButton*, gpointer user_data) {
    auto* widget = static_cast<WeatherWidget*>(user_data);
    if (!widget || !widget->container) return;

    GtkWidget* top = gtk_widget_get_toplevel(widget->container);
    GtkWindow* parent = GTK_IS_WINDOW(top) ? GTK_WINDOW(top) : nullptr;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        _("Change Location"),
        parent,
        GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_OK", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), widget->current_city.c_str());
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char* city = gtk_entry_get_text(GTK_ENTRY(entry));
        widget->set_city(city);
    }
    gtk_widget_destroy(dialog);
}

} // namespace

WeatherWidget* WeatherWidget::new_widget() {
    auto* widget = new WeatherWidget();
    widget->alive = std::make_shared<std::atomic<bool>>(true);
    widget->current_city = loadCity();
    widget->status = WeatherWidget::Status::Waiting;

    // ── Card esterna (stilizzata come #WeatherSidebar nel CSS) ──────────
    widget->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_name(widget->container, "WeatherSidebar");
    gtk_widget_set_margin_start(widget->container, 4);
    gtk_widget_set_margin_end(widget->container, 4);
    gtk_widget_set_margin_top(widget->container, 4);
    gtk_widget_set_margin_bottom(widget->container, 4);

    // ── 1) Header: "Meteo · <città>" a sinistra, icone a destra ──────────
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(header, "weather-header");

    widget->lbl_title = gtk_label_new(nullptr);
    widget->update_header_title();
    gtk_widget_set_halign(widget->lbl_title, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(widget->lbl_title), PANGO_ELLIPSIZE_END);

    // Cambio località: icona mirino/geolocalizzazione
    widget->btn_city = gtk_button_new();
    gtk_widget_set_name(widget->btn_city, "weather-city-btn");
    gtk_widget_set_valign(widget->btn_city, GTK_ALIGN_CENTER);

    GtkWidget* btn_city_icon = gtk_image_new_from_icon_name(
        "find-location-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(widget->btn_city), btn_city_icon);

    gtk_widget_set_tooltip_text(widget->btn_city, _("Change location"));

    // Aggiorna: icona refresh
    widget->btn_refresh = gtk_button_new();
    gtk_widget_set_name(widget->btn_refresh, "weather-refresh-btn");
    gtk_widget_set_valign(widget->btn_refresh, GTK_ALIGN_CENTER);

    GtkWidget* btn_refresh_icon = gtk_image_new_from_icon_name(
        "view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(widget->btn_refresh), btn_refresh_icon);

    gtk_widget_set_tooltip_text(widget->btn_refresh, _("Refresh"));

    gtk_box_pack_start(GTK_BOX(header), widget->lbl_title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), widget->btn_refresh, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), widget->btn_city, FALSE, FALSE, 0);

    // ── 2) Blocco centrale: icona grande a sx, dati a dx ────────────────
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_halign(content, GTK_ALIGN_CENTER);

    const std::string initialPath = findIconPath("partly_cloudy.png");
    widget->lbl_emoji = initialPath.empty()
        ? gtk_image_new()
        : gtk_image_new_from_file(initialPath.c_str());
    gtk_widget_set_name(widget->lbl_emoji, "weather-emoji");
    gtk_widget_set_size_request(widget->lbl_emoji, 72, 72);
    gtk_widget_set_valign(widget->lbl_emoji, GTK_ALIGN_CENTER);

    GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    widget->lbl_temp = gtk_label_new(_("-- °C"));
    gtk_widget_set_name(widget->lbl_temp, "weather-temp-label");
    gtk_widget_set_halign(widget->lbl_temp, GTK_ALIGN_START);

    widget->lbl_condition = gtk_label_new(_("Waiting for weather data..."));
    gtk_widget_set_name(widget->lbl_condition, "condition-label");
    gtk_widget_set_halign(widget->lbl_condition, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(widget->lbl_condition), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(widget->lbl_condition), 18);
    gtk_label_set_xalign(GTK_LABEL(widget->lbl_condition), 0.0f);

    widget->lbl_hi_lo = gtk_label_new(nullptr);
    gtk_widget_set_name(widget->lbl_hi_lo, "weather-hilo-label");
    gtk_widget_set_halign(widget->lbl_hi_lo, GTK_ALIGN_START);
    gtk_label_set_use_markup(GTK_LABEL(widget->lbl_hi_lo), TRUE);
    gtk_widget_set_no_show_all(widget->lbl_hi_lo, TRUE);

    gtk_box_pack_start(GTK_BOX(info), widget->lbl_temp, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(info), widget->lbl_condition, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(info), widget->lbl_hi_lo, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content), widget->lbl_emoji, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), info, TRUE, TRUE, 0);

    // ── 3) Packing finale (rimossa riga città duplicata) ──────────────────
    gtk_box_pack_start(GTK_BOX(widget->container), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(widget->container), content, TRUE, TRUE, 4);

    // ── Callback ──────────────────────────────────────────────────────────
    g_signal_connect(widget->btn_refresh, "clicked",
                     G_CALLBACK(onRefreshClicked), widget);
    g_signal_connect(widget->btn_city, "clicked",
                     G_CALLBACK(onCityClicked), widget);

    // ── Packing finale ──────────────────────────────────────────────────
    gtk_box_pack_start(GTK_BOX(widget->container), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(widget->container), content, TRUE, TRUE, 4);

    gtk_widget_show_all(widget->container);
    gtk_widget_set_visible(widget->lbl_hi_lo, FALSE);

    widget->refresh();
    return widget;
}

// ── Titolo header: "Meteo" + nome città ────────────────────────────────────
// L'helper riescrive lbl_title con la città corrente; così basta chiamarlo
// da new_widget(), set_city() e refresh() per tenerlo sempre aggiornato.
void WeatherWidget::update_header_title() {
    if (!lbl_title)
        return;

    // Escape del nome città: evita che caratteri come "&" o "<"
    // rompano il markup Pango
    gchar* escaped_city = g_markup_escape_text(current_city.c_str(), -1);
    gchar* markup = g_markup_printf_escaped("<b>%s</b>  ·  %s",
                                            _("Meteo"), escaped_city);
    gtk_label_set_markup(GTK_LABEL(lbl_title), markup);
    g_free(markup);
    g_free(escaped_city);
}

// ── Temperatura grande + riga max/min ───────────────────────────────────────
void WeatherWidget::render_temperature_labels() {
    if (lbl_temp) {
        if (status == Status::Weather) {
            gtk_label_set_text(GTK_LABEL(lbl_temp),
                               g_strdup_printf("%d°C", static_cast<int>(last_temp)));
        } else {
            gtk_label_set_text(GTK_LABEL(lbl_temp), _("-- °C"));
        }
    }
    if (lbl_hi_lo) {
        if (status == Status::Weather && has_hi_lo) {
            gtk_label_set_markup(
                GTK_LABEL(lbl_hi_lo),
                g_strdup_printf("↑ %.0f°C   ↓ %.0f°C", temp_hi, temp_lo));
            gtk_widget_set_visible(lbl_hi_lo, TRUE);
        } else {
            gtk_widget_set_visible(lbl_hi_lo, FALSE);
        }
    }
}

// ── Stato dell'etichetta condizione ─────────────────────────────────────────
void WeatherWidget::render_status_label() {
    if (!lbl_condition)
        return;

    switch (status) {
        case Status::Waiting:
            gtk_label_set_text(GTK_LABEL(lbl_condition),
                               _("Waiting for weather data..."));
            break;
        case Status::Loading:
            gtk_label_set_text(GTK_LABEL(lbl_condition),
                               _("Loading..."));
            break;
        case Status::EnterLocation:
            gtk_label_set_text(GTK_LABEL(lbl_condition),
                               _("Enter a location"));
            break;
        case Status::NotAvailable:
            gtk_label_set_text(GTK_LABEL(lbl_condition),
                               _("Weather data not available"));
            break;
        case Status::Weather:
        case Status::Error:
            gtk_label_set_text(GTK_LABEL(lbl_condition),
                               last_detail.empty() ? last_condition.c_str()
                                                   : last_detail.c_str());
            break;
    }
}

// ── Lingua ───────────────────────────────────────────────────────────────────
void WeatherWidget::set_language(const std::string& lang)
{
    language = lang;

    update_header_title();   // al posto del vecchio g_strdup_printf("<b>%s</b>", _("Meteo"))

    if (btn_refresh)
        gtk_widget_set_tooltip_text(btn_refresh, _("Refresh"));

    if (btn_city)
        gtk_widget_set_tooltip_text(btn_city, _("Change location"));

    render_status_label();
    render_temperature_labels();
}

// ── Icona in base alla condizione ────────────────────────────────────────────
void WeatherWidget::set_weather_icon(const std::string& condition) {
    if (!lbl_emoji) return;
    const std::string c = toLowerAscii(condition);
    std::string filename;

    if (c.find("poco nuvol") != std::string::npos || c.find("partial") != std::string::npos || c.find("scatter") != std::string::npos) {
        filename = "partly_cloudy.png";
    } else if (c.find("seren") != std::string::npos || c.find("clear") != std::string::npos || c.find("sunny") != std::string::npos) {
        filename = "clear.png";
    } else if (c.find("nuvol") != std::string::npos || c.find("coperto") != std::string::npos || c.find("cloud") != std::string::npos || c.find("overcast") != std::string::npos) {
        filename = "cloudy.png";
    } else if (c.find("piov") != std::string::npos || c.find("rain") != std::string::npos || c.find("rovescio") != std::string::npos) {
        filename = "rain.png";
    } else if (c.find("tempor") != std::string::npos || c.find("thunder") != std::string::npos || c.find("storm") != std::string::npos) {
        filename = "thunderstorm.png";
    } else if (c.find("neve") != std::string::npos || c.find("snow") != std::string::npos) {
        filename = "snow.png";
    } else {
        filename = "partly_cloudy.png";
    }

    const std::string path = findIconPath(filename.c_str());
    if (!path.empty()) {
        gtk_image_set_from_file(GTK_IMAGE(lbl_emoji), path.c_str());
    }
}

// ── Cambio località ──────────────────────────────────────────────────────────
void WeatherWidget::set_city(const char* city) {
    if (!city || !*city) return;

    current_city = city;
    saveCity(current_city);
    update_header_title();
    refresh();
}

// ── Aggiornamento dati ───────────────────────────────────────────────────────
void WeatherWidget::refresh() {
    update_header_title();

    if (current_city.empty()) {
        status = Status::EnterLocation;
        last_condition.clear();
        last_detail.clear();
        render_status_label();
        render_temperature_labels();
        return;
    }

    status = Status::Loading;
    last_condition.clear();
    last_detail.clear();
    render_status_label();
    render_temperature_labels();

    const auto aliveToken = alive;

    client.fetch(current_city,
        [this, aliveToken](const WeatherData& data, const std::string& error) {
            if (!aliveToken ||
                !aliveToken->load(std::memory_order_acquire)) {
                return;
            }

            if (!lbl_condition || !lbl_temp) return;

            if (!error.empty()) {
                status = Status::Error;
                last_condition = error;
                last_detail.clear();

                render_status_label();
                render_temperature_labels();
                return;
            }

            if (!data.valid) {
                status = Status::NotAvailable;
                last_condition.clear();
                last_detail.clear();

                render_status_label();
                render_temperature_labels();
                return;
            }

            status = Status::Weather;
            last_condition = data.condition;
            last_detail.clear();
            last_temp = static_cast<double>(data.temp_c);

            render_status_label();
            render_temperature_labels();

            if (lbl_emoji) {
                const std::string path = findIconPath(data.icon.c_str());
                if (!path.empty()) {
                    gtk_image_set_from_file(GTK_IMAGE(lbl_emoji), path.c_str());
                } else {
                    set_weather_icon(data.condition);
                }
            }
        });
}

// ── Rilascio risorse ─────────────────────────────────────────────────────────
void WeatherWidget::free_resources() {
    if (alive) {
        alive->store(false, std::memory_order_release);
    }

    client.cancel();

    if (container) {
        gtk_widget_destroy(container);
        container = nullptr;
    }

    lbl_title = nullptr;
    btn_refresh = nullptr;
    btn_city = nullptr;
    lbl_temp = nullptr;
    lbl_hi_lo = nullptr;
    lbl_condition = nullptr;
    lbl_emoji = nullptr;
}

