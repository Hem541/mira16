#include "cpu.h"
#include "pipeline.h"
#include "assembler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

// pipeline runner. prints cycles/stalls/flushes/ipc.

static std::string readFile(const std::string& p){
    std::ifstream f(p); std::stringstream ss; ss<<f.rdbuf(); return ss.str();
}

int main(int argc, char** argv){
    if (argc < 2){ std::cerr<<"usage: "<<argv[0]<<" <program.asm>\n"; return 1; }
    auto code = assemble(readFile(argv[1]));
    if (!code.ok()){
        std::cerr<<"asm failed:\n";
        for(auto&e:code.errors) std::cerr<<"  "<<e<<"\n";
        return 2;
    }

    // scalar reference
    CPU scalar; scalar.enableTrace(false);
    scalar.loadProgram(code.code); scalar.run();

    // pipelined
    PipelinedCPU pipe; pipe.loadProgram(code.code); pipe.run();

    // correctness cross-check
    bool match = true;
    for (int i=0;i<8;i++) if (scalar.reg(i)!=pipe.reg(i)) match=false;

    std::cout << "=== PIPELINED CPU ===\n";
    std::cout << "result check vs scalar : " << (match?"PASS (identical)":"FAIL") << "\n";
    std::cout << "instructions retired   : " << pipe.retired() << "\n";
    std::cout << "clock cycles           : " << pipe.cycles() << "\n";
    std::cout << "pipeline stalls        : " << pipe.stalls()
              << "  (load-use hazards)\n";
    std::cout << "pipeline flushes       : " << pipe.flushes()
              << "  (taken branches)\n";
    std::cout << "IPC (instr per cycle)  : " << std::fixed << std::setprecision(3)
              << pipe.ipc() << "   (ideal = 1.0)\n";

    // A scalar CPU takes ~5 cycles/instruction conceptually; show the
    // pipeline's effective speedup in instructions-per-cycle terms.
    double idealScalarCycles = pipe.retired() * 5.0;
    double speedup = idealScalarCycles / pipe.cycles();
    std::cout << "speedup vs 5-cyc scalar: " << speedup << "x\n";

    std::cout << "\nfinal registers: ";
    for (int i=0;i<8;i++) std::cout<<"R"<<i<<"="<<pipe.reg(i)<<"  ";
    std::cout << "\n";
    return match?0:1;
}
