#include "omnibox/calc_parser.hpp"
#include <vector>
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Blueprint::Omnibox {

namespace {

enum class TokenType {
    Number,
    Plus,
    Minus,
    Multiply,
    Divide,
    Power,
    Modulo,
    LParen,
    RParen,
    Function,
    End
};

struct Token {
    TokenType type;
    double value = 0.0;
    std::string text;
};

class Lexer {
public:
    explicit Lexer(const std::string& input) : m_src(input), m_pos(0) {}

    Token nextToken() {
        skipWhitespace();
        if (m_pos >= m_src.length()) {
            return {TokenType::End, 0.0, ""};
        }

        char c = m_src[m_pos];

        if (std::isdigit(c) || c == '.') {
            size_t start = m_pos;
            bool hasDot = (c == '.');
            m_pos++;
            while (m_pos < m_src.length() && (std::isdigit(m_src[m_pos]) || (!hasDot && m_src[m_pos] == '.'))) {
                if (m_src[m_pos] == '.') hasDot = true;
                m_pos++;
            }
            std::string numStr = m_src.substr(start, m_pos - start);
            try {
                return {TokenType::Number, std::stod(numStr), numStr};
            } catch (...) {
                return {TokenType::End, 0.0, ""};
            }
        }

        if (std::isalpha(c)) {
            size_t start = m_pos;
            while (m_pos < m_src.length() && std::isalpha(m_src[m_pos])) {
                m_pos++;
            }
            std::string name = m_src.substr(start, m_pos - start);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            return {TokenType::Function, 0.0, name};
        }

        m_pos++;
        switch (c) {
            case '+': return {TokenType::Plus, 0.0, "+"};
            case '-': return {TokenType::Minus, 0.0, "-"};
            case '*': return {TokenType::Multiply, 0.0, "*"};
            case '/': return {TokenType::Divide, 0.0, "/"};
            case '^': return {TokenType::Power, 0.0, "^"};
            case '%': return {TokenType::Modulo, 0.0, "%"};
            case '(': return {TokenType::LParen, 0.0, "("};
            case ')': return {TokenType::RParen, 0.0, ")"};
            default: return {TokenType::End, 0.0, ""};
        }
    }

private:
    void skipWhitespace() {
        while (m_pos < m_src.length() && std::isspace(m_src[m_pos])) {
            m_pos++;
        }
    }

    std::string m_src;
    size_t m_pos;
};

// Recursive Descent Parser
class Parser {
public:
    explicit Parser(const std::string& input) : m_lexer(input) {
        m_curr = m_lexer.nextToken();
    }

    std::optional<double> parse() {
        try {
            double res = parseExpression();
            if (m_curr.type != TokenType::End) {
                return std::nullopt;
            }
            return res;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    Lexer m_lexer;
    Token m_curr;

    void consume(TokenType expected) {
        if (m_curr.type == expected) {
            m_curr = m_lexer.nextToken();
        } else {
            throw false;
        }
    }

    double parseExpression() {
        double result = parseTerm();
        while (m_curr.type == TokenType::Plus || m_curr.type == TokenType::Minus) {
            TokenType op = m_curr.type;
            consume(op);
            double nextTerm = parseTerm();
            if (op == TokenType::Plus) result += nextTerm;
            else result -= nextTerm;
        }
        return result;
    }

    double parseTerm() {
        double result = parseFactor();
        while (m_curr.type == TokenType::Multiply || m_curr.type == TokenType::Divide || m_curr.type == TokenType::Modulo) {
            TokenType op = m_curr.type;
            consume(op);
            double nextFactor = parseFactor();
            if (op == TokenType::Multiply) result *= nextFactor;
            else if (op == TokenType::Divide) {
                if (std::abs(nextFactor) < 1e-12) throw false;
                result /= nextFactor;
            } else if (op == TokenType::Modulo) {
                if (std::abs(nextFactor) < 1e-12) throw false;
                result = std::fmod(result, nextFactor);
            }
        }
        return result;
    }

    double parseFactor() {
        double result = parsePrimary();
        if (m_curr.type == TokenType::Power) {
            consume(TokenType::Power);
            double exponent = parseFactor(); // right-associative
            result = std::pow(result, exponent);
        }
        return result;
    }

    double parsePrimary() {
        if (m_curr.type == TokenType::Plus) {
            consume(TokenType::Plus);
            return parsePrimary();
        }
        if (m_curr.type == TokenType::Minus) {
            consume(TokenType::Minus);
            return -parsePrimary();
        }
        if (m_curr.type == TokenType::Number) {
            double val = m_curr.value;
            consume(TokenType::Number);
            return val;
        }
        if (m_curr.type == TokenType::LParen) {
            consume(TokenType::LParen);
            double val = parseExpression();
            consume(TokenType::RParen);
            return val;
        }
        if (m_curr.type == TokenType::Function) {
            std::string fn = m_curr.text;
            consume(TokenType::Function);
            consume(TokenType::LParen);
            double arg = parseExpression();
            consume(TokenType::RParen);

            if (fn == "sqrt") return (arg >= 0) ? std::sqrt(arg) : throw false;
            if (fn == "abs") return std::abs(arg);
            if (fn == "sin") return std::sin(arg);
            if (fn == "cos") return std::cos(arg);
            if (fn == "tan") return std::tan(arg);
            if (fn == "log") return (arg > 0) ? std::log10(arg) : throw false;
            if (fn == "ln") return (arg > 0) ? std::log(arg) : throw false;
            if (fn == "exp") return std::exp(arg);
            throw false;
        }
        throw false;
    }
};

} // anonymous namespace

std::optional<double> Calculator::evaluate(const std::string& expression) {
    if (expression.empty()) return std::nullopt;

    // Check if expression contains any math operators or functions
    bool hasOperator = false;
    for (char c : expression) {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%' || c == '(' || c == ')') {
            hasOperator = true;
            break;
        }
    }
    if (!hasOperator && expression.find("sqrt") == std::string::npos && expression.find("sin") == std::string::npos) {
        return std::nullopt;
    }

    Parser parser(expression);
    return parser.parse();
}

std::string Calculator::formatResult(double value) {
    if (std::isnan(value) || std::isinf(value)) return "Error";
    std::ostringstream oss;
    if (std::abs(value - std::round(value)) < 1e-7) {
        oss << static_cast<long long>(std::round(value));
    } else {
        oss << std::setprecision(8) << value;
    }
    return oss.str();
}

} // namespace Blueprint::Omnibox
