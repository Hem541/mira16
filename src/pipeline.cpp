#include "pipeline.h"
#include <sstream>

// pipelined core.
// latches snapshotted at cycle start, committed at end (double-buffer).

PipelinedCPU::PipelinedCPU() : mem_(P_MEM_WORDS, 0) { reset(); }

void PipelinedCPU::reset() {
    regs_.fill(0);
    std::fill(mem_.begin(), mem_.end(), 0);
    pc_ = 0;
    sp_ = (uint16_t)(P_MEM_WORDS - 1);
    flags_ = PFlags{};
    if_id_  = IF_ID{};
    id_ex_  = ID_EX{};
    ex_mem_ = EX_MEM{};
    mem_wb_ = MEM_WB{};
    haltFetched_ = false;
    done_ = false;
    cycle_ = retired_ = stallCount_ = flushCount_ = 0;
    memStallCycles_ = 0;
    memBusy_ = 0;
    outputs_.clear();
    if (cache_) cache_->reset();
    trace_.clear();
}

void PipelinedCPU::loadProgram(const std::vector<uint16_t>& code, uint16_t at) {
    for (size_t i = 0; i < code.size(); ++i) mem_[at + i] = code[i];
    pc_ = at;
}

static bool writesReg(const Instruction& in, uint8_t& which) {
    switch (in.op) {
        case Opcode::ADD: case Opcode::SUB: case Opcode::MUL:
        case Opcode::DIV: case Opcode::AND: case Opcode::OR:
        case Opcode::XOR: case Opcode::NOT: case Opcode::SHL:
        case Opcode::SHR: case Opcode::MOV: case Opcode::LOADI:
        case Opcode::LOAD: case Opcode::POP:
            which = in.rd;
            return in.rd != 0;
        default:
            return false;
    }
}

