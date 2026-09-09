#pragma once
#include <string>
#include <optional>

namespace Blueprint::Omnibox {

class Calculator {
public:
    static std::optional<double> evaluate(const std::string& expression);
    static std::string formatResult(double value);
};

} // namespace Blueprint::Omnibox
