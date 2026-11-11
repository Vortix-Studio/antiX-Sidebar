#include "open_meteo_client.h"

#include <curl/curl.h>
#include <glib.h>
#include <atomic>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

const char* USER_AGENT = "antiX-Sidebar/1.0 (Linux; x86_64)";
const char* BASE_URL = "https://wttr.in/";
std::once_flag curlInit;

// Formato dell'utente + riga tecnica "@@|..." (rimossa prima della visualizzazione).
// Le sequenze "\n" sono quelle letterali che wttr.in traduce in newline.
const std::string FORMAT_STRING =
    "%C|%t|%f|%h|%w|%P|%p|%u|%c|%l";

struct TransferCtx {
    std::string* body;
    std::atomic<bool>* cancel;
};

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* user) {
    auto* ctx = static_cast<TransferCtx*>(user);
    const size_t bytes = size * nmemb;

    if (ctx->cancel && ctx->cancel->load(std::memory_order_relaxed)) {
        return 0;
    }

    ctx->body->append(ptr, bytes);
    return bytes;
}

bool httpGet(const std::string& url,
             std::string& body,
             std::atomic<bool>& cancel,
             std::string& err) {
    std::call_once(curlInit, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });

    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "Inizializzazione CURL fallita";
        return false;
    }

    TransferCtx ctx{&body, &cancel};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (cancel.load(std::memory_order_relaxed)) {
        err = "Richiesta annullata";
        return false;
    }

    if (res != CURLE_OK) {
        err = curl_easy_strerror(res);
        return false;
    }

    if (http_code != 200) {
        err = "HTTP error " + std::to_string(http_code);
        return false;
    }

    return true;
}

std::string urlEncode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* output = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result = output ? output : value;
    if (output) curl_free(output);
    curl_easy_cleanup(curl);
    return result;
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(sep, start);
        if (pos == std::string::npos) {
            out.push_back(trim(s.substr(start)));
            break;
        }
        out.push_back(trim(s.substr(start, pos - start)));
        start = pos + 1;
    }
    return out;
}

// Primo numero dentro stringhe tipo "+13°C" / "↖18km/h" / "72%"
double firstNumber(const std::string& s, bool* ok) {
    *ok = false;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isdigit(c) ||
            (c == '-' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            char* end = nullptr;
            double v = std::strtod(s.c_str() + i, &end);
            if (end != s.c_str() + i) {
                *ok = true;
                return v;
            }
        }
    }
    return 0.0;
}



} // namespace

OpenMeteoClient::OpenMeteoClient()
    : m_state(std::make_shared<SharedState>()) {}

OpenMeteoClient::~OpenMeteoClient() {
    cancel();
}

void OpenMeteoClient::cancel() {
    if (m_state) {
        m_state->cancelled.store(true, std::memory_order_relaxed);
    }
}

bool OpenMeteoClient::busy() const {
    return m_state && m_state->busy.load(std::memory_order_relaxed);
}

