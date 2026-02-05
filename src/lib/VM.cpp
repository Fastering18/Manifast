#include "manifast/VM/VM.h"
#define DEBUG_VM
#include "manifast/Runtime.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include "manifast/VM/Compiler.h"

namespace manifast {
namespace vm {

#define RUNTIME_ERROR(msg) \
    do { runtimeError(msg); return; } while(0)

// Helper macros for instruction decoding
#define GET_OP(i)   getOpCode(i)
#define GET_A(i)    getA(i)
#define GET_B(i)    getB(i)
#define GET_C(i)    getC(i)
#define GET_Bx(i)   getBx(i)
#define GET_sBx(i)  getsBx(i)

// Access registers (relative to current frame)
#define R(x)        (stack[frames.back().baseSlot + (x)])
// Access constants
#define K(x)        (frames.back().chunk->constants[x])
// Access Register or Constant (Bit 9 of B/C determines this)
// If index >= 256, it's a constant at index-256
#define RK(x)       ((x) < 256 ? R(x) : K((x) - 256))

// Native Print
static void nativePrint(VM* vm, Any* args, int nargs) {
    for (int i = 0; i < nargs; i++) {
        if (i > 0) printf("\t");
        manifast_print_any(&args[i]);
    }
    fflush(stdout);
    args[-1] = {3, 0.0, nullptr}; // Return Nil
}

static void nativePrintln(VM* vm, Any* args, int nargs) {
    for (int i = 0; i < nargs; i++) {
        if (i > 0) printf("\t");
        manifast_print_any(&args[i]);
    }
    printf("\n");
    fflush(stdout);
    args[-1] = {3, 0.0, nullptr}; // Return Nil
}

static std::string anyToString(const Any& a) {
    if (a.type == 1 && a.ptr) return (char*)a.ptr;
    if (a.type == 0) {
        char buf[64];
        if (a.number == (long long)a.number) sprintf(buf, "%lld", (long long)a.number);
        else sprintf(buf, "%g", a.number);
        return buf;
    }
    if (a.type == 2) return a.number ? "true" : "false";
    if (a.type == 3) return "nil";
    if (a.type == 5) return "[Function]";
    if (a.type == 8) return "[Class]";
    if (a.type == 9) return "objek";
    return "unknown";
}

static void nativeTipe(VM* vm, Any* args, int nargs) {
    if (nargs < 1) return;
    const char* t = "unknown";
    switch (args[0].type) {
        case 0: t = "angka"; break;
        case 1: t = "string"; break;
        case 2: t = "bool"; break;
        case 3: t = "nil"; break;
        case 4: t = "native"; break;
        case 5: t = "fungsi"; break;
        case 6: t = "array"; break;
        case 7: t = "objek"; break;
        case 8: t = "objek"; break; // Class is also an object
        case 9: t = "objek"; break; // Instance is an object
    }
    Any res;
    res.type = 1;
    res.ptr = (void*)mf_strdup(t); 
    args[-1] = res;
}

#include <chrono>
#include <thread>

static void nativeTunggu(VM* vm, Any* args, int nargs) {
    if (nargs < 1 || args[0].type != 0) return;
    double sec = args[0].number;
    std::this_thread::sleep_for(std::chrono::duration<double>(sec));
}

static void nativeInput(VM* vm, Any* args, int nargs) {
    if (nargs >= 1 && args[0].type == 1) {
        printf("%s", (char*)args[0].ptr);
    }
    char buffer[1024];
    if (fgets(buffer, 1024, stdin)) {
        // Strip newline
        buffer[strcspn(buffer, "\n")] = 0;
        Any res = {1, 0.0, (void*)mf_strdup(buffer)};
        args[-1] = res;
    } else {
        Any res = {1, 0.0, (void*)mf_strdup("")};
        args[-1] = res;
    }
}

static void nativeAssert(VM* vm, Any* args, int nargs) {
    if (nargs < 1) {
        vm->runtimeError("assert() membutuhkan minimal 1 argumen");
    }
    
    // Explicitly return nil to prevent register pollution
    args[-1] = {3, 0.0, nullptr};

    bool truth = false;
    if (args[0].type == 2) truth = (args[0].number != 0);
    else if (args[0].type == 3) truth = false;
    else if (args[0].type == 0) truth = (args[0].number != 0);
    else truth = true; // Objects, strings are truthy

    if (!truth) {
        std::string msg = "Assertion Failed";
        if (nargs >= 2 && args[1].type == 1) {
            msg = (char*)args[1].ptr;
        }
        vm->runtimeError(msg);
    }
}

static void nativeExit(VM* vm, Any* args, int nargs) {
    int code = 0;
    if (nargs >= 1 && args[0].type == 0) {
        code = (int)args[0].number;
    }
    exit(code);
}

static void nativeImpor(VM* vm, Any* args, int nargs) {
    if (nargs < 1 || args[0].type != 1) return;
    std::string path = (char*)args[0].ptr;
    
    // Internal Modules
    if (path == "os") {
        Any* obj = manifast_create_object();
        auto waktuNano = [](VM* vm, Any* args, int nargs) {
            auto now = std::chrono::steady_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
            args[-1] = {0, (double)ns, nullptr};
        };
        auto exitFn = [](VM* vm, Any* args, int nargs) {
             int code = (nargs >= 1 && args[0].type == 0) ? (int)args[0].number : 0;
             exit(code);
        };
        auto clearOutput = [](VM* vm, Any* args, int nargs) {
            printf("\033[2J\033[H");
            fflush(stdout);
            args[-1] = {3, 0.0, nullptr};
        };
        Any fn1 = {4, 0.0, (void*)+waktuNano};
        Any fn2 = {4, 0.0, (void*)+exitFn};
        Any fn3 = {4, 0.0, (void*)+clearOutput};
        manifast_object_set(obj, "waktuNano", &fn1);
        manifast_object_set(obj, "keluar", &fn2);
        manifast_object_set(obj, "clearOutput", &fn3);
        args[-1] = *obj;
        return;
    } else if (path == "string") {
        Any* obj = manifast_create_object();
        auto split = [](VM* vm, Any* args, int nargs) {
            args[-1] = {3, 0.0, nullptr}; // Default
            
            // Handle self-injection from method call
            int argIdx = 0;
            if (nargs >= 1 && args[argIdx].type != 1) argIdx++;
            
            int remaining = nargs - argIdx;

            if (remaining < 2 || args[argIdx].type != 1 || args[argIdx+1].type != 1 || !args[argIdx].ptr || !args[argIdx+1].ptr) {
                 Any* arr = manifast_create_array(0);
                 args[-1] = *arr;
                 return;
            }
            std::string str = (char*)args[argIdx].ptr;
            std::string delim = (char*)args[argIdx+1].ptr;
            if (delim.empty()) {
                Any* arr = manifast_create_array(1);
                Any val = {1, 0.0, (void*)mf_strdup(str.c_str())};
                manifast_array_set(arr, 1, &val);
                args[-1] = *arr;
                return;
            }
            std::vector<std::string> parts;
            size_t start = 0, end;
            while ((end = str.find(delim, start)) != std::string::npos) {
                parts.push_back(str.substr(start, end - start));
                start = end + delim.length();
            }
            parts.push_back(str.substr(start));
            Any* arr = manifast_create_array((int)parts.size());
            for (int i = 0; i < (int)parts.size(); i++) {
                Any val = {1, 0.0, (void*)mf_strdup(parts[i].c_str())};
                manifast_array_set(arr, i + 1, &val);
            }
            args[-1] = *arr;
        };
        auto substring = [](VM* vm, Any* args, int nargs) {
            args[-1] = {3, 0.0, nullptr}; // Default
            
            int argIdx = 0;
            if (nargs >= 1 && args[argIdx].type != 1) argIdx++;
            
            int remaining = nargs - argIdx;
            
            if (remaining < 3 || args[argIdx].type != 1) return;
            if (!args[argIdx].ptr) return;
            std::string str = (char*)args[argIdx].ptr;
            int start = (int)args[argIdx+1].number;
            int len = (int)args[argIdx+2].number;
            if (start < 1) start = 1; // Standardize to 1-based start
            if (start > (int)str.length() || len <= 0) { 
                args[-1] = {1, 0.0, (void*)mf_strdup("")}; 
                return; 
            }
            if (start + len - 1 > (int)str.length()) len = (int)str.length() - start + 1;
            std::string res = str.substr(start - 1, len);
            args[-1] = {1, 0.0, (void*)mf_strdup(res.c_str())};
        };
        Any fn1 = {4, 0.0, (void*)+split};
        Any fn2 = {4, 0.0, (void*)+substring};
        manifast_object_set(obj, "split", &fn1);
        manifast_object_set(obj, "substring", &fn2);
        args[-1] = *obj;
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    SyntaxConfig config;
    Lexer lexer(source, config);
    Parser parser(lexer);
    auto stmts = parser.parse();
    
    Chunk* chunk = new Chunk();
    Compiler compiler;
    if (compiler.compile(stmts, *chunk, path)) {
        vm->managedChunks.push_back(chunk);
        vm->interpret(chunk, source);
        // lastResult is updated by interpret's final RETURN
        args[-1] = vm->getLastResult();
    } else {
        delete chunk;
    }
}

VM::VM() : lastResult({3, 0.0, nullptr}) {
    resetStack();
    
    // Define builtins
    defineNative("print", nativePrint);
    defineNative("println", nativePrintln);
    defineNative("tipe", nativeTipe);
    defineNative("tunggu", nativeTunggu);
    defineNative("input", nativeInput);
    defineNative("impor", nativeImpor);
    defineNative("assert", nativeAssert);
    defineNative("exit", nativeExit);
}

VM::~VM() {
    for (auto c : managedChunks) {
        delete c;
    }
}

void VM::defineNative(const std::string& name, NativeFn fn) {
    Any func;
    func.type = 4; // Native Function
    func.ptr = (void*)fn;
    globals[name] = func;
}

void VM::resetStack() {
    stack.clear();
    stack.resize(4096, {3, 0.0, nullptr});
    frames.clear();
}

void VM::interpret(Chunk* chunk, std::string_view src) {
    if (!chunk || chunk->code.empty()) return;
    
    std::string oldSource = this->source;
    this->source = std::string(src);

    // Use current frames size to find fresh base
    int nextBase = 0;
    if (!frames.empty()) {
        nextBase = frames.back().baseSlot + 256; 
        if (nextBase + 256 >= (int)stack.size()) {
            RUNTIME_ERROR("Batas rekursi tercapai (Interpret)");
            return;
        }
    } else {
        resetStack();
    }
    
    CallFrame frame;
    frame.chunk = chunk;
    frame.pc = 0;
    frame.baseSlot = nextBase;
    frame.returnReg = 0;
    
    frames.push_back(frame);
    run((int)frames.size() - 1);
    
    this->source = oldSource;
}

void VM::runtimeError(const std::string& message) {
    if (frames.empty()) {
        fprintf(stderr, "\n[ERROR RUNTIME] %s\n", message.c_str());
        return;
    }
    
    CallFrame& frame = frames.back();
    int pc = frame.pc; // pc is now kept in sync via frame->pc = pc in run loop
    if (pc < 0) pc = 0;
    
    int line = (pc < (int)frame.chunk->lines.size()) ? frame.chunk->lines[pc] : -1;
    int offset = (pc < (int)frame.chunk->offsets.size()) ? frame.chunk->offsets[pc] : -1;

    fprintf(stderr, "\n[ERROR RUNTIME] Baris %d\n", line);
    
    // Print source context with caret if available
    if (offset != -1 && !source.empty()) {
        // Find start of line in source
        int start = offset;
        while (start > 0 && source[start-1] != '\n') start--;
        
        // Find end of line
        int end = offset;
        while (end < (int)source.length() && source[end] != '\n') end++;
        
        std::string lineStr = std::string(source.substr(start, end - start));
        fprintf(stderr, "  %s\n", lineStr.c_str());
        
        // Caret
        std::string caret = "  ";
        int col = offset - start;
        for (int j = 0; j < col; j++) {
            if (lineStr[j] == '\t') caret += '\t';
            else caret += ' ';
        }
        caret += '^';
        
        fprintf(stderr, "%s\n", caret.c_str());
    }
    
    fprintf(stderr, "-> %s\n", message.c_str());
    
    // Debug: Print OpCode that failed
    if (pc < (int)frame.chunk->code.size()) {
        Instruction i = frame.chunk->code[pc];
        OpCode op = (OpCode)(i & 0x3F);
        fprintf(stderr, "OpCode Gagal: %d (A=%d, B=%d, C=%d)\n", (int)op, (i>>6)&0xFF, (i>>14)&0x1FF, (i>>23)&0x1FF);
    
    int base = frame.baseSlot;
    fprintf(stderr, "\nRegister Dump (base=%d):\n", base);
    for (int j = 0; j < 16; j++) {
        Any v = stack[base + j];
        fprintf(stderr, "  R(%d): tipe=%d, val=%g", j, v.type, v.number);
        if (v.type == 1 && v.ptr) fprintf(stderr, " s=\"%s\"", (char*)v.ptr);
        fprintf(stderr, "\n");
    }
    }

    // Vertical Stack Trace
    fprintf(stderr, "\nJejak tumpukan (Stack Trace):\n");
    for (int i = (int)frames.size() - 1; i >= 0; i--) {
        CallFrame& f = frames[i];
        int fpc = f.pc - 1;
        if (fpc < 0) fpc = 0;
        int fline = (fpc < (int)f.chunk->lines.size()) ? f.chunk->lines[fpc] : -1;
        const char* name = f.chunk->name.empty() ? "<anonim>" : f.chunk->name.c_str();
        fprintf(stderr, "  pada %s (baris %d)\n", name, fline);
    }
    fprintf(stderr, "\n");
    
    // Reset VM state
    resetStack();
}

void VM::run(int entryFrameDepth) {
    // Current frame state cached in locals for speed
    CallFrame* frame = &frames.back();
    int pc = frame->pc;
    int base = frame->baseSlot;
    Instruction* code = frame->chunk->code.data();
    Any* stack_data = stack.data();

    // Helper to refresh state after frames change or stack reallocates
    auto sync = [&]() {
        frame = &frames.back();
        pc = frame->pc;
        base = frame->baseSlot;
        code = frame->chunk->code.data();
        stack_data = stack.data();
    };

    // Macro for local-cached register access
    #define LR(x) (stack_data[base + (x)])
    #define LK(x) (frame->chunk->constants[x])
    #define LRK(x) ((x) < 256 ? LR(x) : ((x)-256 >= 0 && (x)-256 < (int)frame->chunk->constants.size() ? LK((x) - 256) : Any{3, 0.0, nullptr}))

    int instructions = 0;
    for (;;) {
        if (instructions++ > 1000000) {
            fprintf(stderr, "Error: Execution limit reached (1M instructions)\n");
            return;
        }
        
        // Sync PC with frame for error reporting and re-entrancy
        frame->pc = pc;

        Instruction i = code[pc++];
        
        if (debugMode) {
            fprintf(stderr, "[TRACE] %d: Op=%-10d A=%d B=%d C=%d\n", pc-1, (int)GET_OP(i), (int)GET_A(i), (int)GET_B(i), (int)GET_C(i));
        }
        
        switch (GET_OP(i)) {
            case OpCode::MOVE: {
                LR(GET_A(i)) = LR(GET_B(i));
                break;
            }
            case OpCode::LOADK: {
                LR(GET_A(i)) = LK(GET_Bx(i));
                break;
            }
            case OpCode::LOADBOOL: {
                LR(GET_A(i)) = {2, (double)GET_B(i), nullptr};
                if (GET_C(i)) pc++;
                break;
            }
            case OpCode::LOADNIL: {
                int a = GET_A(i);
                int b = GET_B(i);
                for (int j = 0; j <= b; j++) LR(a + j) = {3, 0.0, nullptr};
                break;
            }
            case OpCode::ADD: 
            case OpCode::SUB: 
            case OpCode::MUL: 
            case OpCode::DIV:
            case OpCode::MOD: {
                Any vb = LRK(GET_B(i));
                Any vc = LRK(GET_C(i));
                OpCode op = GET_OP(i);
                if (vb.type == 0 && vc.type == 0) {
                    double res = 0;
                    if (op == OpCode::ADD) res = vb.number + vc.number;
                    else if (op == OpCode::SUB) res = vb.number - vc.number;
                    else if (op == OpCode::MUL) res = vb.number * vc.number;
                    else if (op == OpCode::DIV) res = vb.number / vc.number;
                    else if (op == OpCode::MOD) res = fmod(vb.number, vc.number);
                    LR(GET_A(i)) = {0, res, nullptr};
                } else if (op == OpCode::ADD && (vb.type == 1 || vc.type == 1)) {
                    // String concatenation
                    auto anyToString = [](Any& v) -> std::string {
                        if (v.type == 1 && v.ptr) return (char*)v.ptr;
                        if (v.type == 0) {
                             if (v.number == (long long)v.number) return std::to_string((long long)v.number);
                             else return std::to_string(v.number);
                        }
                        if (v.type == 2) return v.number ? "true" : "false"; // "benar"/"salah"? keep internal English for now
                        if (v.type == 3) return "nil";
                        if (v.type == 4) return "[Native]";
                        if (v.type == 5) return "[Function]";
                        if (v.type == 6) return "[Array]";
                        if (v.type == 7) return "{Object}";
                        return "";
                    };

                    std::string s1 = anyToString(vb);
                    std::string s2 = anyToString(vc);
                    
                    std::string res = s1 + s2;
                    LR(GET_A(i)) = {1, 0.0, (void*)mf_strdup(res.c_str())};
                } else if (vb.type == 9 || vc.type == 9) { // Instance Metamethod
                    const char* mm = nullptr;
                    if (op == OpCode::ADD) mm = "__jumlah";
                    else if (op == OpCode::SUB) mm = "__kurang";
                    else if (op == OpCode::MUL) mm = "__kali";
                    else if (op == OpCode::DIV) mm = "__bagi";
                    
                    Any& inst = (vb.type == 9) ? vb : vc;
                    ManifastInstance* mfi = (ManifastInstance*)inst.ptr;
                    Any* func = manifast_object_get_raw(mfi->klass->methods, mm);
                    if (func && func->type == 5) {
                        int nextBase = base + GET_A(i) + 1;
                        if (nextBase + 255 >= (int)stack.size()) RUNTIME_ERROR("Stack Overflow");
                        
                        frames.back().pc = pc; // Save current PC
                        
                        // Args: self, other
                        LR(GET_A(i) + 1) = vb;
                        LR(GET_A(i) + 2) = vc;
                        
                        CallFrame frame;
                        frame.chunk = (Chunk*)func->ptr;
                        frame.pc = 0;
                        frame.baseSlot = base + GET_A(i) + 1;
                        frame.returnReg = GET_A(i);
                        frames.push_back(frame);
                        sync();
                    }
                }
                break;
            }
            case OpCode::POW: break;
            case OpCode::NOT: {
                Any v = LR(GET_B(i));
                bool isTruthy = true;
                if (v.type == 3) isTruthy = false; // nil
                else if (v.type == 2) isTruthy = (v.number != 0); // bool
                else if (v.type == 0) isTruthy = (v.number != 0); // number
                LR(GET_A(i)) = {2, isTruthy ? 0.0 : 1.0, nullptr};
                break;
            }
            case OpCode::LT: {
                Any vb = LRK(GET_B(i));
                Any vc = LRK(GET_C(i));
                bool res = (vb.type == 0 && vc.type == 0) ? (vb.number < vc.number) : false;
                if (res != (GET_A(i) != 0)) pc++;
                break;
            }
            case OpCode::LE: {
                Any vb = LRK(GET_B(i));
                Any vc = LRK(GET_C(i));
                bool res = (vb.type == 0 && vc.type == 0) ? (vb.number <= vc.number) : false;
                if (res != (GET_A(i) != 0)) pc++;
                break;
            }
            case OpCode::EQ: {
                Any vb = LRK(GET_B(i));
                Any vc = LRK(GET_C(i));
                bool res = false;
                if (vb.type == 0 && vc.type == 0) res = (vb.number == vc.number);
                else if (vb.type == 1 && vc.type == 1) res = (std::strcmp((char*)vb.ptr, (char*)vc.ptr) == 0);
                else if (vb.type == 3 && vc.type == 3) res = true;
                if (res != (GET_A(i) != 0)) pc++;
                break;
            }
            case OpCode::JMP: {
                pc += GET_sBx(i);
                break;
            }
            case OpCode::TEST: {
                Any v = LR(GET_A(i));
                bool val = true;
                if (v.type == 3) val = false;
                else if (v.type == 2) val = (v.number != 0);
                else if (v.type == 0) val = (v.number != 0);
                if (val != (GET_C(i) != 0)) pc++;
                break;
            }
            case OpCode::TESTSET: {
                Any v = LR(GET_B(i));
                bool val = true;
                if (v.type == 3) val = false;
                else if (v.type == 2) val = (v.number != 0);
                else if (v.type == 0) val = (v.number != 0);
                if (val == (GET_C(i) != 0)) LR(GET_A(i)) = v; else pc++;
                break;
            }
            case OpCode::GETGLOBAL: {
                Any key = LK(GET_Bx(i)); 
                if (key.type == 1 && key.ptr) {
                    std::string name((char*)key.ptr);
                    auto it = globals.find(name);
                    if (it != globals.end()) {
                        LR(GET_A(i)) = it->second;
                    } else {
                        #ifdef DEBUG_VM
                        fprintf(stderr, "[DEBUG] Global tidak ditemukan: '%s'\n", name.c_str());
                        #endif
                        LR(GET_A(i)) = {3, 0.0, nullptr};
                    }
                }
                break;
            }
            case OpCode::SETGLOBAL: {
                Any key = LK(GET_Bx(i));
                if (key.type == 1 && key.ptr) globals[(char*)key.ptr] = LR(GET_A(i));
                break;
            }
            case OpCode::CALL: {
                int a = GET_A(i);
                int nparams = GET_B(i) - 1; 
                int nresults = GET_C(i) - 1;
                
                Any callee = LR(a);
                if (callee.type == 4) { // Native
                    frames.back().pc = pc;
                    NativeFn fn = (NativeFn)callee.ptr;
                    fn(this, &LR(a + 1), nparams);
                    sync();
                } else if (callee.type == 5) { // Bytecode
                    int nextBase = base + a + 1;
                    if (nextBase + 255 >= (int)stack.size()) RUNTIME_ERROR("Tumpukan Meluap (Stack Overflow)");
                    
                    frames.back().pc = pc;
                    Chunk* chunk = (Chunk*)callee.ptr;
                    CallFrame frame;
                    frame.chunk = chunk;
                    frame.pc = 0;
                    frame.baseSlot = base + a + 1;
                    frame.returnReg = a;
                    frames.push_back(frame);
                    sync();
                } else if (callee.type == 8) { // Class (Constructor)
                    frames.back().pc = pc;
                    Any inst = *manifast_create_instance(&callee);
                    ManifastClass* klass = (ManifastClass*)callee.ptr;
                    Any* inisiasi = manifast_object_get_raw(klass->methods, "inisiasi");
                    
                    if (inisiasi && inisiasi->type == 5) {
                        int nextBase = base + a;
                        if (nextBase + 255 >= (int)stack.size()) RUNTIME_ERROR("Tumpukan Meluap (Stack Overflow)");
                        
                        // Copy instance to callee slot but keep original for frame restoration?
                        // Usually self is at nextBase
                        LR(a) = inst; 
                        
                        frames.back().pc = pc;
                        Chunk* chunk = (Chunk*)inisiasi->ptr;
                        CallFrame frame;
                        frame.chunk = chunk;
                        frame.pc = 0;
                        frame.baseSlot = base + a;
                        frame.returnReg = -1; // Do not overwrite instance with inisiasi return
                        frames.push_back(frame);
                        sync();
                    } else {
                        LR(a) = inst;
                    }
                } else {
                     char buf[128];
                     sprintf(buf, "Panggilan ke non-fungsi (tipe %d)", callee.type);
                     RUNTIME_ERROR(buf);
                }
                break;
            }
            case OpCode::RETURN: {
                int a = GET_A(i);
                int n = GET_B(i) - 1;
                Any result = (n > 0) ? LR(a) : Any{3, 0.0, nullptr};
                
                if (frames.empty()) return; // Should not happen in well-formed code
                int retReg = frames.back().returnReg;
                frames.pop_back();
                
                // If we dropped to the depth where run() started, we are done
                if ((int)frames.size() == entryFrameDepth) {
                    lastResult = result; 
                    return; 
                }
                
                sync();
                // If returning from internal nativeImpor frame, stack_data might be reset? No sync fixes it.
                if (retReg >= 0) {
                    LR(retReg) = result;
                }
                break;
            }
            case OpCode::GETTABLE: {
                Any obj = LR(GET_B(i));
                Any key = LRK(GET_C(i));
                if (obj.type == 3) { // Nil
                     RUNTIME_ERROR("Mencoba mengakses properti pada 'nil'");
                }
                if (obj.type == 7) { // Object
                    LR(GET_A(i)) = *manifast_object_get(&obj, (char*)key.ptr);
                } else if (obj.type == 9) { // Instance
                    // 1. Look in instance fields
                    ManifastInstance* inst = (ManifastInstance*)obj.ptr;
                    Any* val = manifast_object_get_raw(inst->fields, (char*)key.ptr);
                    if (val && val->type != 3) {
                         LR(GET_A(i)) = *val;
                    } else {
                        // 2. Look in class methods
                        LR(GET_A(i)) = *manifast_object_get_raw(inst->klass->methods, (char*)key.ptr);
                    }
                } else if (obj.type == 8) { // Class
                    ManifastClass* klass = (ManifastClass*)obj.ptr;
                    LR(GET_A(i)) = *manifast_object_get_raw(klass->methods, (char*)key.ptr);
                } else if (obj.type == 6) { // Array
                    int idx = (int)key.number;
                    if (idx == 0) RUNTIME_ERROR("Indeks array harus dimulai dari 1 (Manifast menggunakan 1-based indexing)");
                    LR(GET_A(i)) = *manifast_array_get(&obj, key.number);
                } else if (obj.type == 1) { // String
                     char* s = (char*)obj.ptr;
                     int idx = (int)key.number;
                     if (idx == 0) RUNTIME_ERROR("Indeks string harus dimulai dari 1 (Manifast menggunakan 1-based indexing)");
                     if (idx < 1) RUNTIME_ERROR("Indeks string harus >= 1");
                     if (idx <= (int)strlen(s)) {
                         char buf[2] = {s[idx-1], '\0'};
                         LR(GET_A(i)) = *manifast_create_string(buf); 
                     } else {
                         LR(GET_A(i)) = {3, 0.0, nullptr}; // Nil
                     }
                } else {
                    RUNTIME_ERROR("Tipe tidak dapat di-index (bukan array/objek/string)");
                }
                break;
            }
            case OpCode::SETTABLE: {
                Any obj = LR(GET_A(i));
                Any key = LRK(GET_B(i));
                Any val = LRK(GET_C(i));
                if (obj.type == 7) {
                    manifast_object_set(&obj, (char*)key.ptr, &val);
                } else if (obj.type == 9) {
                    ManifastInstance* inst = (ManifastInstance*)obj.ptr;
                    manifast_object_set_raw(inst->fields, (char*)key.ptr, &val);
                } else if (obj.type == 8) {
                    ManifastClass* klass = (ManifastClass*)obj.ptr;
                    manifast_object_set_raw(klass->methods, (char*)key.ptr, &val);
                } else if (obj.type == 6) {
                    manifast_array_set(&obj, key.number, &val);
                }
                break;
            }
            case OpCode::NEWARRAY: {
                LR(GET_A(i)) = *manifast_create_array(GET_B(i));
                break;
            }
            case OpCode::NEWTABLE: {
                LR(GET_A(i)) = *manifast_create_object();
                break;
            }
            case OpCode::NEWCLASS: {
                 Any& name = LK(GET_Bx(i));
                 LR(GET_A(i)) = *manifast_create_class((char*)name.ptr);
                 break;
            }
            case OpCode::GETSLICE: {
                if (pc >= (int)frame->chunk->code.size()) RUNTIME_ERROR("Truncated chunk (GETSLICE)");
                Any obj = LR(GET_B(i));
                Any start = LRK(GET_C(i));
                // End is in next word
                Instruction i2 = code[pc++];
                Any end = LRK(i2);
                
                if (obj.type == 6) { // Array slicing
                    ManifastArray* src = (ManifastArray*)obj.ptr;
                    int s = (start.type == 3) ? 1 : (int)start.number;
                    int e = (end.type == 3) ? (int)src->size : (int)end.number;
                    
                    if (s < 1) s = 1;
                    if (e > (int)src->size) e = src->size;
                    
                    int len = (e >= s) ? (e - s + 1) : 0;
                    Any* res = manifast_create_array(len);
                    ManifastArray* dst = (ManifastArray*)res->ptr;
                    for (int j = 0; j < len; j++) {
                        dst->elements[j] = src->elements[s + j - 1];
                    }
                    LR(GET_A(i)) = *res;
                } else {
                    LR(GET_A(i)) = {3, 0.0, nullptr};
                }
                break;
            }
            case OpCode::SETLIST: {
                int a = GET_A(i);
                int n = GET_B(i); // num of elements to set
                int c = GET_C(i); // batch index
                Any& arr = LR(a);
                for (int j = 1; j <= n; j++) {
                    manifast_array_set(&arr, (double)((c-1)*50 + j), &LR(a + j));
                }
                break;
            }
            case OpCode::COUNT: break;
            default: RUNTIME_ERROR("Unknown opcode");
        }
    }
}

} // namespace vm
} // namespace manifast
