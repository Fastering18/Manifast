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
        // Allowlist for valid path characters.
        // This explicitly allows standard punctuation (+, ~, (, )), space, standard path separators,
        // and non-ASCII characters (for UTF-8 compatibility) to ensure valid paths are not rejected.
        bool isNonAscii = static_cast<unsigned char>(c) >= 128;
        if (!(isAlphaNum || isNonAscii || c == '.' || c == '/' || c == '\\' || c == '_' || c == '-' || c == ' ' || c == ':' || c == '+' || c == '~' || c == '(' || c == ')')) {
            return false;
        }
    }
    return true;
}

} // namespace utils
} // namespace manifast
