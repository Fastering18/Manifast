#include "manifast/VM/VM.h"
#include <iostream>
#include <cmath>

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

VM::VM() {
    stack.resize(1024); // Initial stack size
    resetStack();
}

VM::~VM() {
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
                // Fast path float
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
            case OpCode::RETURN: {
                // Done
                return;
            }
            case OpCode::GETGLOBAL: {
                // TODO
                break;
            }
            default:
                RUNTIME_ERROR("Unknown opcode");
        }
    }
}

} // namespace vm
} // namespace manifast
