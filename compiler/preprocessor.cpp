#include "std.h"
#include "preprocessor.h"
#include "ex.h"
#include "toker.h"
#include "../MultiLang/MultiLang.h"
#include <algorithm>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <format>
#include <cctype>

std::map<std::string, int> precedence = {
        {"||", 1},
        {"&&", 2},
        {"==", 3},
        {"!=", 3},
        {"<", 3},
        {"<=", 3},
        {">", 3},
        {">=", 3}
};

PreprocessorOps::PreprocessorOps(const std::string& v, Type t) : value(v), type(t) {}

PreprocessorOps PreprocessorOps::fromNumber(const std::string& num) {
    return PreprocessorOps(num, NUMBER);
}

PreprocessorOps PreprocessorOps::fromBoolean(bool b) {
    return PreprocessorOps(b ? "true" : "false", BOOLEAN);
}

bool PreprocessorOps::toBoolean() const {
    if (type == BOOLEAN) {
        return value == "true";
    }
    try {
        return std::stod(value) != 0;
    }
    catch (...) {
        return false;
    }
}

bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtod(s.c_str(), &end);
    return *end == '\0';
}

static std::vector<std::string> tokenizeExpression(const std::string& expr) {
    std::vector<std::string> tokens;
    std::string current;
    for (size_t i = 0; i < expr.size(); ++i) {
        char c = expr[i];
        if (isspace(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        if ((c == '|' && i + 1 < expr.size() && expr[i + 1] == '|') ||
            (c == '&' && i + 1 < expr.size() && expr[i + 1] == '&') ||
            (c == '=' && i + 1 < expr.size() && expr[i + 1] == '=') ||
            (c == '!' && i + 1 < expr.size() && expr[i + 1] == '=') ||
            (c == '<' && i + 1 < expr.size() && expr[i + 1] == '=') ||
            (c == '>' && i + 1 < expr.size() && expr[i + 1] == '=')) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(expr.substr(i, 2));
            ++i;
            continue;
        }

        if (c == '(' || c == ')' || c == '!' || c == '<' || c == '>' || c == '=') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, c));
            continue;
        }
        current += c;
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

static std::vector<std::string> toRPN(const std::vector<std::string>& tokens) {
    std::vector<std::string> output;
    std::stack<std::string> ops;

    for (const auto& token : tokens) {
        if (token == "(") {
            ops.push(token);
        }
        else if (token == ")") {
            while (!ops.empty() && ops.top() != "(") {
                output.push_back(ops.top());
                ops.pop();
            }
            if (!ops.empty()) ops.pop();
        }
        else if (precedence.count(token)) {
            while (!ops.empty() && ops.top() != "(" && precedence[ops.top()] >= precedence[token]) {
                output.push_back(ops.top());
                ops.pop();
            }
            ops.push(token);
        }
        else {
            output.push_back(token);
        }
    }
    while (!ops.empty()) {
        output.push_back(ops.top());
        ops.pop();
    }
    return output;
}

static bool evaluateRPN(const std::vector<std::string>& rpn) {
    std::stack<PreprocessorOps> st;
    for (const auto& tok : rpn) {
        if (precedence.count(tok)) {
            if (st.size() < 2) throw Ex("Not enough operands");
            PreprocessorOps rhs = st.top(); st.pop();
            PreprocessorOps lhs = st.top(); st.pop();
            PreprocessorOps result = evaluateOperation(lhs, rhs, tok);
            st.push(result);
        }
        else {
            st.push(processToken(tok));
        }
    }
    if (st.size() != 1) throw Ex("Invalid expression");
    return st.top().toBoolean();
}

bool evaluateExpression(const std::string& expr) {
    std::vector<std::string> tokens = tokenizeExpression(expr);
    std::vector<std::string> rpn = toRPN(tokens);
    return evaluateRPN(rpn);
}

PreprocessorOps processToken(const std::string& token) {
    if (MacroDefines.contains(token)) {
        std::string value = MacroDefines.at(token);
        if (isNumber(value))
            return PreprocessorOps::fromNumber(value);
        else if (value == "true" || value == "false")
            return PreprocessorOps::fromBoolean(value == "true");
        else
            return PreprocessorOps(value, PreprocessorOps::NUMBER);
    }
    if (isNumber(token)) {
        return PreprocessorOps::fromNumber(token);
    }
    if (token == "true" || token == "false") {
        return PreprocessorOps::fromBoolean(token == "true");
    }

    return PreprocessorOps::fromNumber("0");
}

PreprocessorOps evaluateOperation(const PreprocessorOps& lhs, const PreprocessorOps& rhs, const std::string& op) {
    if (op == "&&") {
        bool lhsValue = lhs.toBoolean();
        bool rhsValue = rhs.toBoolean();
        return PreprocessorOps::fromBoolean(lhsValue && rhsValue);
    }
    if (op == "||") {
        bool lhsValue = lhs.toBoolean();
        bool rhsValue = rhs.toBoolean();
        return PreprocessorOps::fromBoolean(lhsValue || rhsValue);
    }

    double lhsNum = std::stod(lhs.value);
    double rhsNum = std::stod(rhs.value);
    bool result;
    if (op == ">") result = lhsNum > rhsNum;
    else if (op == ">=") result = lhsNum >= rhsNum;
    else if (op == "<") result = lhsNum < rhsNum;
    else if (op == "<=") result = lhsNum <= rhsNum;
    else if (op == "==") result = lhsNum == rhsNum;
    else if (op == "!=") result = lhsNum != rhsNum;
    else throw Ex(std::format(MultiLang::unsupported_operator, op));
    return PreprocessorOps::fromBoolean(result);
}