std::string OpenMeteoClient::condition_to_icon(const std::string& condition,
                                               const std::string& emoji) {
    // lower-case per riconoscere anche le parole inglesi di wttr.in
    std::string lc;
    lc.reserve(condition.size());
    for (char c : condition) {
        lc += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    auto has = [&lc](std::initializer_list<const char*> keys) {
        for (const char* k : keys) {
            if (lc.find(k) != std::string::npos) return true;
        }
        return false;
    };

    if (has({"temporale", "thunder", "⛈"}))          return "thunderstorm.png";
    if (has({"neve", "nevicata", "granelli", "snow", "❄", "🌨"})) return "snow.png";
    if (has({"pioggia", "pioggerella", "pioviggin", "rovesci",
             "rain", "drizzle", "shower", "🌧", "☔"})) return "rain.png";
    if (has({"nebbia", "fog", "coperto", "overcast",
             "nuvol", "cloud", "🌫"}))                return "cloudy.png";
    if (has({"sereno", "sole", "clear", "sunny",
             "☀", "☼", "🌞"}))                        return "clear.png";

    // fallback basato sull'emoji %c
    if (emoji.find("☀") != std::string::npos || emoji.find("☼") != std::string::npos)
        return "clear.png";
    if (emoji.find("⛈") != std::string::npos)
        return "thunderstorm.png";
    if (emoji.find("🌧") != std::string::npos || emoji.find("☔") != std::string::npos ||
        emoji.find("🌦") != std::string::npos)
        return "rain.png";
    if (emoji.find("❄") != std::string::npos || emoji.find("🌨") != std::string::npos)
        return "snow.png";
    if (emoji.find("☁") != std::string::npos || emoji.find("🌫") != std::string::npos)
        return "cloudy.png";

    return "partly_cloudy.png";
}

void OpenMeteoClient::fetch(const std::string& query, Callback cb) {
    if (!cb) return;

    if (m_state->busy.exchange(true, std::memory_order_acq_rel)) {
        cb(WeatherData{}, "Richiesta già in corso");
        return;
    }

    m_state->cancelled.store(false, std::memory_order_relaxed);

    auto state = m_state;
    std::string q = query;

    std::thread worker([state, q, cb]() {
        auto notify = [state, cb](WeatherData data, std::string error) {
            struct Payload {
                std::shared_ptr<SharedState> state;
                Callback callback;
                WeatherData data;
                std::string error;
            };

            auto* payload = new Payload{state, cb, std::move(data), std::move(error)};

            g_idle_add(
                +[](gpointer p) -> gboolean {
                    auto* pl = static_cast<Payload*>(p);
                    if (!pl->state->cancelled.load(std::memory_order_relaxed)) {
                        pl->callback(pl->data, pl->error);
                    }
                    pl->state->busy.store(false, std::memory_order_release);
                    delete pl;
                    return G_SOURCE_REMOVE;
                },
                payload
            );
        };

        if (state->cancelled.load(std::memory_order_relaxed)) {
            state->busy.store(false, std::memory_order_release);
            return;
        }

        // wttr.in risolve la località internamente: niente geocoding separato
        std::string url =
            std::string(BASE_URL) + urlEncode(q) +
            "?lang=it&format=" + FORMAT_STRING;

        std::string body;
        std::string err;
        if (!httpGet(url, body, state->cancelled, err)) {
            notify(WeatherData{}, "Errore richiesta wttr.in: " + err);
            return;
        }

        g_printerr("wttr.in risposta (%zu byte): %s\n",
                   body.size(), body.c_str());   // ← debug: rimuovi dopo

        if (body.find("Unknown location") != std::string::npos ||
            body.find("Sorry") != std::string::npos ||
            body.find("<html") != std::string::npos ||
            body.find("<!DOCTYPE") != std::string::npos) {
            notify(WeatherData{}, "Località '" + q + "' non trovata");
            return;
        }

        // Con formato custom ridotto, il body è già solo la riga tecnica
        std::string parseLine = body;
        while (!parseLine.empty() &&
               (parseLine.back() == '\n' || parseLine.back() == '\r')) {
            parseLine.pop_back();
        }

        // ordine dei campi: 0 %C | 1 %t | 2 %f | 3 %h | 4 %w | 5 %P |
        //                   6 %p | 7 %u | 8 %c | 9 %l
        std::vector<std::string> f = split(parseLine, '|');
        if (f.size() < 10) {
            notify(WeatherData{}, "Risposta inattesa da wttr.in");
            return;
        }

        bool ok = false;
        WeatherData d;
        d.valid         = true;
        d.location      = f[9];
        d.condition     = f[0];
        d.emoji         = f[8];
        d.temp_c        = static_cast<int>(std::round(firstNumber(f[1], &ok)));
        d.feels_like_c  = static_cast<int>(std::round(firstNumber(f[2], &ok)));
        d.humidity      = static_cast<int>(std::round(firstNumber(f[3], &ok)));
        d.wind_kmh      = static_cast<int>(std::round(firstNumber(f[4], &ok)));
        d.pressure_hpa  = static_cast<int>(std::round(firstNumber(f[5], &ok)));
        d.precip_mm     = static_cast<int>(std::round(firstNumber(f[6], &ok)));
        d.uv_index      = f[7];
        d.icon          = condition_to_icon(d.condition, d.emoji);

        notify(std::move(d), "");
    });

    worker.detach();
}
