#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <map>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <fmt/core.h>
#include <fmt/color.h>

#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/VM/Compiler.h"
#include "manifast/VM/VM.h"
#include "manifast/CodeGen.h" 

#ifdef _WIN32
#include <io.h>
#define ISATTY(fd) _isatty(fd)
#define DUP _dup
#define DUP2 _dup2
#define FILENO _fileno
#define CLOSE _close
#define NULL_DEVICE "NUL"
#else
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#define DUP dup
#define DUP2 dup2
#define FILENO fileno
#define CLOSE close
#define NULL_DEVICE "/dev/null"
#endif

namespace fs = std::filesystem;

struct TestResult {
    std::string name;
    double durationMs;
    bool success;
};

bool g_isInteractive = false;

// Helper to silence stdout/stderr during test execution and capture it
class OutputSilencer {
    int old_stdout;
    int old_stderr;
    FILE* log_file;
    bool active = false;
    std::string logPath;
public:
    OutputSilencer(const std::string& path) : logPath(path) {
        std::fflush(stdout);
        std::fflush(stderr);
        old_stdout = DUP(FILENO(stdout));
        old_stderr = DUP(FILENO(stderr));
        
        log_file = std::fopen(logPath.c_str(), "w");
        if (log_file) {
            DUP2(FILENO(log_file), FILENO(stdout));
            DUP2(FILENO(log_file), FILENO(stderr));
            active = true;
        }
    }
    
    ~OutputSilencer() {
        if (active) {
            std::fflush(stdout);
            std::fflush(stderr);
            DUP2(old_stdout, FILENO(stdout));
            DUP2(old_stderr, FILENO(stderr));
            std::fclose(log_file);
            CLOSE(old_stdout);
            CLOSE(old_stderr);
        }
    }

    std::string getLog() {
        if (logPath == NULL_DEVICE) return "";
        std::ifstream f(logPath);
        if (!f) return "";
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
};

bool runTestInProcess(const std::string& source, std::string& outputLog, bool useVM, manifast::vm::Compiler* reusableCompiler = nullptr, manifast::vm::VM* reusableVM = nullptr) {
    const std::string tempLog = "test_run.tmp";
    OutputSilencer silencer(tempLog);

    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer);

    bool finalSuccess = false;
    try {
        auto statements = parser.parse();
        if (parser.hadError()) {
            finalSuccess = false;
        } else if (statements.empty()) {
            finalSuccess = true;
        } else {
            if (useVM) {
                manifast::vm::Chunk chunk;
                bool compilationSuccess = false;
                if (reusableCompiler) compilationSuccess = reusableCompiler->compile(statements, chunk);
                else {
                    manifast::vm::Compiler compiler;
                    compilationSuccess = compiler.compile(statements, chunk);
                }

                if (compilationSuccess) {
                    if (reusableVM) reusableVM->interpret(&chunk);
                    else {
                        manifast::vm::VM vm;
                        vm.interpret(&chunk);
                    }
                    chunk.free();
                    finalSuccess = true;
                } else finalSuccess = false;
            } else {
                manifast::CodeGen codegen;
                codegen.compile(statements);
                finalSuccess = codegen.run();
            }
        }
    } catch (const manifast::RuntimeError& e) {
        finalSuccess = false;
    } catch (const std::exception& e) {
        finalSuccess = false;
    } catch (...) {
        finalSuccess = false;
    }

    outputLog = silencer.getLog();
    return finalSuccess;
}


void printUsage() {
    fmt::print(fmt::emphasis::bold, "Manifast Management Tool (mifast) v0.0.12\n");
    fmt::print("Usage: mifast <command> [args]\n\n");
    fmt::print("Commands:\n");
    fmt::print("  run <file> [--vm]    Compile and run a Manifast file\n");
    fmt::print("  test [--vm]          Run the project test suite (In-Process)\n");
}

