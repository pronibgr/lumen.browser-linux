#include "omnibox/converter.hpp"
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <regex>
#include <iostream>
#include <curl/curl.h>

#include <glib.h>

namespace Blueprint::Omnibox {

namespace {

// Rates relative to Base Units
// Length base: Meter (m)
const std::unordered_map<std::string, double> LENGTH_TO_M = {
    {"m", 1.0}, {"meter", 1.0}, {"meters", 1.0}, {"метр", 1.0}, {"метра", 1.0}, {"метров", 1.0},
    {"km", 1000.0}, {"kilometer", 1000.0}, {"kilometers", 1000.0}, {"км", 1000.0},
    {"cm", 0.01}, {"centimeter", 0.01}, {"см", 0.01},
    {"mm", 0.001}, {"millimeter", 0.001}, {"мм", 0.001},
    {"mi", 1609.344}, {"mile", 1609.344}, {"miles", 1609.344}, {"миля", 1609.344}, {"миль", 1609.344},
    {"ft", 0.3048}, {"foot", 0.3048}, {"feet", 0.3048}, {"фут", 0.3048}, {"футов", 0.3048},
    {"in", 0.0254}, {"inch", 0.0254}, {"inches", 0.0254}, {"дюйм", 0.0254}, {"дюймов", 0.0254},
    {"yd", 0.9144}, {"yard", 0.9144}, {"yards", 0.9144}, {"ярд", 0.9144}
};

// Weight base: Kilogram (kg)
const std::unordered_map<std::string, double> WEIGHT_TO_KG = {
    {"kg", 1.0}, {"kilogram", 1.0}, {"kilograms", 1.0}, {"кг", 1.0},
    {"g", 0.001}, {"gram", 0.001}, {"grams", 0.001}, {"г", 0.001},
    {"mg", 0.000001}, {"milligram", 0.000001}, {"мг", 0.000001},
    {"lb", 0.45359237}, {"lbs", 0.45359237}, {"pound", 0.45359237}, {"pounds", 0.45359237}, {"фунт", 0.45359237},
    {"oz", 0.028349523}, {"ounce", 0.028349523}, {"унция", 0.028349523},
    {"ton", 1000.0}, {"tons", 1000.0}, {"тонна", 1000.0}
};

// Data base: Megabyte (mb)
const std::unordered_map<std::string, double> DATA_TO_MB = {
    {"b", 1.0 / (1024.0 * 1024.0)}, {"byte", 1.0 / (1024.0 * 1024.0)}, {"bytes", 1.0 / (1024.0 * 1024.0)},
    {"kb", 1.0 / 1024.0}, {"kilobyte", 1.0 / 1024.0}, {"кб", 1.0 / 1024.0},
    {"mb", 1.0}, {"megabyte", 1.0}, {"мб", 1.0},
    {"gb", 1024.0}, {"gigabyte", 1024.0}, {"гб", 1024.0},
    {"tb", 1024.0 * 1024.0}, {"terabyte", 1024.0 * 1024.0}, {"тб", 1024.0 * 1024.0}
};

// currency aliases with rich russian and international declensions
const std::unordered_map<std::string, std::string> CURRENCY_ALIASES = {
    {"usd", "USD"}, {"$", "USD"}, {"доллар", "USD"}, {"доллара", "USD"}, {"долларов", "USD"},
    {"доллары", "USD"}, {"долларах", "USD"}, {"bucks", "USD"}, {"бакс", "USD"}, {"бакса", "USD"},
    {"баксов", "USD"}, {"баксах", "USD"},
    {"eur", "EUR"}, {"€", "EUR"}, {"евро", "EUR"}, {"euro", "EUR"}, {"euros", "EUR"},
    {"rub", "RUB"}, {"₽", "RUB"}, {"рубль", "RUB"}, {"рубля", "RUB"}, {"руб", "RUB"},
    {"рублей", "RUB"}, {"рубли", "RUB"}, {"рублях", "RUB"},
    {"gbp", "GBP"}, {"£", "GBP"}, {"фунт", "GBP"}, {"фунта", "GBP"}, {"фунтов", "GBP"},
    {"фунты", "GBP"}, {"фунт_ст", "GBP"},
    {"cny", "CNY"}, {"¥", "CNY"}, {"юань", "CNY"}, {"юаня", "CNY"}, {"юаней", "CNY"}, {"юани", "CNY"},
    {"jpy", "JPY"}, {"иена", "JPY"}, {"иены", "JPY"}, {"йена", "JPY"}, {"йены", "JPY"},
    {"иен", "JPY"}, {"йен", "JPY"},
    {"chf", "CHF"}, {"франк", "CHF"}, {"франка", "CHF"}, {"франков", "CHF"}, {"франки", "CHF"},
    {"cad", "CAD"}, {"aud", "AUD"},
    {"kzt", "KZT"}, {"тенге", "KZT"}, {"тг", "KZT"},
    {"uah", "UAH"}, {"гривна", "UAH"}, {"гривны", "UAH"}, {"гривен", "UAH"}, {"грн", "UAH"},
    {"byn", "BYN"}, {"белрубль", "BYN"}, {"белруб", "BYN"},
    {"try", "TRY"}, {"лира", "TRY"}, {"лиры", "TRY"}, {"лир", "TRY"},
    {"pln", "PLN"}, {"злотый", "PLN"}, {"злотых", "PLN"}, {"злотые", "PLN"},
    {"inr", "INR"}, {"рупия", "INR"}, {"рупии", "INR"}, {"рупий", "INR"},
    {"krw", "KRW"}, {"вона", "KRW"}, {"воны", "KRW"}, {"вон", "KRW"},
    {"brl", "BRL"}, {"sek", "SEK"}, {"nok", "NOK"}, {"dkk", "DKK"},
    {"sgd", "SGD"}, {"hkd", "HKD"},
    {"thb", "THB"}, {"бат", "THB"}, {"баты", "THB"}, {"батов", "THB"},
    {"btc", "BTC"}, {"биткоин", "BTC"}, {"биткоина", "BTC"}, {"биткоинов", "BTC"},
    {"биток", "BTC"}, {"биткойн", "BTC"}, {"bitcoin", "BTC"},
    {"eth", "ETH"}, {"эфир", "ETH"}, {"эфира", "ETH"}, {"эфириум", "ETH"}, {"ethereum", "ETH"}
};

// updated offline baseline rates to usd just in case network is down
const std::unordered_map<std::string, double> OFFLINE_RATES_TO_USD = {
    {"USD", 1.0},
    {"EUR", 1.16},
    {"GBP", 1.35},
    {"RUB", 0.0116},    // ~86.4 RUB/USD
    {"CNY", 0.149},     // ~6.7 CNY/USD
    {"JPY", 0.0065},    // ~153 JPY/USD
    {"CHF", 1.24},
    {"CAD", 0.73},
    {"AUD", 0.72},
    {"KZT", 0.0019},    // ~523 KZT/USD
    {"UAH", 0.024},     // ~41 UAH/USD
    {"BYN", 0.31},      // ~3.2 BYN/USD
    {"TRY", 0.021},     // ~48 TRY/USD
    {"PLN", 0.27},
    {"INR", 0.0105},
    {"KRW", 0.00075},
    {"BRL", 0.196},
    {"SEK", 0.104},
    {"NOK", 0.109},
    {"DKK", 0.156},
    {"SGD", 0.79},
    {"HKD", 0.128},
    {"THB", 0.030},
    {"BTC", 78500.0},
    {"ETH", 2500.0}
};

// proper lowercase with full utf-8 and cyrillic alphabet handling
std::string toLower(const std::string& s) {
    if (s.empty()) return "";
    gchar* down = g_utf8_strdown(s.c_str(), -1);
    if (!down) return s;
    std::string res(down);
    g_free(down);
    return res;
}

// thread-safe live rates cache and async fetch state
struct LiveRatesCache {
    std::mutex mtx;
    std::unordered_map<std::string, double> rates; // currency -> rate to USD
    std::chrono::steady_clock::time_point lastFetch{};
    std::atomic<bool> fetching{false};
    std::function<void()> onUpdated;
};

LiveRatesCache g_ratesCache;

size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total);
    return total;
}

