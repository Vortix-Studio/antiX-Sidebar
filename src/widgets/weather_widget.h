#ifndef WEATHER_WIDGET_H
#define WEATHER_WIDGET_H

#include <gtk/gtk.h>
#include <atomic>
#include <memory>
#include <string>

#include "open_meteo_client.h"

class WeatherWidget {
public:
    static WeatherWidget* new_widget();

    GtkWidget* get_container() const
    {
        return container;
    }

    void set_language(const std::string& lang);
    void set_city(const char* city);
    void refresh();
    void render_status_label();
    void free_resources();

    // Stato corrente del pannello: serve a set_language() per
    // riscrivere lbl_condition con il testo giusto nella nuova lingua.
    enum class Status {
        Waiting,
        Loading,
        EnterLocation,
        NotAvailable,
        Weather,
        Error
    };

    // Accessibili anche alle callback libere in weather_widget.cpp.
    GtkWidget* container = nullptr;
    std::string language = "it";
    std::string current_city;
    Status status = Status::Waiting;
    std::string last_condition;   // condizione breve (es. "Sereno")
    std::string last_detail;      // riga completa per lbl_condition

private:
    WeatherWidget() = default;

    void set_weather_icon(const std::string& condition);
    void render_temperature_labels();
    void update_header_title();

    // Layout come da mockup:
    //  [Meteo                        📍 ↻]
    //  [icona grande | 18°C / condizione / ↑↓]
    //  ─────────────────────────────────────────
    //  [📍 Roma, IT]  ← solo indicativa
    GtkWidget* lbl_title     = nullptr;   // titolo "Meteo" nell'header
    GtkWidget* lbl_temp      = nullptr;
    GtkWidget* lbl_hi_lo     = nullptr;   // riga ↑ max ↓ min
    GtkWidget* lbl_condition = nullptr;
    GtkWidget* lbl_emoji     = nullptr;
    //GtkWidget* lbl_location  = nullptr;   // testo della riga città in basso
    //GtkWidget* city_row      = nullptr;   // box indicativo (pin + città)
    GtkWidget* btn_refresh   = nullptr;   // pulsante ↻
    GtkWidget* btn_city      = nullptr;   // pulsante 📍

    // Dati derivati dalla risposta meteo
    bool   has_hi_lo = false;
    double temp_hi   = 0.0;
    double temp_lo   = 0.0;
    double last_temp = 0.0;

    OpenMeteoClient client;
    std::shared_ptr<std::atomic<bool>> alive;
};

#endif // WEATHER_WIDGET_H
