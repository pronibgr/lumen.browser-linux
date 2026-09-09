#pragma once
#include <string>
#include <optional>

#include <functional>

namespace Blueprint::Omnibox {

struct ConversionResult {
    double sourceValue;
    std::string sourceUnit;
    double targetValue;
    std::string targetUnit;
    std::string formatted;
    bool isCurrency = false;
    bool isLive = true;
};

class UnitConverter {
public:
    static std::optional<ConversionResult> convert(const std::string& query);
    static void setOnRatesUpdated(std::function<void()> cb);
};

} // namespace Blueprint::Omnibox
