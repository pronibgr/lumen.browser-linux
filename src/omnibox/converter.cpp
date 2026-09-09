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

// Canonical currency symbol / name mapping to standard 3-letter ISO code
const std::unordered_map<std::string, std::string> CURRENCY_ALIASES = {
    {"usd", "USD"}, {"$", "USD"}, {"доллар", "USD"}, {"доллара", "USD"}, {"долларов", "USD"}, {"bucks", "USD"},
    {"eur", "EUR"}, {"€", "EUR"}, {"евро", "EUR"},
    {"rub", "RUB"}, {"₽", "RUB"}, {"рубль", "RUB"}, {"рубля", "RUB"}, {"руб", "RUB"}, {"рублей", "RUB"},
    {"gbp", "GBP"}, {"£", "GBP"}, {"фунт", "GBP"}, {"фунта", "GBP"}, {"фунтов", "GBP"}, {"фунт_ст", "GBP"},
    {"cny", "CNY"}, {"¥", "CNY"}, {"юань", "CNY"}, {"юаня", "CNY"}, {"юаней", "CNY"},
    {"jpy", "JPY"}, {"иена", "JPY"}, {"иены", "JPY"}, {"йена", "JPY"}, {"йены", "JPY"},
    {"chf", "CHF"}, {"франк", "CHF"}, {"франков", "CHF"},
    {"cad", "CAD"}, {"aud", "AUD"}, {"kzt", "KZT"}, {"тенге", "KZT"},
    {"try", "TRY"}, {"лира", "TRY"}, {"лиры", "TRY"},
    {"pln", "PLN"}, {"злотый", "PLN"}, {"злотых", "PLN"},
    {"inr", "INR"}, {"рупия", "INR"}, {"рупий", "INR"},
    {"krw", "KRW"}, {"вона", "KRW"}, {"воны", "KRW"},
    {"brl", "BRL"}, {"sek", "SEK"}, {"nok", "NOK"}, {"dkk", "DKK"},
    {"sgd", "SGD"}, {"hkd", "HKD"}, {"thb", "THB"}, {"бат", "THB"},
    {"btc", "BTC"}, {"eth", "ETH"}
};

