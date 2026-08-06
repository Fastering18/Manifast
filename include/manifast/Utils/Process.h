#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#endif

namespace manifast {
namespace utils {

// Run an argv-style command without a shell (avoids command injection via paths).
// Returns process exit code, or -1 on launch/wait failure.
inline int runCommand(const std::vector<std::string>& args) {
    if (args.empty()) return -1;

#ifdef _WIN32
    // Build a quoted Windows command line for CreateProcessA
    auto quoteArg = [](const std::string& arg) -> std::string {
        // Escape per CreateProcess rules: wrap in ", double internal quotes
        std::string out = "\"";
        for (char c : arg) {
            if (c == '"') out += "\\\"";
            else out += c;
        }
        out += "\"";
        return out;
    };

    std::string cmdLine;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdLine += ' ';
        cmdLine += quoteArg(args[i]);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(
            /*lpApplicationName*/ nullptr,
            cmdBuffer.data(),
            nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    pid_t pid = fork();
    if (pid == -1) {
        return -1;
    }
    if (pid == 0) {
        std::vector<char*> c_args;
        c_args.reserve(args.size() + 1);
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

} // namespace utils
} // namespace manifast
