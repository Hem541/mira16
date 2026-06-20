#include "cpu.h"
#include "assembler.h"
#include <iostream>
#include <chrono>

// throughput benchmark. prints instructions/sec.

int main() {
    // A tight arithmetic loop: accumulate R3 += R1*R2, 1000 iterations.
    // R4 counts down from 1000. No I/O inside -> pure execution cost.
    std::string src =
        "LOADI R1, 7\n"
        "LOADI R2, 9\n"
        "LOADI R3, 0\n"
        "LOADI R4, 1000\n"      // note: 8-bit immediate caps at 255...
        "LOADI R6, 1\n"
        "LOADI R7, 0\n"
        "loop: CMP R4, R7\n"
        "JZ done\n"
        "MUL R5, R1, R2\n"
        "ADD R3, R3, R5\n"
        "SUB R4, R4, R6\n"
        "JMP loop\n"
        "done: HALT\n";

    auto a = assemble(src);
    if (!a.ok()) { std::cerr << "asm failed\n"; return 1; }

    const int REPEATS = 20000;   // run the whole program this many times
    uint64_t totalInstr = 0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REPEATS; ++i) {
        CPU cpu;
        cpu.enableTrace(false);          // measure the ENGINE, not logging
        cpu.loadProgram(a.code);
        cpu.run();
        totalInstr += cpu.cycles();
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double secs = std::chrono::duration<double>(t1 - t0).count();
    double ips  = totalInstr / secs;

    std::cout << "=== CPU EMULATOR BENCHMARK ===\n";
    std::cout << "program runs      : " << REPEATS << "\n";
    std::cout << "instructions total: " << totalInstr << "\n";
    std::cout << "wall time         : " << secs << " s\n";
    std::cout << "throughput        : " << (uint64_t)ips
              << " instructions/sec\n";
    std::cout << "                  ~ " << (ips / 1e6) << " MIPS\n";
    return 0;
}