bool fetchUrl(const std::string& url, std::string& buffer, long timeoutMs = 5000L) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    buffer.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lumen-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK && httpCode == 200 && !buffer.empty());
}

double fetchCryptoPrice(const std::string& symbol) {
    std::string buf;
    if (fetchUrl("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol, buf, 3000L)) {
        std::regex r(R"raw("price"\s*:\s*"([0-9.]+)")raw");
        std::smatch m;
        if (std::regex_search(buf, m, r)) {
            try {
                return std::stod(m[1].str());
            } catch (...) {}
        }
    }
    return 0.0;
}

void triggerAsyncFetch() {
    bool expected = false;
    if (!g_ratesCache.fetching.compare_exchange_strong(expected, true)) {
        return; // already fetching, avoid hammering
    }

    std::thread([]() {
        std::unordered_map<std::string, double> newRates;
        newRates["USD"] = 1.0;

        // open.er-api provides fast global fiat rates including rub, kzt, eur, cny, gbp
        std::string buffer;
        if (fetchUrl("https://open.er-api.com/v6/latest/USD", buffer, 5000L)) {
            std::regex ratesBlockRegex(R"raw("rates"\s*:\s*\{([^}]+)\})raw");
            std::smatch match;
            if (std::regex_search(buffer, match, ratesBlockRegex)) {
                std::string ratesBody = match[1].str();
                std::regex pairRegex(R"raw("([A-Za-z0-9]{3,5})"\s*:\s*"?([0-9.]+)"?)raw");
                std::sregex_iterator next(ratesBody.begin(), ratesBody.end(), pairRegex);
                std::sregex_iterator end;

                while (next != end) {
                    std::string code = (*next)[1].str();
                    try {
                        double rateFromUsd = std::stod((*next)[2].str());
                        if (rateFromUsd > 0.0) {
                            newRates[code] = 1.0 / rateFromUsd;
                        }
                    } catch (...) {}
                    ++next;
                }
            }
        }

        // secondary fallback to frankfurter if open.er-api was unreachable
        if (newRates.size() < 10) {
            buffer.clear();
            if (fetchUrl("https://api.frankfurter.dev/v1/latest?base=USD", buffer, 5000L)) {
                std::regex ratesBlockRegex(R"raw("rates"\s*:\s*\{([^}]+)\})raw");
                std::smatch match;
                if (std::regex_search(buffer, match, ratesBlockRegex)) {
                    std::string ratesBody = match[1].str();
                    std::regex pairRegex(R"raw("([A-Za-z]{3})"\s*:\s*([0-9.]+))raw");
                    std::sregex_iterator next(ratesBody.begin(), ratesBody.end(), pairRegex);
                    std::sregex_iterator end;

                    while (next != end) {
                        std::string code = (*next)[1].str();
                        try {
                            double rateFromUsd = std::stod((*next)[2].str());
                            if (rateFromUsd > 0.0) {
                                newRates[code] = 1.0 / rateFromUsd;
                            }
                        } catch (...) {}
                        ++next;
                    }
                }
            }
        }

        // commit fiat rates to live cache right away so omnibox is instantly snappy
        if (newRates.size() > 5) {
            std::function<void()> notifyCb;
            {
                std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
                for (const auto& [code, val] : OFFLINE_RATES_TO_USD) {
                    if (newRates.find(code) == newRates.end()) {
                        if (g_ratesCache.rates.find(code) != g_ratesCache.rates.end()) {
                            newRates[code] = g_ratesCache.rates[code];
                        } else {
                            newRates[code] = val;
                        }
                    }
                }
                g_ratesCache.rates = newRates;
                g_ratesCache.lastFetch = std::chrono::steady_clock::now();
                notifyCb = g_ratesCache.onUpdated;
            }
            if (notifyCb) notifyCb();
        }

        // now grab real-time crypto prices without holding up fiat currencies
        double btc = fetchCryptoPrice("BTCUSDT");
        double eth = fetchCryptoPrice("ETHUSDT");
        if (btc > 0.0 || eth > 0.0) {
            std::function<void()> notifyCb;
            {
                std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
                if (btc > 0.0) g_ratesCache.rates["BTC"] = btc;
                if (eth > 0.0) g_ratesCache.rates["ETH"] = eth;
                notifyCb = g_ratesCache.onUpdated;
            }
            if (notifyCb) notifyCb();
        }

        g_ratesCache.fetching.store(false);
    }).detach();
}

} // anonymous namespace

