#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include "manifast/Runtime.h"
#include "manifast/AST.h"

using namespace manifast;

// Helper to capture stdout
std::string captureStdout(void (*func)(Any*, Any*), Any* arg1, Any* arg2) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    int old_stdout = dup(fileno(stdout));
    fflush(stdout);
    dup2(pipefd[1], fileno(stdout));

    func(arg1, arg2);

    fflush(stdout);
    dup2(old_stdout, fileno(stdout));
    close(pipefd[1]);
    close(old_stdout);

    char buf[1024];
    std::string result;
    ssize_t n;
    // Non-blocking read or careful close so we don't hang
    while ((n = read(pipefd[0], buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        result += buf;
        if (n < (ssize_t)sizeof(buf)-1) break;
    }
    close(pipefd[0]);

    return result;
}

void test_manifast_printfmt() {
    Any fmt;
    fmt.type = ANY_STRING;
    fmt.ptr = (void*)"%d";

    Any val;
    val.type = ANY_NUMBER;
    val.number = 42;

    std::string out = captureStdout(manifast_printfmt, &fmt, &val);
    if (out != "42") {
        std::cerr << "Fail: Expected output 42 for number type, got: " << out << std::endl;
        exit(1);
    }

    val.type = ANY_STRING;
    val.ptr = (void*)"hello";
    fmt.ptr = (void*)"%s";
    out = captureStdout(manifast_printfmt, &fmt, &val);
    if (out != "hello") {
        std::cerr << "Fail: Expected output hello for string type, got: " << out << std::endl;
        exit(1);
    }

    val.type = ANY_BOOLEAN;
    val.number = 1;
    out = captureStdout(manifast_printfmt, &fmt, &val);
    if (out != "benar") {
        std::cerr << "Fail: Expected output benar for boolean true, got: " << out << std::endl;
        exit(1);
    }

    val.type = ANY_NIL;
    out = captureStdout(manifast_printfmt, &fmt, &val);
    if (out != "nil") {
        std::cerr << "Fail: Expected output nil for nil type, got: " << out << std::endl;
        exit(1);
    }

    std::cout << "test_manifast_printfmt passed!" << std::endl;
}

int main() {
    test_manifast_printfmt();
    std::cout << "All C++ Runtime tests passed!" << std::endl;
    return 0;
}
