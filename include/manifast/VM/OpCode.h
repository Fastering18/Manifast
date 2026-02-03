#pragma once

#include <cstdint>

namespace manifast {
namespace vm {

using Instruction = uint32_t;

// Lua-style Register VM Opcodes
enum class OpCode : uint8_t {
    // Memory
    MOVE,       // R(A) := R(B)
    LOADK,      // R(A) := K(Bx)
    LOADBOOL,   // R(A) := (Bool)B; if (C) pc++
    LOADNIL,    // R(A)..R(A+B) := nil
    
    // Arithmetic (R(A) := R(B) op R(C)) or (R(A) := R(B) op K(C))?
    // Lua 5.1 uses RK(C) - bit 8 of C determines if it's Reg or Const
    ADD, SUB, MUL, DIV, MOD, POW,
    
    // Logic
    NOT,        // R(A) := not R(B)
    EQ,         // if ((RK(B) == RK(C)) ~= A) then pc++
    LT,         // if ((RK(B) <  RK(C)) ~= A) then pc++
    LE,         // if ((RK(B) <= RK(C)) ~= A) then pc++
    
    // Control
    JMP,        // pc += sBx
    TEST,       // if not (R(A) <=> C) then pc++
    TESTSET,    // if (R(B) <=> C) then R(A) := R(B) else pc++

    // Function
    CALL,       // R(A), ... := R(A)(R(A+1), ... , R(A+B-1))
    RETURN,     // return R(A), ... , R(A+B-1)
    
    // Globals (Optimization)
    GETGLOBAL,  // R(A) := Gbl[K(Bx)]
    SETGLOBAL,  // Gbl[K(Bx)] := R(A)

    COUNT
};

// Instruction Layout Helpers
// iABC: [ Op(6) | A(8) | B(9) | C(9) ]
// iABx: [ Op(6) | A(8) | Bx(18)    ]
// iAsBx:[ Op(6) | A(8) | sBx(18)   ]

enum class OpMode { iABC, iABx, iAsBx };

inline OpCode getOpCode(Instruction i) { return static_cast<OpCode>(i & 0x3F); }
inline uint8_t getA(Instruction i) { return (i >> 6) & 0xFF; }
inline uint16_t getB(Instruction i) { return (i >> 23) & 0x1FF; }
inline uint16_t getC(Instruction i) { return (i >> 14) & 0x1FF; }
inline uint32_t getBx(Instruction i) { return (i >> 14) & 0x3FFFF; }
inline int32_t getsBx(Instruction i) { return static_cast<int32_t>(getBx(i)) - 131071; }

// Encoders
inline Instruction createABC(OpCode op, int a, int b, int c) {
    return (static_cast<uint32_t>(op) & 0x3F) | 
           ((a & 0xFF) << 6) | 
           ((c & 0x1FF) << 14) | 
           ((b & 0x1FF) << 23);
}

inline Instruction createABx(OpCode op, int a, int bx) {
    return (static_cast<uint32_t>(op) & 0x3F) | 
           ((a & 0xFF) << 6) | 
           ((bx & 0x3FFFF) << 14);
}

inline Instruction createAsBx(OpCode op, int a, int sbx) {
    return createABx(op, a, sbx + 131071);
}

} // namespace vm
} // namespace manifast
