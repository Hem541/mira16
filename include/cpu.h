#ifndef CPU_H
#define CPU_H

#include "isa.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// scalar cpu: registers, memory, flags + step/run.

constexpr int      NUM_REGS = 8;
constexpr uint32_t MEM_WORDS = 65536;   // 2^16 addressable words

// Status flags packed into one byte. Set by ADD/SUB/CMP, read by jumps.
struct Flags {
    bool zero     = false;  // last result was 0
    bool negative = false;  // last result had high bit set (signed neg)
    bool carry    = false;  // last add/sub carried/borrowed out of 16 bits
};

// A single executed step, recorded for the tracer/visualizer.
struct TraceEvent {
    uint16_t    pc;         // PC at the moment of fetch
    Instruction in;         // what we ran
    std::string note;       // human description, e.g. "R1 = 5 + 3 = 8"
    std::array<uint16_t, NUM_REGS> regs; // register snapshot AFTER execute
    uint16_t    sp;
    Flags       flags;
};

class CPU {
public:
    CPU();

    // ---- lifecycle --------------------------------------------------
    void reset();
    // Load already-assembled machine code into memory starting at addr.
    void loadProgram(const std::vector<uint16_t>& code, uint16_t at = 0);

    // ---- the loop ---------------------------------------------------
    void run(uint64_t maxSteps = 1'000'000); // run until HALT or limit
    bool step();   // execute exactly ONE instruction; false once halted

    // ---- introspection (used by tracer + visualizer) ---------------
    uint16_t reg(int i)  const { return regs_[i]; }
    uint16_t pc()        const { return pc_; }
    uint16_t sp()        const { return sp_; }
    Flags    flags()     const { return flags_; }
    uint16_t mem(uint16_t a) const { return mem_[a]; }
    bool     halted()    const { return halted_; }
    uint64_t cycles()    const { return cycles_; }
    const std::vector<TraceEvent>& trace() const { return trace_; }
    const std::vector<uint16_t>& outputs() const { return outputs_; }

    // Install a callback fired after every step (the live visualizer
    // hooks in here so the CPU stays decoupled from how it's displayed).
    void onStep(std::function<void(const CPU&, const TraceEvent&)> cb) {
        onStep_ = std::move(cb);
    }

    void enableTrace(bool on) { traceOn_ = on; }

private:
    // ---- state ------------------------------------------------------
    std::array<uint16_t, NUM_REGS> regs_{};
    std::vector<uint16_t>          mem_;   // sized to MEM_WORDS
    uint16_t pc_  = 0;
    uint16_t sp_  = 0;          // set to top of memory on reset
    Flags    flags_{};
    bool     halted_ = false;
    uint64_t cycles_ = 0;
    std::vector<uint16_t> outputs_;   // values emitted by OUT

    // ---- trace / hooks ---------------------------------------------
    bool traceOn_ = true;
    std::vector<TraceEvent> trace_;
    std::function<void(const CPU&, const TraceEvent&)> onStep_;

    // ---- internals --------------------------------------------------
    uint16_t fetch();                 // read mem[PC], advance PC
    void     execute(const Instruction& in, std::string& note);
    void     setFlagsFrom(uint32_t wideResult); // update Z/N/C
    void     writeReg(uint8_t i, uint16_t v);    // enforces R0 == 0
};

#endif // CPU_H