// Approximate offline fallback exchange rates (Base: USD = 1.0)
const std::unordered_map<std::string, double> OFFLINE_RATES_TO_USD = {
    {"USD", 1.0},
    {"EUR", 1.08},      // 1 EUR ≈ 1.08 USD
    {"GBP", 1.28},      // 1 GBP ≈ 1.28 USD
    {"RUB", 0.011},     // 1 RUB ≈ 0.011 USD (~90 RUB/USD)
    {"CNY", 0.14},      // 1 CNY ≈ 0.14 USD (~7.1 CNY/USD)
    {"JPY", 0.0067},    // 1 JPY ≈ 0.0067 USD (~150 JPY/USD)
    {"CHF", 1.13},      // 1 CHF ≈ 1.13 USD
    {"CAD", 0.73},      // 1 CAD ≈ 0.73 USD
    {"AUD", 0.66},      // 1 AUD ≈ 0.66 USD
    {"KZT", 0.0021},    // 1 KZT ≈ 0.0021 USD (~480 KZT/USD)
    {"TRY", 0.030},     // 1 TRY ≈ 0.030 USD (~33 TRY/USD)
    {"PLN", 0.25},      // 1 PLN ≈ 0.25 USD
    {"INR", 0.012},     // 1 INR ≈ 0.012 USD
    {"KRW", 0.00074},   // 1 KRW ≈ 0.00074 USD
    {"BRL", 0.18},      // 1 BRL ≈ 0.18 USD
    {"SEK", 0.096},     // 1 SEK ≈ 0.096 USD
    {"NOK", 0.094},     // 1 NOK ≈ 0.094 USD
    {"DKK", 0.145},     // 1 DKK ≈ 0.145 USD
    {"SGD", 0.76},      // 1 SGD ≈ 0.76 USD
    {"HKD", 0.128},     // 1 HKD ≈ 0.128 USD
    {"THB", 0.029},     // 1 THB ≈ 0.029 USD
    {"BTC", 65000.0},
    {"ETH", 3400.0}
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Thread-safe live rates cache and async fetch state
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

void triggerAsyncFetch() {
    bool expected = false;
    if (!g_ratesCache.fetching.compare_exchange_strong(expected, true)) {
        return; // Already fetching
    }

    std::thread([]() {
        std::string buffer;
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = "https://api.frankfurter.dev/v1/latest?base=USD";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1500L); // 1.5s max timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1200L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "lampa-browser/1.0");
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            if (res == CURLE_OK && !buffer.empty()) {
                // Parse "rates": { "EUR": 0.86, ... }
                std::regex ratesBlockRegex(R"raw("rates"\s*:\s*\{([^}]+)\})raw");
                std::smatch match;
                if (std::regex_search(buffer, match, ratesBlockRegex)) {
                    std::string ratesBody = match[1].str();
                    std::regex pairRegex(R"raw("([A-Za-z]{3})"\s*:\s*([0-9.]+))raw");
                    std::sregex_iterator next(ratesBody.begin(), ratesBody.end(), pairRegex);
                    std::sregex_iterator end;

                    std::unordered_map<std::string, double> newRates;
                    newRates["USD"] = 1.0;
                    while (next != end) {
                        std::smatch m = *next;
                        std::string code = m[1].str();
                        double rateFromUsd = std::stod(m[2].str());
                        if (rateFromUsd > 0.0) {
                            // Convert to "value of 1 CUR in USD"
                            newRates[code] = 1.0 / rateFromUsd;
                        }
                        ++next;
                    }

                    if (newRates.size() > 5) {
                        std::function<void()> notifyCb;
                        {
                            std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
                            // Preserve currencies not reported by Frankfurter (like RUB, KZT, BTC) with current/offline values
                            for (const auto& [code, val] : OFFLINE_RATES_TO_USD) {
                                if (newRates.find(code) == newRates.end()) {
                                    if (g_ratesCache.rates.find(code) != g_ratesCache.rates.end()) {
                                        newRates[code] = g_ratesCache.rates[code];
                                    } else {
                                        newRates[code] = val;
                                    }
                                }
                            }
                            g_ratesCache.rates = std::move(newRates);
                            g_ratesCache.lastFetch = std::chrono::steady_clock::now();
                            notifyCb = g_ratesCache.onUpdated;
                        }
                        if (notifyCb) notifyCb();
                    }
                }
            }
        }
        g_ratesCache.fetching.store(false);
    }).detach();
}

} // anonymous namespace

void UnitConverter::setOnRatesUpdated(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(g_ratesCache.mtx);
    g_ratesCache.onUpdated = std::move(cb);
}

std::optional<ConversionResult> UnitConverter::convert(const std::string& query) {
    if (query.empty()) return std::nullopt;

    std::string lower = toLower(query);
    std::istringstream iss(lower);
    double val = 0.0;
    std::string fromUnit, separator, toUnit;

    if (!(iss >> val >> fromUnit)) {
        return std::nullopt;
    }

    if (iss >> separator) {
        if (separator == "to" || separator == "in" || separator == "в" || separator == "->" || separator == "=") {
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
                         (std::chrono::duration_cast<std::chrono::minutes>(now - g_ratesCache.lastFetch).count() < 60);

            if (fresh && g_ratesCache.rates.count(codeFrom) && g_ratesCache.rates.count(codeTo)) {
                rateFrom = g_ratesCache.rates.at(codeFrom);
                rateTo   = g_ratesCache.rates.at(codeTo);
                hasLive  = true;
            }
        }

        // If not cached or stale, trigger async fetch in the background
        if (!hasLive) {
            triggerAsyncFetch();
            // Fallback to offline table
            if (OFFLINE_RATES_TO_USD.count(codeFrom) && OFFLINE_RATES_TO_USD.count(codeTo)) {
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
        ss << val << " " << codeFrom << " " << (hasLive ? "≈ " : "≈ ")
           << std::fixed << std::setprecision(2) << result << " " << codeTo;

        return ConversionResult{val, codeFrom, result, codeTo, ss.str(), true, hasLive};
    }

    return std::nullopt;
}

} // namespace Blueprint::Omnibox
