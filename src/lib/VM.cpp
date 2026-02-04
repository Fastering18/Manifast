#include "manifast/VM/VM.h"
#include "manifast/Runtime.h"
#include <iostream>
#include <cmath>
#include <cstring>

namespace manifast {
namespace vm {

#define RUNTIME_ERROR(msg) \
    do { std::cerr << "Runtime Error: " << msg << "\n"; return; } while(0)

// Helper macros for instruction decoding
#define GET_OP(i)   getOpCode(i)
#define GET_A(i)    getA(i)
#define GET_B(i)    getB(i)
#define GET_C(i)    getC(i)
#define GET_Bx(i)   getBx(i)
#define GET_sBx(i)  getsBx(i)

// Access registers (relative to current frame)
#define R(x)        (frame->slots[x])
// Access constants
#define K(x)        (frame->chunk->constants[x])
// Access Register or Constant (Bit 9 of B/C determines this)
// If index >= 256, it's a constant at index-256
#define RK(x)       ((x) < 256 ? R(x) : K((x) - 256))

// Native Print
static void nativePrint(VM* vm, Any* args, int nargs) {
    for (int i = 0; i < nargs; i++) {
        manifast_print_any(&args[i]);
    }
}

static void nativePrintln(VM* vm, Any* args, int nargs) {
    for (int i = 0; i < nargs; i++) {
        manifast_print_any(&args[i]);
    }
    std::cout << "\n";
}

VM::VM() {
    stack.resize(1024); // Initial stack size
    resetStack();
    
    // Define builtins
    defineNative("print", nativePrint);
    defineNative("println", nativePrintln);
}

VM::~VM() {
}

void VM::defineNative(const std::string& name, NativeFn fn) {
    Any func;
    func.type = 4; // Native Function
    func.ptr = (void*)fn;
    globals[name] = func;
}

void VM::resetStack() {
    stackTop = stack.data();
    frames.clear();
}

void VM::interpret(Chunk* chunk) {
    if (!chunk || chunk->code.empty()) return;

    resetStack();
    
    // Set up main frame
    CallFrame frame;
    frame.chunk = chunk;
    frame.ip = chunk->code.data();
    frame.slots = stack.data();
    
    frames.push_back(frame);
    
    try {
        run();
    } catch (...) {
        std::cerr << "VM Panic\n";
    }
}

void VM::run() {
    CallFrame* frame = &frames.back();
    
    for (;;) {
        Instruction i = *frame->ip++;
        
        switch (GET_OP(i)) {
            case OpCode::MOVE: {
                int a = GET_A(i);
                int b = GET_B(i);
                R(a) = R(b);
                break;
            }
            case OpCode::LOADK: {
                int a = GET_A(i);
                int bx = GET_Bx(i);
                R(a) = K(bx);
                break;
            }
            case OpCode::ADD: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                if (vb.type == 0 && vc.type == 0) {
                    R(a) = {0, vb.number + vc.number, nullptr};
                } else {
                     RUNTIME_ERROR("Arithmetic on non-number");
                }
                break;
            }
            case OpCode::SUB: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                if (vb.type == 0 && vc.type == 0) {
                    R(a) = {0, vb.number - vc.number, nullptr};
                }
                break;
            }
            case OpCode::MUL: {
                 int a = GET_A(i);
                 int b = GET_B(i);
                 int c = GET_C(i);
                 Any vb = RK(b);
                 Any vc = RK(c);
                 if (vb.type == 0 && vc.type == 0) {
                     R(a) = {0, vb.number * vc.number, nullptr};
                 }
                 break;
            }
             case OpCode::DIV: {
                 int a = GET_A(i);
                 int b = GET_B(i);
                 int c = GET_C(i);
                 Any vb = RK(b);
                 Any vc = RK(c);
                 if (vb.type == 0 && vc.type == 0) {
                     R(a) = {0, vb.number / vc.number, nullptr};
                 }
                 break;
            }
            case OpCode::GETGLOBAL: {
                int a = GET_A(i);
                int bx = GET_Bx(i);
                Any key = K(bx); // Constant index as string name?
                // Wait, logic in Compiler check. 
                // In Compiler (Step 4154):
                // emit(createABC(OpCode::LOADNIL, r, 0, 0));
                // Wait! Compiler didn't emit GETGLOBAL!
                // It emitted LOADNIL for unresolved vars.
                // I need to fix Compiler to emit GETGLOBAL if it's not a local.
                
                // Assuming I fix compiler next:
                if (key.type == 3 && key.ptr) {
                    std::string name((char*)key.ptr);
                    if (globals.count(name)) {
                        R(a) = globals[name];
                    } else {
                        R(a) = {3, 0.0, nullptr}; // Nil
                        // Or error? Lua returns nil.
                    }
                }
                break;
            }
            case OpCode::CALL: {
                int a = GET_A(i);
                int b = GET_B(i); // Args + 1
                int c = GET_C(i); // Results + 1
                
                // Func is at R(a)
                Any func = R(a);
                int nargs = b - 1;
                
                if (func.type == 4) { // Native
                    NativeFn fn = (NativeFn)func.ptr;
                    fn(this, &R(a + 1), nargs);
                    // Result handling? NativeFn returns void for now.
                    // Lua moves results.
                    // For now assume 0 results.
                } else {
                    RUNTIME_ERROR("Call to non-function");
                }
                break;
            }
            case OpCode::RETURN: {
                return;
            }
            default:
                RUNTIME_ERROR("Unknown opcode");
        }
    }
}

} // namespace vm
} // namespace manifast
