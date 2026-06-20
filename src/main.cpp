#include "cpu.h"
#include "assembler.h"
#include "tracer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

// scalar cpu cli runner.

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0]
                  << " <program.asm> [--step|--quiet]\n";
        return 1;
    }
    std::string path = argv[1];
    bool stepMode  = false, quiet = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--step")  stepMode = true;
        if (a == "--quiet") quiet = true;
    }

    // ---- assemble ---------------------------------------------------
    std::string src = readFile(path);
    AssembleResult asmres = assemble(src);
    if (!asmres.ok()) {
        std::cerr << "Assembly failed:\n";
        for (auto& e : asmres.errors) std::cerr << "  " << e << "\n";
        return 2;
    }

    // ---- set up the machine ----------------------------------------
    CPU cpu;
    cpu.loadProgram(asmres.code);
    const std::vector<uint16_t>& program = asmres.code;

    // Live dashboard hook (skipped in --quiet for speed).
    if (!quiet) {
        cpu.onStep([&](const CPU& c, const TraceEvent& ev){
            renderFrame(c, ev, program, /*clear=*/true);
            if (stepMode) {
                std::cout << "\n " << "[Enter] step  /  Ctrl-C quit ";
                std::cout.flush();
                std::cin.get();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(140));
            }
        });
    }

    // ---- run --------------------------------------------------------
    cpu.run();

    // final frame + summary
    if (!quiet && !cpu.trace().empty())
        renderFrame(cpu, cpu.trace().back(), program, true);

    std::cout << "\n--- HALTED after " << cpu.cycles() << " cycles ---\n";
    std::cout << "Final registers: ";
    for (int i = 0; i < NUM_REGS; ++i)
        std::cout << "R" << i << "=" << cpu.reg(i) << "  ";
    std::cout << "\n";

    dumpTrace(cpu, "trace.csv");
    std::cout << "Wrote trace.csv (" << cpu.trace().size()
              << " steps) for viz/trace_plot.py\n";
    return 0;
}
