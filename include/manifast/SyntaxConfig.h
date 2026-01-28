#pragma once

#include "Token.h"
#include <unordered_map>
#include <string_view>

namespace manifast {

class SyntaxConfig {
public:
    SyntaxConfig() {
        // Initialize with default Indonesian Grammar
        keywords["jika"] = TokenType::K_If;
        keywords["maka"] = TokenType::K_Then;
        keywords["tutup"] = TokenType::K_End;
        
        // Primary keyword mappings
        // 'kalau' and 'sebaliknya' both map to Else for flexibility
        keywords["kalau"] = TokenType::K_Else; 
        keywords["sebaliknya"] = TokenType::K_Else;
        
        keywords["fungsi"] = TokenType::K_Function;
        keywords["kembali"] = TokenType::K_Return;
        keywords["lokal"] = TokenType::K_Var;
        keywords["tetap"] = TokenType::K_Const;
        keywords["selama"] = TokenType::K_While; // changed from selagi
        keywords["untuk"] = TokenType::K_For;
        keywords["benar"] = TokenType::K_True;
        keywords["salah"] = TokenType::K_False;
        keywords["nil"] = TokenType::K_Null; // changed from nihil
        
        // Loop / Flow
        keywords["ke"] = TokenType::K_To;
        keywords["langkah"] = TokenType::K_Step;
        keywords["lakukan"] = TokenType::K_Do;
        keywords["coba"] = TokenType::K_Try;
        keywords["tangkap"] = TokenType::K_Catch;
        
        // Types
        keywords["string"] = TokenType::K_String;
        keywords["boolean"] = TokenType::K_Boolean;
        keywords["int32"] = TokenType::K_Int32;
    }

    TokenType lookupKeyword(std::string_view text) const {
        auto it = keywords.find(std::string(text)); // string_view lookup in C++20 map is cleaner, casting to string for safety if pre-C++20 heterogenous lookup issues
        if (it != keywords.end()) {
            return it->second;
        }
        return TokenType::Identifier;
    }

private:
    std::unordered_map<std::string, TokenType> keywords;
};

} // namespace manifast
