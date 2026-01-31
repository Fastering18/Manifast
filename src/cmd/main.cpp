#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/AST.h"
#include "manifast/CodeGen.h"

namespace fs = std::filesystem;

struct TestResult {
    std::string path;
    bool success;
    long long duration_ns;
};

// --- Helpers ---

bool runSilent(const std::string& source) {
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer);
    
    try {
        auto statements = parser.parse();
        manifast::CodeGen codegen;
        codegen.compile(statements);
        // codegen.run(); // For now just test if it compiles without crashing
        return true;
    } catch (...) {
        return false;
    }
}

void runVerbose(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(content, config);
    manifast::Parser parser(lexer);
    
    try {
        auto statements = parser.parse();
        std::cout << "--- Code Generation ---\n";
        manifast::CodeGen codegen;
        codegen.compile(statements);
        codegen.printIR();
        std::cout << "--- Execution ---\n";
        codegen.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Unknown Error.\n";
    }
}

void runTests(const std::string& root) {
    std::vector<std::string> files;
    if (fs::is_directory(root)) {
        for (auto const& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mnf") {
                files.push_back(entry.path().string());
            }
        }
    } else {
        files.push_back(root);
    }

    if (files.empty()) {
        std::cout << "No .mnf files found.\n";
        return;
    }

    std::sort(files.begin(), files.end());

    std::vector<TestResult> results;
    int total = files.size();
    int current = 0;

    std::cout << "Starting tests in: " << root << " (" << total << " files)\n\n";

    for (const auto& path : files) {
        current++;
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        auto start = std::chrono::high_resolution_clock::now();
        bool success = runSilent(content);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        results.push_back({path, success, duration});

        std::cout << "[" << current << "/" << total << "] Running " << fs::path(path).filename().string() << "... " 
                  << (success ? "OK" : "FAIL") << " (" << duration / 1000 << " us)\n";
    }

    // Summary
    int passed = std::count_if(results.begin(), results.end(), [](const auto& r) { return r.success; });
    long long total_ns = 0;
    long long min_ns = results[0].duration_ns;
    long long max_ns = results[0].duration_ns;

    for (const auto& r : results) {
        total_ns += r.duration_ns;
        if (r.duration_ns < min_ns) min_ns = r.duration_ns;
        if (r.duration_ns > max_ns) max_ns = r.duration_ns;
    }

    double avg_ns = (double)total_ns / total;
    double success_rate = (double)passed / total * 100.0;

    std::cout << "\n-------------------------------------\n";
    std::cout << "SUMMARY\n";
    std::cout << "-------------------------------------\n";
    std::cout << "Total Files   : " << total << "\n";
    std::cout << "Success       : " << passed << " (" << std::fixed << std::setprecision(1) << success_rate << "%)\n";
    std::cout << "Failed        : " << (total - passed) << "\n\n";

    std::cout << "EXECUTION TIME (us)\n";
    std::cout << "Total         : " << total_ns / 1000 << "\n";
    std::cout << "Average       : " << avg_ns / 1000 << "\n";
    std::cout << "Min           : " << min_ns / 1000 << "\n";
    std::cout << "Max           : " << max_ns / 1000 << "\n";
    std::cout << "-------------------------------------\n";
}

void runREPL() {
    std::cout << "Manifast 0.1.0 (REPL)\n";
    std::string line;
    manifast::SyntaxConfig config;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "keluar") break; 
        
        manifast::Lexer lexer(line, config);
        manifast::Token t;
        do {
            t = lexer.nextToken();
            std::cout << "[" << manifast::tokenTypeToString(t.type) << "] '" << t.lexeme << "'\n";
        } while (t.type != manifast::TokenType::EndOfFile);
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--test" && argc > 2) {
            runTests(argv[2]);
        } else if (fs::is_directory(arg)) {
            runTests(arg);
        } else {
            runVerbose(arg);
        }
    } else {
        runREPL();
    }
    return 0;
}
