#ifndef PIPELINE_H
#define PIPELINE_H

#include "isa.h"
#include "cache.h"
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

// 5-stage pipeline (IF ID EX MEM WB).
// data forwarding, load-use stall, branch flush.

constexpr int      P_NUM_REGS = 8;
constexpr uint32_t P_MEM_WORDS = 65536;

struct PFlags { bool zero=false, negative=false, carry=false; };

// ---- The inter-stage latches. Each holds one instruction's state -----
//  as it moves down the pipe. `bubble`=true means "no real instruction
//  here" (a NOP injected by a stall or flush).

// IF -> ID
struct IF_ID {
    bool     bubble = true;
    uint16_t pc = 0;        // PC of this instruction
    uint16_t raw = 0;       // the raw 16-bit word fetched
};

// ID -> EX
struct ID_EX {
    bool        bubble = true;
    uint16_t    pc = 0;
    Instruction in{};       // decoded fields
    uint16_t    valRs = 0;  // value read from register rs
    uint16_t    valRt = 0;  // value read from register rt
};

// EX -> MEM
struct EX_MEM {
    bool        bubble = true;
    uint16_t    pc = 0;
    Instruction in{};
    uint16_t    aluResult = 0; // ALU output (or address for LOAD/STORE)
    uint16_t    valRt = 0;     // value to STORE (for STORE instructions)
    bool        takenBranch = false;
    uint16_t    branchTarget = 0;
};

// MEM -> WB
struct MEM_WB {
    bool        bubble = true;
    uint16_t    pc = 0;
    Instruction in{};
    uint16_t    writeData = 0; // final value to write into rd
};

// One row of the per-cycle trace, for the visualizer: what instruction
// (by PC) is sitting in each of the 5 stages this cycle.
struct PipeSnapshot {
    uint64_t cycle = 0;
    // PC of the instruction in each stage, or -1 if bubble/empty
    int ifPC = -1, idPC = -1, exPC = -1, memPC = -1, wbPC = -1;
    std::array<uint16_t, P_NUM_REGS> regs{};
    std::string note;       // what happened this cycle (hazard? forward?)
    bool stalled = false;
    bool flushed = false;
};

class PipelinedCPU {
public:
    PipelinedCPU();
    void reset();
    void loadProgram(const std::vector<uint16_t>& code, uint16_t at = 0);

    // Run the clock until the program drains out of the pipe. Returns
    // when a HALT has passed through WB and the pipe is empty.
    void run(uint64_t maxCycles = 1'000'000);
    bool tick();   // advance the clock exactly ONE cycle

    // Attach a direct-mapped L1 cache to data memory. When enabled, every
    // LOAD/STORE goes through the cache; a miss stalls the MEM stage for
    // the miss penalty, modeling real memory-hierarchy cost.
    void enableCache(uint32_t blockWords = 4, uint32_t numSlots = 16,
                     uint32_t memLatency = 10) {
        cache_ = std::make_unique<Cache>(blockWords, numSlots, memLatency);
    }
    bool hasCache() const { return (bool)cache_; }
    const Cache* cache() const { return cache_.get(); }
    // Total cycles spent stalled waiting on memory (miss penalties).
    uint64_t memStallCycles() const { return memStallCycles_; }

    // ---- introspection -------------------------------------------
    uint16_t reg(int i) const { return regs_[i]; }
    uint64_t cycles()   const { return cycle_; }
    uint64_t retired()  const { return retired_; }   // instructions completed
    uint64_t stalls()   const { return stallCount_; }
    uint64_t flushes()  const { return flushCount_; }
    bool     done()     const { return done_; }
    const std::vector<PipeSnapshot>& trace() const { return trace_; }
    const std::vector<uint16_t>& outputs() const { return outputs_; }

    // Throughput metric: instructions retired per cycle. A perfect
    // pipeline approaches 1.0 (called IPC). Hazards push it below 1.
    double ipc() const {
        return cycle_ ? (double)retired_ / (double)cycle_ : 0.0;
    }

private:
    std::array<uint16_t, P_NUM_REGS> regs_{};
    std::vector<uint16_t> mem_;
    uint16_t pc_ = 0;
    uint16_t sp_ = (uint16_t)(P_MEM_WORDS - 1);  // stack pointer (grows down)
    PFlags flags_{};

    // current latch state
    IF_ID  if_id_;
    ID_EX  id_ex_;
    EX_MEM ex_mem_;
    MEM_WB mem_wb_;

    bool     haltFetched_ = false; // saw HALT in IF; stop fetching more
    bool     done_ = false;
    uint64_t cycle_ = 0;
    uint64_t retired_ = 0;
    uint64_t stallCount_ = 0;
    uint64_t flushCount_ = 0;

    // optional L1 cache + memory-stall bookkeeping
    std::unique_ptr<Cache> cache_;
    uint64_t memStallCycles_ = 0;
    uint32_t memBusy_ = 0;   // cycles remaining on an in-progress miss
    std::vector<uint16_t> outputs_;   // values emitted by OUT (at WB)

    std::vector<PipeSnapshot> trace_;

    // per-stage logic
    void doWB();
    void doMEM();
    void doEX();
    void doID(bool& stallOut);
    void doIF(bool stalled);

    // hazard helpers
    uint16_t forwardRs(const ID_EX& d, uint16_t original);
    uint16_t forwardRt(const ID_EX& d, uint16_t original);
};

#endif // PIPELINE_H
