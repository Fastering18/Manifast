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
    frame.returnReg = 0;
    
    frames.push_back(frame);
    
    run();
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
            case OpCode::LOADBOOL: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                R(a) = {2, (double)b, nullptr};
                if (c) frame->ip++;
                break;
            }
            case OpCode::LOADNIL: {
                int a = GET_A(i);
                int b = GET_B(i);
                for (int j = 0; j <= b; j++) R(a + j) = {3, 0.0, nullptr};
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
            case OpCode::MOD: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                if (vb.type == 0 && vc.type == 0) {
                    R(a) = {0, (double)((long long)vb.number % (long long)vc.number), nullptr};
                }
                break;
            }
            case OpCode::POW: {
                 // Simplified
                 break;
            }
            case OpCode::NOT: {
                int a = GET_A(i);
                int b = GET_B(i);
                bool val = (R(b).type == 3 && R(b).ptr == nullptr) || (R(b).type == 0 && R(b).number == 0);
                R(a) = {2, (double)val, nullptr};
                break;
            }
            case OpCode::LT: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                bool res = false;
                if (vb.type == 0 && vc.type == 0) res = vb.number < vc.number;
                if (res != (a != 0)) frame->ip++;
                break;
            }
            case OpCode::LE: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                bool res = false;
                if (vb.type == 0 && vc.type == 0) res = vb.number <= vc.number;
                if (res != (a != 0)) frame->ip++;
                break;
            }
            case OpCode::EQ: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                Any vb = RK(b);
                Any vc = RK(c);
                bool res = false;
                if (vb.type == 0 && vc.type == 0) res = vb.number == vc.number;
                else if (vb.type == 1 && vc.type == 1) res = std::string((char*)vb.ptr) == std::string((char*)vc.ptr);
                else if (vb.type == 3 && vc.type == 3) res = true; // nil == nil
                if (res != (a != 0)) frame->ip++;
                break;
            }
            case OpCode::JMP: {
                frame->ip += GET_sBx(i);
                break;
            }
            case OpCode::TEST: {
                int a = GET_A(i);
                int c = GET_C(i);
                bool val = true;
                if (R(a).type == 3) val = false; // Nil
                else if (R(a).type == 2) val = (R(a).number != 0); // Bool
                else if (R(a).type == 1) val = true; // Non-empty string (simplified)
                else if (R(a).type == 0) val = (R(a).number != 0); // Num
                if (val != (c != 0)) frame->ip++;
                break;
            }
            case OpCode::TESTSET: {
                int a = GET_A(i);
                int b = GET_B(i);
                int c = GET_C(i);
                bool val = true;
                if (R(b).type == 3) val = false;
                else if (R(b).type == 2) val = (R(b).number != 0);
                else if (R(b).type == 1) val = true;
                else if (R(b).type == 0) val = (R(b).number != 0);
                if (val == (c != 0)) R(a) = R(b); else frame->ip++;
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
                if (key.type == 1 && key.ptr) {
                    std::string name((uintptr_t)key.ptr < 1000 ? "INVALID" : (char*)key.ptr);
                    if (globals.count(name)) {
                        R(a) = globals[name];
                    } else {
                        R(a) = {3, 0.0, nullptr}; // Nil
                    }
                }
                break;
            }
            case OpCode::SETGLOBAL: {
                int a = GET_A(i);
                int bx = GET_Bx(i);
                Any key = K(bx);
                if (key.type == 1 && key.ptr) {
                    std::string name((char*)key.ptr);
                    globals[name] = R(a);
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
                    // For now assume 0 results or 1 result moved to R(a)
                } 
                else if (func.type == 5) { // Bytecode
                    int funcIdx = (int)func.number;
                    Chunk* subChunk = frame->chunk->functions[funcIdx].get();
                    
                    // Push new frame
                    CallFrame newFrame;
                    newFrame.chunk = subChunk;
                    newFrame.ip = subChunk->code.data();
                    newFrame.slots = &R(a + 1); 
                    newFrame.returnReg = a; // Store result back in R(a)
                    
                    frames.push_back(newFrame);
                    frame = &frames.back(); 
                }
                else {
                    RUNTIME_ERROR("Call to non-function");
                }
                break;
            }
            case OpCode::RETURN: {
                int a = GET_A(i);
                int b = GET_B(i); // numResults + 1
                
                Any result = (b > 1) ? R(a) : Any{3, 0.0, nullptr}; // Default nil
                
                int retReg = frame->returnReg;
                frames.pop_back();
                if (frames.empty()) return; // Exit main interpret
                
                frame = &frames.back();
                frame->slots[retReg] = result;
                break;
            }
            case OpCode::COUNT: break;
            default: {
                char buf[64];
                sprintf(buf, "Unknown opcode: %d", (int)GET_OP(i));
                RUNTIME_ERROR(buf);
            }
        }
    }
}

} // namespace vm
} // namespace manifast