// These now operate on EXPLICIT snapshot arguments, not member latches,
// so ordering within a cycle cannot corrupt shared state.
bool PipelinedCPU::tick() {
    if (done_) return false;

    // ---- 0. MEMORY-BUSY STALL ----
    // If a cache miss is still being serviced, the whole pipeline freezes
    // this cycle: no stage advances, we just burn one memory cycle. This
    // models the MEM stage holding up everything behind it on a miss.
    if (memBusy_ > 0) {
        --memBusy_;
        ++memStallCycles_;
        PipeSnapshot fz;
        fz.cycle = cycle_;
        fz.ifPC  = if_id_.bubble  ? -1 : (int)if_id_.pc;
        fz.idPC  = id_ex_.bubble  ? -1 : (int)id_ex_.pc;
        fz.exPC  = ex_mem_.bubble ? -1 : (int)ex_mem_.pc;
        fz.memPC = mem_wb_.bubble ? -1 : (int)mem_wb_.pc;
        fz.regs = regs_;
        fz.stalled = true;
        fz.note = "memory stall (cache miss in flight)";
        trace_.push_back(fz);
        ++cycle_;
        return !done_;
    }

    // ---- 1. SNAPSHOT current latches (the "inputs" for this cycle) ----
    const IF_ID  s_if_id  = if_id_;
    const ID_EX  s_id_ex  = id_ex_;
    const EX_MEM s_ex_mem = ex_mem_;
    const MEM_WB s_mem_wb = mem_wb_;

    PipeSnapshot snap;
    snap.cycle = cycle_;
    snap.ifPC  = s_if_id.bubble  ? -1 : (int)s_if_id.pc;
    snap.idPC  = s_id_ex.bubble  ? -1 : (int)s_id_ex.pc;
    snap.exPC  = s_ex_mem.bubble ? -1 : (int)s_ex_mem.pc;
    snap.memPC = s_mem_wb.bubble ? -1 : (int)s_mem_wb.pc;

    // ---- 2. WB: commit s_mem_wb into the register file ----
    if (!s_mem_wb.bubble) {
        uint8_t w;
        if (writesReg(s_mem_wb.in, w)) regs_[w] = s_mem_wb.writeData;
        if (s_mem_wb.in.op == Opcode::OUT) outputs_.push_back(s_mem_wb.writeData);
        ++retired_;
        if (s_mem_wb.in.op == Opcode::HALT) done_ = true;
    }

    // ---- 3. MEM: from s_ex_mem -> new MEM_WB ----
    MEM_WB n_mem_wb{};
    if (!s_ex_mem.bubble) {
        const Instruction& in = s_ex_mem.in;
        n_mem_wb.bubble = false;
        n_mem_wb.in = in;
        n_mem_wb.pc = s_ex_mem.pc;

        bool isMemOp = (in.op == Opcode::LOAD || in.op == Opcode::STORE);

        // If a cache is attached and this is a memory op, consult it. A
        // miss returns a latency > 1; we charge (latency-1) extra cycles
        // by setting memBusy_, which freezes the whole pipeline (step 0)
        // on the following cycles. The data itself still comes from real
        // memory below -- the cache models TIMING, not data storage.
        if (cache_ && isMemOp) {
            uint32_t lat = cache_->access(s_ex_mem.aluResult,
                                          in.op == Opcode::STORE);
            if (lat > 1) memBusy_ = lat - 1;   // remaining stall cycles
        }

        if (in.op == Opcode::LOAD) {
            n_mem_wb.writeData = mem_[s_ex_mem.aluResult];
        } else if (in.op == Opcode::STORE) {
            mem_[s_ex_mem.aluResult] = s_ex_mem.valRt;
            n_mem_wb.writeData = 0;
        } else if (in.op == Opcode::PUSH) {
            mem_[s_ex_mem.aluResult] = s_ex_mem.valRt;  // write pushed value
            n_mem_wb.writeData = 0;
        } else if (in.op == Opcode::POP) {
            n_mem_wb.writeData = mem_[s_ex_mem.aluResult]; // read into rd at WB
        } else {
            n_mem_wb.writeData = s_ex_mem.aluResult;
        }
    }

    // ---- 4. EX: from s_id_ex -> new EX_MEM, with FORWARDING ----
    //  Forwarding sources, in priority order:
    //    (a) the instruction in EX/MEM (s_ex_mem) -- most recent result
    //    (b) the instruction in MEM/WB (s_mem_wb) -- older result
    //  We forward the FINAL data each would write.
    EX_MEM n_ex_mem{};
    bool branchTaken = false; uint16_t branchTo = 0;
    if (!s_id_ex.bubble) {
        const Instruction& in = s_id_ex.in;
        n_ex_mem.bubble = false;
        n_ex_mem.in = in;
        n_ex_mem.pc = s_id_ex.pc;

        auto forward = [&](uint8_t srcReg, uint16_t fallback) -> uint16_t {
            uint8_t w;
            if (srcReg == 0) return 0; // R0 always 0
            // (a) EX/MEM latch: result available (for non-load it's aluResult)
            if (!s_ex_mem.bubble && writesReg(s_ex_mem.in, w) && w == srcReg) {
                // a LOAD's value isn't ready here, but load-use stall guarantees
                // we never reach this case for a load feeding the very next instr
                return s_ex_mem.aluResult;
            }
            // (b) MEM/WB latch: final write data
            if (!s_mem_wb.bubble && writesReg(s_mem_wb.in, w) && w == srcReg) {
                return s_mem_wb.writeData;
            }
            return fallback;
        };

        uint16_t a = forward(in.rs, s_id_ex.valRs);
        uint16_t b = forward(in.rt, s_id_ex.valRt);
        n_ex_mem.valRt = b; // STORE writes this

        auto setFlags = [&](uint32_t wide){
            uint16_t low = (uint16_t)(wide & 0xFFFF);
            flags_.zero = (low==0);
            flags_.negative = (low & 0x8000)!=0;
            flags_.carry = (wide & 0x10000)!=0;
        };
        switch (in.op) {
            case Opcode::ADD:{uint32_t r=(uint32_t)a+b;setFlags(r);n_ex_mem.aluResult=(uint16_t)r;break;}
            case Opcode::SUB:{uint32_t r=(uint32_t)a-b;setFlags(r);n_ex_mem.aluResult=(uint16_t)r;break;}
            case Opcode::MUL:{uint32_t r=(uint32_t)a*b;setFlags(r);n_ex_mem.aluResult=(uint16_t)r;break;}
            case Opcode::DIV:
                if(b==0){n_ex_mem.aluResult=0;flags_.carry=true;}
                else{n_ex_mem.aluResult=a/b;setFlags(a/b);} break;
            case Opcode::AND:n_ex_mem.aluResult=a&b;setFlags(a&b);break;
            case Opcode::OR: n_ex_mem.aluResult=a|b;setFlags(a|b);break;
            case Opcode::XOR:n_ex_mem.aluResult=a^b;setFlags(a^b);break;
            case Opcode::NOT:n_ex_mem.aluResult=(uint16_t)~a;setFlags((uint16_t)~a);break;
            case Opcode::SHL:n_ex_mem.aluResult=a<<(b&15);setFlags(a<<(b&15));break;
            case Opcode::SHR:n_ex_mem.aluResult=a>>(b&15);setFlags(a>>(b&15));break;
            case Opcode::MOV:n_ex_mem.aluResult=a;break;
            case Opcode::LOADI:n_ex_mem.aluResult=in.imm;break;
            case Opcode::LOAD: n_ex_mem.aluResult=in.imm;break;  // address
            case Opcode::STORE:n_ex_mem.aluResult=in.imm;break;  // address
            case Opcode::CMP:{uint32_t r=(uint32_t)a-b;setFlags(r);break;}
            case Opcode::JMP:branchTaken=true;branchTo=in.addr;break;
            case Opcode::JZ: if(flags_.zero){branchTaken=true;branchTo=in.addr;}break;
            case Opcode::JNZ:if(!flags_.zero){branchTaken=true;branchTo=in.addr;}break;

            // --- stack push/pop: manage SP here in EX (in-order) ---
            // (CALL/RET are supported on the scalar CPU; the compiler does
            //  not emit them, so the pipeline implements PUSH/POP only.)
            case Opcode::PUSH:
                sp_ = (uint16_t)(sp_ - 1);
                n_ex_mem.aluResult = sp_;          // address to write
                n_ex_mem.valRt = forward(in.rd, regs_[in.rd]); // value to push
                break;
            case Opcode::POP:
                n_ex_mem.aluResult = sp_;          // address to read
                sp_ = (uint16_t)(sp_ + 1);
                break;

            case Opcode::OUT:
                // forward the value to print into aluResult; WB emits it
                n_ex_mem.aluResult = forward(in.rd, regs_[in.rd]);
                break;

            default:break;
        }
        n_ex_mem.takenBranch = branchTaken;
        n_ex_mem.branchTarget = branchTo;
    }

    // ---- 5. ID: from s_if_id -> new ID_EX, detect LOAD-USE stall ----
    ID_EX n_id_ex{};
    bool stall = false;
    if (!s_if_id.bubble) {
        Instruction in = decode(s_if_id.raw);
        // LOAD-USE: instruction currently in EX (s_id_ex) is a LOAD whose
        // time to forward into our EX next cycle -> stall one cycle.
        if (!s_id_ex.bubble && s_id_ex.in.op == Opcode::LOAD) {
            uint8_t ld = s_id_ex.in.rd;
            bool usesR = (in.fmt==Format::R || in.op==Opcode::CMP);
            bool reads = usesR && (in.rs==ld || in.rt==ld);
            bool st    = (in.op==Opcode::STORE) && in.rd==ld;
            bool out   = (in.op==Opcode::OUT)   && in.rd==ld;
            bool push  = (in.op==Opcode::PUSH)  && in.rd==ld;
            if (ld!=0 && (reads || st || out || push)) stall = true;
        }
        if (!stall) {
            n_id_ex.bubble = false;
            n_id_ex.pc = s_if_id.pc;
            n_id_ex.in = in;
            n_id_ex.valRs = regs_[in.rs];
            n_id_ex.valRt = regs_[in.rt];
        }
        // if stall: n_id_ex stays a bubble (inject bubble into EX)
    }

    // ---- 6. IF: fetch next, unless stalled or halted ----
    IF_ID n_if_id{};
    bool flushed = branchTaken;

    if (flushed) {
        // control hazard: discard what we fetched/decoded this cycle
        pc_ = branchTo;
        n_if_id = IF_ID{};   // flush IF/ID
        n_id_ex = ID_EX{};   // flush ID/EX
        haltFetched_ = false; // a HALT we may have fetched was on the wrong path
        ++flushCount_;
    } else if (stall) {
        // hold IF/ID and PC: re-present the same instruction next cycle
        n_if_id = s_if_id;   // keep what was in IF/ID
        ++stallCount_;
    } else if (!haltFetched_) {
        n_if_id.bubble = false;
        n_if_id.pc = pc_;
        n_if_id.raw = mem_[pc_];
        if (decode(n_if_id.raw).op == Opcode::HALT) haltFetched_ = true;
        ++pc_;
    }
    // else: past HALT, fetch bubbles (n_if_id stays bubble)

    // ---- 7. COMMIT all new latch values at once ----
    if_id_  = n_if_id;
    id_ex_  = n_id_ex;
    ex_mem_ = n_ex_mem;
    mem_wb_ = n_mem_wb;

    // record snapshot
    snap.regs = regs_;
    snap.stalled = stall;
    snap.flushed = flushed;
    std::ostringstream n;
    if (flushed) n << "branch taken -> flush, PC=" << branchTo << " ";
    if (stall)   n << "load-use hazard -> stall";
    snap.note = n.str();
    trace_.push_back(snap);

    ++cycle_;
    return !done_;
}

void PipelinedCPU::run(uint64_t maxCycles) {
    while (!done_ && cycle_ < maxCycles) tick();
}

// unused legacy stage methods (kept out; logic is inline in tick())
void PipelinedCPU::doWB() {}
void PipelinedCPU::doMEM() {}
void PipelinedCPU::doEX() {}
void PipelinedCPU::doID(bool&) {}
void PipelinedCPU::doIF(bool) {}
uint16_t PipelinedCPU::forwardRs(const ID_EX&, uint16_t o){return o;}
uint16_t PipelinedCPU::forwardRt(const ID_EX&, uint16_t o){return o;}
