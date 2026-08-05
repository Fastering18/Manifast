#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "manifast/Runtime.h"
#include "manifast/AST.h"

#ifdef _WIN32
#include <io.h>
#define ISATTY_FD(fd) _isatty(fd)
#define DUP_FD _dup
#define DUP2_FD _dup2
#define FILENO_FD _fileno
#define CLOSE_FD _close
#else
#include <unistd.h>
#define ISATTY_FD(fd) isatty(fd)
#define DUP_FD dup
#define DUP2_FD dup2
#define FILENO_FD fileno
#define CLOSE_FD close
#endif

using namespace manifast;
namespace fs = std::filesystem;

// Portable stdout capture via temp file + fd redirect (MSVC + POSIX)
std::string captureStdout(void (*func)(Any*, Any*), Any* arg1, Any* arg2) {
    const auto tmpPath = fs::temp_directory_path() / "manifast_test_stdout.tmp";
    const std::string path = tmpPath.string();

    std::fflush(stdout);
    int old_stdout = DUP_FD(FILENO_FD(stdout));
    if (old_stdout < 0) return "";

    FILE* log_file = std::fopen(path.c_str(), "w");
    if (!log_file) {
        CLOSE_FD(old_stdout);
        return "";
    }

    DUP2_FD(FILENO_FD(log_file), FILENO_FD(stdout));

    func(arg1, arg2);

    std::fflush(stdout);
    DUP2_FD(old_stdout, FILENO_FD(stdout));
    CLOSE_FD(old_stdout);
    std::fclose(log_file);

    std::ifstream in(path, std::ios::binary);
    std::string result((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::error_code ec;
    fs::remove(tmpPath, ec);
    return result;
}

void test_manifast_printfmt() {
    Any fmt;
    fmt.type = 1; // String type in Manifast Any
    fmt.ptr = (void*)"%d";

    Any val;
    val.type = 0; // Number type
    val.number = 42;

    std::string out = captureStdout(manifast_printfmt, &fmt, &val);
    assert(out == "42" && "Expected output 42 for number type");

    val.type = 1; // String
    val.ptr = (void*)"hello";
    fmt.ptr = (void*)"%s";
    out = captureStdout(manifast_printfmt, &fmt, &val);
    assert(out == "hello" && "Expected output hello for string type");

    val.type = 2; // Bool
    val.number = 1;
    out = captureStdout(manifast_printfmt, &fmt, &val);
    assert(out == "benar" && "Expected output benar for boolean true");

    val.type = 3; // Nil
    out = captureStdout(manifast_printfmt, &fmt, &val);
    assert(out == "nil" && "Expected output nil for nil type");

    std::cout << "test_manifast_printfmt passed!" << std::endl;
}

int main() {
    test_manifast_printfmt();
    std::cout << "All C++ Runtime tests passed!" << std::endl;
    return 0;
}
