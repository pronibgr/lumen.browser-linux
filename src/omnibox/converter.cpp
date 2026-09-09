#include "omnibox/converter.hpp"
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <cctype>

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

// Currencies base: USD
const std::unordered_map<std::string, double> CURRENCY_TO_USD = {
    {"usd", 1.0}, {"$", 1.0}, {"доллар", 1.0}, {"долларов", 1.0},
    {"eur", 1.08}, {"€", 1.08}, {"евро", 1.08},
    {"gbp", 1.28}, {"£", 1.28}, {"фунт_ст", 1.28},
    {"rub", 0.011}, {"₽", 0.011}, {"рубль", 0.011}, {"руб", 0.011}, {"рублей", 0.011},
    {"cny", 0.14}, {"юань", 0.14}, {"юаней", 0.14},
    {"jpy", 0.0067}, {"иена", 0.0067}, {"йена", 0.0067},
    {"btc", 65000.0}, {"eth", 3400.0}
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // anonymous namespace

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
        if (separator == "to" || separator == "in" || separator == "в" || separator == "->") {
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
        return ConversionResult{val, "°C", result, "°F", ss.str()};
    }
    if ((fromUnit == "f" || fromUnit == "fahrenheit") && (toUnit == "c" || toUnit == "celsius")) {
        double result = (val - 32.0) * 5.0 / 9.0;
        std::ostringstream ss;
        ss << val << " °F = " << std::fixed << std::setprecision(2) << result << " °C";
        return ConversionResult{val, "°F", result, "°C", ss.str()};
    }

    // Length Check
    if (LENGTH_TO_M.count(fromUnit) && LENGTH_TO_M.count(toUnit)) {
        double inMeters = val * LENGTH_TO_M.at(fromUnit);
        double result = inMeters / LENGTH_TO_M.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str()};
    }

    // Weight Check
    if (WEIGHT_TO_KG.count(fromUnit) && WEIGHT_TO_KG.count(toUnit)) {
        double inKg = val * WEIGHT_TO_KG.at(fromUnit);
        double result = inKg / WEIGHT_TO_KG.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str()};
    }

    // Data Check
    if (DATA_TO_MB.count(fromUnit) && DATA_TO_MB.count(toUnit)) {
        double inMb = val * DATA_TO_MB.at(fromUnit);
        double result = inMb / DATA_TO_MB.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " = " << std::setprecision(6) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str()};
    }

    // Currency Check
    if (CURRENCY_TO_USD.count(fromUnit) && CURRENCY_TO_USD.count(toUnit)) {
        double inUsd = val * CURRENCY_TO_USD.at(fromUnit);
        double result = inUsd / CURRENCY_TO_USD.at(toUnit);
        std::ostringstream ss;
        ss << val << " " << fromUnit << " ≈ " << std::fixed << std::setprecision(2) << result << " " << toUnit;
        return ConversionResult{val, fromUnit, result, toUnit, ss.str()};
    }

    return std::nullopt;
}

} // namespace Blueprint::Omnibox
