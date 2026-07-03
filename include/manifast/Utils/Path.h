#pragma once

#include <string>

namespace manifast {
namespace utils {

inline bool isSafePath(const std::string& path) {
    if (path.empty()) return true;

    // Reject leading dash to prevent options injection if used in commands
    if (path[0] == '-') {
        return false;
    }

    for (char c : path) {
        bool isAlphaNum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        // Added +, ~, (, ) as per reviewer comment. Kept others.
        // Assuming non-ASCII should be allowed? The reviewer says "current checks reject some valid path characters like +, ~, parentheses, and non-ASCII."
        // For non-ASCII in C++ string (which is char), if it's UTF-8, characters have the high bit set.
        bool isNonAscii = static_cast<unsigned char>(c) >= 128;
        if (!(isAlphaNum || isNonAscii || c == '.' || c == '/' || c == '\\' || c == '_' || c == '-' || c == ' ' || c == ':' || c == '+' || c == '~' || c == '(' || c == ')')) {
            return false;
        }
    }
    return true;
}

} // namespace utils
} // namespace manifast
