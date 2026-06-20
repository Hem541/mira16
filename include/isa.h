#ifndef ISA_H
#define ISA_H

#include <cstdint>

// 16-bit instruction format. opcode + operands, three layouts (R/I/J).
// encode + decode only. nothing here executes.

enum class Opcode : uint8_t {
    NOP   = 0,   // do nothing
    LOADI = 1,   // I: rd = imm                 (load immediate constant)
    LOAD  = 2,   // I: rd = mem[imm]            (read from memory)
    STORE = 3,   // I: mem[imm] = rd            (write to memory)
    MOV   = 4,   // R: rd = rs
    ADD   = 5,   // R: rd = rs + rt
    SUB   = 6,   // R: rd = rs - rt
    AND   = 7,   // R: rd = rs & rt
    OR    = 8,   // R: rd = rs | rt
    XOR   = 9,   // R: rd = rs ^ rt
    NOT   = 10,  // R: rd = ~rs
    SHL   = 11,  // R: rd = rs << rt
    SHR   = 12,  // R: rd = rs >> rt
    CMP   = 13,  // R: set FLAGS from (rs - rt), discard result
    JMP   = 14,  // J: PC = addr
    JZ    = 15,  // J: if zero flag set,    PC = addr
    JNZ   = 16,  // J: if zero flag clear,  PC = addr
    PUSH  = 17,  // I: mem[--SP] = rd
    POP   = 18,  // I: rd = mem[SP++]
    HALT  = 19,  // stop the fetch-decode-execute loop
    MUL   = 20,  // R: rd = rs * rt   (low 16 bits; carry set if overflow)
    DIV   = 21,  // R: rd = rs / rt   (rt==0 -> result 0, carry flag set)
    CALL  = 22,  // J: push return addr (PC), then PC = addr  (subroutine)
    RET   = 23,  // pop return addr into PC  (return from subroutine)
    OUT   = 24   // I: print the value in rd (to stdout) -- I/O instruction
};

// ---- Which format does each opcode use? Decode needs to know this. --
enum class Format : uint8_t { R, I, J, NONE };

inline Format formatOf(Opcode op) {
    switch (op) {
        case Opcode::MOV: case Opcode::ADD: case Opcode::SUB:
        case Opcode::AND: case Opcode::OR:  case Opcode::XOR:
        case Opcode::NOT: case Opcode::SHL: case Opcode::SHR:
        case Opcode::CMP: case Opcode::MUL: case Opcode::DIV:
            return Format::R;
        case Opcode::LOADI: case Opcode::LOAD: case Opcode::STORE:
        case Opcode::PUSH:  case Opcode::POP:  case Opcode::OUT:
            return Format::I;
        case Opcode::JMP: case Opcode::JZ: case Opcode::JNZ:
        case Opcode::CALL:
            return Format::J;
        default: // NOP, HALT, RET
            return Format::NONE;
    }
}

// ---- A decoded instruction: the unpacked form the CPU executes. -----
//  The raw 16-bit word is hard to work with. After decode() we hand the
//  CPU this friendly struct with named fields already extracted.
struct Instruction {
    Opcode  op;
    Format  fmt;
    uint8_t rd;    // destination register index (0..7)
    uint8_t rs;    // source register index
    uint8_t rt;    // second source register index
    uint8_t imm;   // 8-bit immediate (I-type)
    uint16_t addr; // 11-bit jump address (J-type)
    uint16_t raw;  // the original word, kept for tracing/printing
};

//  Bit-field positions and masks. Defined once here so encode() and
//  decode() can never disagree about where a field lives.
namespace bits {
    constexpr int OP_SHIFT = 11;          // opcode occupies bits 15..11
    constexpr uint16_t OP_MASK  = 0x1F;   // 5 bits
    constexpr uint16_t REG_MASK = 0x07;   // 3 bits (register index 0..7)
    constexpr uint16_t IMM_MASK = 0xFF;   // 8 bits
    constexpr uint16_t ADDR_MASK = 0x07FF;// 11 bits

    // R-type field shifts
    constexpr int RD_SHIFT = 8;   // bits 10..8
    constexpr int RS_SHIFT = 5;   // bits 7..5
    constexpr int RT_SHIFT = 2;   // bits 4..2
}

// ---- ENCODE: build a 16-bit word from its parts. --------------------
//  Used by the assembler to turn parsed text into machine code.
//  Each helper enforces its format's layout. Inputs are masked so a
//  caller passing a too-big value can't corrupt a neighbouring field.

inline uint16_t encodeR(Opcode op, uint8_t rd, uint8_t rs, uint8_t rt) {
    return  (static_cast<uint16_t>(op) & bits::OP_MASK) << bits::OP_SHIFT
          | (rd & bits::REG_MASK) << bits::RD_SHIFT
          | (rs & bits::REG_MASK) << bits::RS_SHIFT
          | (rt & bits::REG_MASK) << bits::RT_SHIFT;
}

inline uint16_t encodeI(Opcode op, uint8_t rd, uint8_t imm) {
    return  (static_cast<uint16_t>(op) & bits::OP_MASK) << bits::OP_SHIFT
          | (rd & bits::REG_MASK) << bits::RD_SHIFT
          | (imm & bits::IMM_MASK);
}

inline uint16_t encodeJ(Opcode op, uint16_t addr) {
    return  (static_cast<uint16_t>(op) & bits::OP_MASK) << bits::OP_SHIFT
          | (addr & bits::ADDR_MASK);
}

inline uint16_t encodeNone(Opcode op) {
    return (static_cast<uint16_t>(op) & bits::OP_MASK) << bits::OP_SHIFT;
}

// ---- DECODE: split a 16-bit word back into named fields. ------------
//  Used by the CPU every cycle. We extract EVERY field unconditionally
//  (cheap) and let the executor read only the ones its format uses.
inline Instruction decode(uint16_t word) {
    Instruction in{};
    in.raw = word;
    in.op  = static_cast<Opcode>((word >> bits::OP_SHIFT) & bits::OP_MASK);
    in.fmt = formatOf(in.op);
    in.rd  = (word >> bits::RD_SHIFT) & bits::REG_MASK;
    in.rs  = (word >> bits::RS_SHIFT) & bits::REG_MASK;
    in.rt  = (word >> bits::RT_SHIFT) & bits::REG_MASK;
    in.imm = word & bits::IMM_MASK;
    in.addr = word & bits::ADDR_MASK;
    return in;
}

#endif // ISA_H