void UnitConverter::init() {
    triggerAsyncFetch();
}

void UnitConverter::refreshRates() {
    triggerAsyncFetch();
}

void UnitConverter::setOnRatesUpdated(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
    g_ratesCache.onUpdated = std::move(cb);
}

std::optional<ConversionResult> UnitConverter::convert(const std::string& query) {
    if (query.empty()) return std::nullopt;

    std::string lower = toLower(query);

    // trim leading spaces
    size_t firstNonSpace = lower.find_first_not_of(" \t");
    if (firstNonSpace != std::string::npos) {
        lower = lower.substr(firstNonSpace);
    }

    // support prefix currency symbols like $100, €50, ₽500, £20, ¥1000
    std::string prefixSymbol;
    const std::vector<std::string> symbols = {"$", "€", "₽", "£", "¥"};
    for (const auto& sym : symbols) {
        if (lower.rfind(sym, 0) == 0) {
            prefixSymbol = sym;
            lower = lower.substr(sym.length());
            break;
        }
    }

    std::istringstream iss(lower);
    double val = 0.0;
    std::string fromUnit, separator, toUnit;

    if (!prefixSymbol.empty()) {
        fromUnit = prefixSymbol;
        if (!(iss >> val)) return std::nullopt;
    } else {
        if (!(iss >> val >> fromUnit)) return std::nullopt;
    }

    if (iss >> separator) {
        if (separator == "to" || separator == "in" || separator == "into" ||
            separator == "в" || separator == "к" || separator == "->" ||
            separator == "=" || separator == ":") {
            if (!(iss >> toUnit)) return std::nullopt;
        } else {
            toUnit = separator;
        }
    } else {
        return std::nullopt;
    }

    // Temperature Conversion
    if ((fromUnit == "c" || fromUnit == "celsius") && (toUnit == "f" || toUnit == "fahrenheit")) {
        double result = (val * 9.0 / 5.0) + 32.0;
        std::ostringstream ss;
        ss << val << " °C = " << std::fixed << std::setprecision(2) << result << " °F";
        return ConversionResult{val, "°C", result, "°F", ss.str(), false, true};
    }
    if ((fromUnit == "f" || fromUnit == "fahrenheit") && (toUnit == "c" || toUnit == "celsius")) {
        double result = (val - 32.0) * 5.0 / 9.0;
        std::ostringstream ss;
        ss << val << " °F = " << std::fixed << std::setprecision(2) << result << " °C";
        return ConversionResult{val, "°F", result, "°C", ss.str(), false, true};
    }

    // Length Check
    if (LENGTH_TO_M.count(fromUnit) && LENGTH_TO_M.count(toUnit)) {
        double inMeters = val * LENGTH_TO_M.at(fromUnit);
        double result = inMeters / LENGTH_TO_M.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str(), false, true};
    }

    // Weight Check
    if (WEIGHT_TO_KG.count(fromUnit) && WEIGHT_TO_KG.count(toUnit)) {
        double inKg = val * WEIGHT_TO_KG.at(fromUnit);
        double result = inKg / WEIGHT_TO_KG.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str(), false, true};
    }

    // Data Check
    if (DATA_TO_MB.count(fromUnit) && DATA_TO_MB.count(toUnit)) {
        double inMb = val * DATA_TO_MB.at(fromUnit);
        double result = inMb / DATA_TO_MB.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str(), false, true};
    }

    // Currency Check
    auto itFrom = CURRENCY_ALIASES.find(fromUnit);
    auto itTo   = CURRENCY_ALIASES.find(toUnit);
    if (itFrom != CURRENCY_ALIASES.end() && itTo != CURRENCY_ALIASES.end()) {
        std::string codeFrom = itFrom->second;
        std::string codeTo   = itTo->second;

        bool hasLive = false;
        double rateFrom = 0.0, rateTo = 0.0;
        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
            bool fresh = (g_ratesCache.lastFetch.time_since_epoch().count() > 0) &&
                         (std::chrono::duration_cast<std::chrono::minutes>(now - g_ratesCache.lastFetch).count() < 120);

            if (fresh && g_ratesCache.rates.count(codeFrom) && g_ratesCache.rates.count(codeTo)) {
                rateFrom = g_ratesCache.rates.at(codeFrom);
                rateTo   = g_ratesCache.rates.at(codeTo);
                hasLive  = true;
            }
        }

        // if not cached or stale, trigger async fetch in the background
        if (!hasLive) {
            triggerAsyncFetch();
            // check if we have previous cached rates or fallback to offline table
            std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
            if (g_ratesCache.rates.count(codeFrom) && g_ratesCache.rates.count(codeTo)) {
                rateFrom = g_ratesCache.rates.at(codeFrom);
                rateTo   = g_ratesCache.rates.at(codeTo);
                hasLive  = true;
            } else if (OFFLINE_RATES_TO_USD.count(codeFrom) && OFFLINE_RATES_TO_USD.count(codeTo)) {
                rateFrom = OFFLINE_RATES_TO_USD.at(codeFrom);
                rateTo   = OFFLINE_RATES_TO_USD.at(codeTo);
            } else {
                return std::nullopt;
            }
        }

        if (rateTo <= 0.0) return std::nullopt;

        double inUsd = val * rateFrom;
        double result = inUsd / rateTo;
        std::ostringstream ss;
        ss << val << " " << codeFrom << " ≈ "
           << std::fixed << std::setprecision(2) << result << " " << codeTo;

        return ConversionResult{val, codeFrom, result, codeTo, ss.str(), true, hasLive};
    }

    return std::nullopt;
}

} // namespace Blueprint::Omnibox
