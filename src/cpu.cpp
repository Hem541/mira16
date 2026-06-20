#include "cpu.h"
#include <sstream>
#include <iomanip>

// scalar fetch / decode / execute.

CPU::CPU() : mem_(MEM_WORDS, 0) { reset(); }

void CPU::reset() {
    regs_.fill(0);
    std::fill(mem_.begin(), mem_.end(), 0);
    pc_      = 0;
    sp_      = static_cast<uint16_t>(MEM_WORDS - 1); // stack grows DOWN
    flags_   = Flags{};
    halted_  = false;
    cycles_  = 0;
    trace_.clear();
}

void CPU::loadProgram(const std::vector<uint16_t>& code, uint16_t at) {
    for (size_t i = 0; i < code.size(); ++i)
        mem_[at + i] = code[i];
    pc_ = at;
}

// R0 is hardwired to zero: writes to it are silently dropped. This is a
// deliberate ISA choice (like MIPS $zero) giving us a free constant 0.
void CPU::writeReg(uint8_t i, uint16_t v) {
    if (i == 0) return;
    regs_[i] = v;
}

uint16_t CPU::fetch() {
    uint16_t word = mem_[pc_];
    ++pc_;                 // fixed-width ISA => always advance by one word
    return word;
}

// Update Z/N/C from a 32-bit-wide arithmetic result. We compute in 32
// bits so we can SEE the carry out of bit 15 before truncating to 16.
void CPU::setFlagsFrom(uint32_t wide) {
    uint16_t low = static_cast<uint16_t>(wide & 0xFFFF);
    flags_.zero     = (low == 0);
    flags_.negative = (low & 0x8000) != 0;     // high bit = sign bit
    flags_.carry    = (wide & 0x10000) != 0;   // anything above 16 bits
}

