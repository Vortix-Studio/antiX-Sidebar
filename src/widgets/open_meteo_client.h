#ifndef OPEN_METEO_CLIENT_H
#define OPEN_METEO_CLIENT_H

#include <string>
#include <functional>
#include <memory>
#include <atomic>

struct WeatherData {
    bool valid = false;

    std::string raw_text;     // riga formattata di wttr.in, pronta per la UI
    std::string location;     // %l
    std::string emoji;        // %c
    std::string condition;    // %C

    int temp_c = 0;           // %t
    int feels_like_c = 0;     // %f
    int humidity = 0;         // %h (%)
    int pressure_hpa = 0;     // %P
    int precip_mm = 0;        // %p
    int wind_kmh = 0;         // %w
    std::string uv_index;     // %u
    std::string sunrise;      // %S (HH:MM)
    std::string sunset;       // %s (HH:MM)
    std::string moon_phase;   // %m
    int moon_day = 0;         // %M
    std::string icon = "partly_cloudy.png";
};

class OpenMeteoClient {
public:
    using Callback = std::function<void(const WeatherData& data, const std::string& error)>;

    OpenMeteoClient();
    ~OpenMeteoClient();

    void fetch(const std::string& query, Callback cb);
    void cancel();
    bool busy() const;

    // mappa condizione (testo %C o emoji %c) -> icona della sidebar
    static std::string condition_to_icon(const std::string& condition,
                                         const std::string& emoji = "");

private:
    struct SharedState {
        std::atomic<bool> busy{false};
        std::atomic<bool> cancelled{false};
    };
    std::shared_ptr<SharedState> m_state;
};

#endif // OPEN_METEO_CLIENT_H
