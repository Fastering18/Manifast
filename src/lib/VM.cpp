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
                else if (vb.type == 3 && vc.type == 3) res = std::string((char*)vb.ptr) == std::string((char*)vc.ptr);
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
                bool val = (R(a).type != 3 || R(a).ptr != nullptr); // Basic truthiness
                if (R(a).type == 0) val = R(a).number != 0;
                if (val != (c != 0)) frame->ip++;
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
                if (key.type == 3 && key.ptr) {
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
                    // Arguments follow the function in the stack.
                    // R(a) is the function, R(a+1) is Arg 0.
                    // We want Arg 0 to be R(0) in the new frame.
                    newFrame.slots = &R(a + 1); 
                    
                    frames.push_back(newFrame);
                    frame = &frames.back(); // Update local frame pointer
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
                
                frames.pop_back();
                if (frames.empty()) return; // Exit main interpret
                
                frame = &frames.back();
                // Find where the function was on the previous frame and put result there
                // The previous frame's 'slots' at the call site was where R(a) was.
                // Wait, in my CALL implementation: newFrame.slots = &R(a).
                // So the result just needs to stay at R(0) of the popped frame,
                // which IS R(a) of the current frame.
                R(getA(*(frame->ip - 1))) = result; 
                break;
            }
            default:
                RUNTIME_ERROR("Unknown opcode");
        }
    }
}

} // namespace vm
} // namespace manifast
