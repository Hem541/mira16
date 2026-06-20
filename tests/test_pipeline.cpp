#include "cpu.h"
#include "pipeline.h"
#include "assembler.h"
#include <iostream>
#include <string>
#include <vector>

// Run a program on BOTH the scalar CPU and the pipelined CPU.
// They MUST produce identical register results, or the pipeline is buggy.
static bool compare(const std::string& name, const std::string& src) {
    auto a = assemble(src);
    if (!a.ok()) { std::cout << name << ": ASM FAIL\n"; return false; }

    CPU scalar; scalar.enableTrace(false);
    scalar.loadProgram(a.code); scalar.run();

    PipelinedCPU pipe;
    pipe.loadProgram(a.code); pipe.run();

    bool match = true;
    for (int i = 0; i < 8; ++i)
        if (scalar.reg(i) != pipe.reg(i)) match = false;

    std::cout << (match ? "MATCH " : "MISMATCH ") << name
              << "  | scalar cycles=" << scalar.cycles()
              << "  pipe cycles=" << pipe.cycles()
              << "  retired=" << pipe.retired()
              << "  stalls=" << pipe.stalls()
              << "  flushes=" << pipe.flushes()
              << "  IPC=" << pipe.ipc() << "\n";
    if (!match) {
        std::cout << "   scalar: ";
        for (int i=0;i<8;i++) std::cout << "R"<<i<<"="<<scalar.reg(i)<<" ";
        std::cout << "\n   pipe:   ";
        for (int i=0;i<8;i++) std::cout << "R"<<i<<"="<<pipe.reg(i)<<" ";
        std::cout << "\n";
    }
    return match;
}

int main(){
    int fails = 0;

    // 1. pure arithmetic, no hazards
    if(!compare("arith",
        "LOADI R1,5\nLOADI R2,3\nADD R3,R1,R2\nMUL R4,R3,R2\nHALT")) fails++;

    // 2. back-to-back dependency (forwarding test): R3 used immediately
    if(!compare("forward-chain",
        "LOADI R1,10\nLOADI R2,20\nADD R3,R1,R2\nADD R4,R3,R1\nADD R5,R4,R3\nHALT")) fails++;

    // 3. load-use hazard
    if(!compare("load-use",
        "LOADI R1,42\nSTORE R1,[100]\nLOAD R2,[100]\nADD R3,R2,R1\nHALT")) fails++;

    // 4. branch / control hazard (loop)
    if(!compare("loop-mul",
        "LOADI R1,6\nLOADI R2,7\nLOADI R3,0\nLOADI R6,1\nLOADI R7,0\n"
        "CMP R2,R7\nJZ 12\nADD R3,R3,R1\nSUB R2,R2,R6\nJMP 5\nNOP\nNOP\nHALT")) fails++;

    // 5. fibonacci
    if(!compare("fib",
        "LOADI R1,0\nLOADI R2,1\nLOADI R4,10\nLOADI R6,1\n"
        "ADD R3,R1,R2\nMOV R1,R2\nMOV R2,R3\nSUB R4,R4,R6\nJNZ 4\nHALT")) fails++;

    std::cout << "\n" << (fails==0 ? "ALL PIPELINE TESTS MATCH SCALAR" :
                          std::to_string(fails)+" MISMATCH(ES)") << "\n";
    return fails;
}