void CPU::execute(const Instruction& in, std::string& note) {
    std::ostringstream n;
    const uint16_t a = regs_[in.rs];
    const uint16_t b = regs_[in.rt];

    switch (in.op) {
        case Opcode::NOP:
            n << "nop";
            break;

        case Opcode::LOADI:
            writeReg(in.rd, in.imm);
            n << "R" << +in.rd << " = " << +in.imm << " (imm)";
            break;

        case Opcode::LOAD:
            writeReg(in.rd, mem_[in.imm]);
            n << "R" << +in.rd << " = mem[" << +in.imm << "] = " << mem_[in.imm];
            break;

        case Opcode::STORE:
            mem_[in.imm] = regs_[in.rd];
            n << "mem[" << +in.imm << "] = R" << +in.rd << " = " << regs_[in.rd];
            break;

        case Opcode::MOV:
            writeReg(in.rd, a);
            n << "R" << +in.rd << " = R" << +in.rs << " = " << a;
            break;

        case Opcode::ADD: {
            uint32_t r = (uint32_t)a + b;
            setFlagsFrom(r);
            writeReg(in.rd, (uint16_t)r);
            n << "R" << +in.rd << " = " << a << " + " << b << " = " << (uint16_t)r;
            break;
        }
        case Opcode::SUB: {
            uint32_t r = (uint32_t)a - b;
            setFlagsFrom(r);
            writeReg(in.rd, (uint16_t)r);
            n << "R" << +in.rd << " = " << a << " - " << b << " = " << (uint16_t)r;
            break;
        }
        case Opcode::AND:
            writeReg(in.rd, a & b); setFlagsFrom(a & b);
            n << "R" << +in.rd << " = " << a << " & " << b << " = " << (a & b);
            break;
        case Opcode::OR:
            writeReg(in.rd, a | b); setFlagsFrom(a | b);
            n << "R" << +in.rd << " = " << a << " | " << b << " = " << (a | b);
            break;
        case Opcode::XOR:
            writeReg(in.rd, a ^ b); setFlagsFrom(a ^ b);
            n << "R" << +in.rd << " = " << a << " ^ " << b << " = " << (a ^ b);
            break;
        case Opcode::NOT:
            writeReg(in.rd, ~a); setFlagsFrom(~a);
            n << "R" << +in.rd << " = ~" << a << " = " << (uint16_t)~a;
            break;
        case Opcode::SHL:
            writeReg(in.rd, a << (b & 15)); setFlagsFrom(a << (b & 15));
            n << "R" << +in.rd << " = " << a << " << " << b;
            break;
        case Opcode::SHR:
            writeReg(in.rd, a >> (b & 15)); setFlagsFrom(a >> (b & 15));
            n << "R" << +in.rd << " = " << a << " >> " << b;
            break;

        case Opcode::CMP: {
            uint32_t r = (uint32_t)a - b;      // subtract but DISCARD result
            setFlagsFrom(r);                    // we only want the flags
            n << "cmp R" << +in.rs << "(" << a << ") , R" << +in.rt
              << "(" << b << ")  -> Z=" << flags_.zero << " N=" << flags_.negative;
            break;
        }

        case Opcode::JMP:
            pc_ = in.addr;
            n << "jmp " << in.addr;
            break;
        case Opcode::JZ:
            if (flags_.zero) { pc_ = in.addr; n << "jz taken -> " << in.addr; }
            else             { n << "jz not taken"; }
            break;
        case Opcode::JNZ:
            if (!flags_.zero) { pc_ = in.addr; n << "jnz taken -> " << in.addr; }
            else              { n << "jnz not taken"; }
            break;

        case Opcode::PUSH:
            mem_[--sp_] = regs_[in.rd];
            n << "push R" << +in.rd << " (" << regs_[in.rd] << ") -> mem[" << sp_ << "]";
            break;
        case Opcode::POP:
            writeReg(in.rd, mem_[sp_++]);
            n << "pop mem[" << (sp_ - 1) << "] -> R" << +in.rd;
            break;

        case Opcode::MUL: {
            uint32_t r = (uint32_t)a * b;       // 32-bit product
            setFlagsFrom(r);                     // carry set if > 16 bits
            writeReg(in.rd, (uint16_t)r);
            n << "R" << +in.rd << " = " << a << " * " << b << " = " << (uint16_t)r;
            break;
        }
        case Opcode::DIV: {
            if (b == 0) {                         // divide-by-zero guard
                writeReg(in.rd, 0);
                flags_.carry = true;              // signal the error via carry
                n << "R" << +in.rd << " = " << a << " / 0  -> ERROR (carry set)";
            } else {
                uint16_t q = a / b;
                setFlagsFrom(q);
                writeReg(in.rd, q);
                n << "R" << +in.rd << " = " << a << " / " << b << " = " << q;
            }
            break;
        }
        case Opcode::CALL:
            mem_[--sp_] = pc_;        // pc_ already points at NEXT instr (return addr)
            pc_ = in.addr;            // jump into the subroutine
            n << "call " << in.addr << "  (return addr " << mem_[sp_] << " pushed)";
            break;
        case Opcode::RET:
            pc_ = mem_[sp_++];        // pop return address back into PC
            n << "ret -> " << pc_;
            break;

        case Opcode::HALT:
            halted_ = true;
            n << "halt";
            break;

        case Opcode::OUT:
            n << "OUT R" << +in.rd << " = " << regs_[in.rd];
            outputs_.push_back(regs_[in.rd]);
            break;
    }
    note = n.str();
}

bool CPU::step() {
    if (halted_) return false;

    uint16_t pcAtFetch = pc_;
    uint16_t word = fetch();
    Instruction in = decode(word);

    std::string note;
    execute(in, note);
    ++cycles_;

    if (traceOn_ || onStep_) {
        TraceEvent ev;
        ev.pc    = pcAtFetch;
        ev.in    = in;
        ev.note  = note;
        ev.regs  = regs_;
        ev.sp    = sp_;
        ev.flags = flags_;
        if (traceOn_) trace_.push_back(ev);
        if (onStep_)  onStep_(*this, ev);
    }
    return !halted_;
}

void CPU::run(uint64_t maxSteps) {
    uint64_t s = 0;
    while (!halted_ && s < maxSteps) {
        step();
        ++s;
    }
}