void runTestRunner(bool useVM) {
    if (g_isInteractive) {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "🚀 Starting Manifast Test Suite ({}) ...\n\n", useVM ? "Bytecode VM" : "LLVM JIT");
    } else {
        fmt::print("Starting Manifast Test Suite ({})...\n\n", useVM ? "Bytecode VM" : "LLVM JIT");
    }
    
    // Persistent instances for VM mode
    std::unique_ptr<manifast::vm::Compiler> sharedCompiler;
    std::unique_ptr<manifast::vm::VM> sharedVM;
    
    if (useVM) {
        sharedCompiler = std::make_unique<manifast::vm::Compiler>();
        sharedVM = std::make_unique<manifast::vm::VM>();
    }
    
    std::map<std::string, std::vector<TestResult>> resultsByCategory;
    std::vector<fs::path> testFiles;

    if (fs::exists("tests")) {
        for (auto const& dir_entry : fs::recursive_directory_iterator("tests")) {
            if (dir_entry.path().extension() == ".mnf") {
                testFiles.push_back(dir_entry.path());
            }
        }
    }

    if (testFiles.empty()) {
        fmt::print(fg(fmt::color::red), "No test files found in 'tests/' directory.\n");
        return;
    }

    size_t total = testFiles.size();
    size_t completed = 0;

    for (const auto& file : testFiles) {
        completed++;
        
        if (g_isInteractive) {
            float progress = (float)completed / total;
            int barWidth = 30;
            fmt::print("\r[");
            int pos = barWidth * progress;
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) fmt::print(fg(fmt::color::green), "━");
                else if (i == pos) fmt::print(fg(fmt::color::green), "╾");
                else fmt::print(" ");
            }
            fmt::print("] {:3.0f}% | Testing: {}\033[K", progress * 100.0, file.filename().string());
            std::fflush(stdout);
        }

        std::string rel = fs::relative(file, "tests").parent_path().string();
        if (rel.empty()) rel = "default";
        std::replace(rel.begin(), rel.end(), '\\', '/');

        // Pre-read file to exclude I/O from benchmark
        std::ifstream fstream(file);
        if (!fstream) continue;
        std::string source((std::istreambuf_iterator<char>(fstream)), std::istreambuf_iterator<char>());

        auto start = std::chrono::high_resolution_clock::now();
        
        std::string logOutput;
        bool success = runTestInProcess(source, logOutput, useVM, sharedCompiler.get(), sharedVM.get());
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        if (!success) {
             if (g_isInteractive) {
                fmt::print(fg(fmt::color::red), "\n[FAIL] {} ({:.2f} ms)\n", file.filename().string(), elapsed.count());
             } else {
                fmt::print("\n[FAIL] {} ({:.2f} ms)\n", file.filename().string(), elapsed.count());
             }
             fmt::print("Log:\n{}\n", logOutput);
        }

        resultsByCategory[rel].push_back({file.filename().string(), elapsed.count(), success});
    }

    fmt::print("\n\n");
    fmt::print(fmt::emphasis::bold, "Test Metrics Summary ({})\n", useVM ? "VM" : "JIT");
    fmt::print("┌──────────────────────────┬────────────┬────────────┬────────────┬────────────┐\n");
    fmt::print("│ Category                 │ Pass/Fail  │ Min (ms)   │ Avg (ms)   │ Max (ms)   │\n");
    fmt::print("├──────────────────────────┼────────────┼────────────┼────────────┼────────────┤\n");

    for (auto const& [category, results] : resultsByCategory) {
        int passed = 0;
        double minTime = 1e9, maxTime = 0, sumTime = 0;
        for (auto const& r : results) {
            if (r.success) passed++;
            minTime = std::min(minTime, r.durationMs);
            maxTime = std::max(maxTime, r.durationMs);
            sumTime += r.durationMs;
        }
        double avgTime = sumTime / results.size();

        std::string stats = fmt::format("{}/{}", passed, results.size());
        auto statusColor = (passed == results.size()) ? fg(fmt::color::green) : fg(fmt::color::red);

        fmt::print("│ {:<24} │ ", category);
        if (g_isInteractive) {
            fmt::print(statusColor, "{:<10}", stats);
        } else {
            fmt::print("{:<10}", stats);
        }
        fmt::print(" │ {:<10.2f} │ {:<10.2f} │ {:<10.2f} │\n", minTime, avgTime, maxTime);
    }
    fmt::print("└──────────────────────────┴────────────┴────────────┴────────────┴────────────┘\n");
}

int main(int argc, char* argv[]) {
    g_isInteractive = ISATTY(1); 

    if (argc < 2) {
        printUsage();
        return 0;
    }

    std::string cmd = argv[1];
    
    // Simple argument parsing
    bool useVM = false;
    bool debugDev = false;
    std::string filePath;
    
    // Check for --vm in any position after command
    for(int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if(arg == "--vm") useVM = true;
        else if(arg == "--debugdev") debugDev = true;
        else if (filePath.empty()) filePath = arg;
    }

    if (cmd == "test") {
        runTestRunner(useVM);
    } else if (cmd == "run") {
        if (filePath.empty()) {
            fmt::print(fg(fmt::color::red), "Error: No file specified.\n");
            return 1;
        }
        
        std::ifstream file(filePath);
        if (!file) {
            fmt::print(fg(fmt::color::red), "Error: Could not open file.\n");
            return 1;
        }
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        manifast::SyntaxConfig config;
        manifast::Lexer lexer(source, config);
        manifast::Parser parser(lexer);
        parser.debugMode = debugDev;
        try {
            auto statements = parser.parse();
            
            if (useVM) {
                manifast::vm::Chunk chunk;
                manifast::vm::Compiler compiler;
                compiler.debugMode = debugDev;
                if (compiler.compile(statements, chunk)) {
                    manifast::vm::VM vm;
                    vm.debugMode = debugDev;
                    vm.interpret(&chunk);
                    chunk.free();
                } else {
                     fmt::print(fg(fmt::color::red), "Compilation failed.\n");
                     return 1;
                }
            } else {
                manifast::CodeGen codegen;
                codegen.compile(statements);
                if (!codegen.run()) return 1;
            }

        } catch (const std::exception& e) {
            fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
            return 1;
        }
    } else {
        fmt::print(fg(fmt::color::red), "Unknown command: {}\n", cmd);
        return 1;
    }

    return 0;
}
