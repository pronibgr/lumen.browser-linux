#pragma once
#include <string>
#include <optional>

namespace Blueprint::Omnibox {

struct ConversionResult {
    double sourceValue;
    std::string sourceUnit;
    double targetValue;
    std::string targetUnit;
    std::string formatted;
};

class UnitConverter {
public:
    static std::optional<ConversionResult> convert(const std::string& query);
};

} // namespace Blueprint::Omnibox
