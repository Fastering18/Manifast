#pragma once

#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace manifast {
namespace utils {

inline int runCommand(const std::vector<std::string>& args) {
    if (args.empty()) return -1;
#ifdef _WIN32
    std::string cmdLine;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdLine += " ";
        std::string arg = args[i];
        std::string escaped_arg;
        for (char c : arg) {
            if (c == '"') escaped_arg += "\\\"";
            else escaped_arg += c;
        }
        cmdLine += "\"" + escaped_arg + "\"";
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
#else
    pid_t pid = fork();
    if (pid == -1) {
        return -1;
    } else if (pid == 0) {
        std::vector<char*> c_args;
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
#endif
}

} // namespace utils
} // namespace manifast
