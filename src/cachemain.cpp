#include "cpu.h"
#include "pipeline.h"
#include "assembler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

// cache runner. prints hit-rate + amat.

static std::string readFile(const std::string& p){
    std::ifstream f(p); std::stringstream ss; ss<<f.rdbuf(); return ss.str();
}

int main(int argc, char** argv){
    if (argc < 2){
        std::cerr<<"usage: "<<argv[0]<<" <program.asm> [blockWords numSlots missPenalty]\n";
        return 1;
    }
    uint32_t bw = (argc>2)?std::stoul(argv[2]):4;
    uint32_t ns = (argc>3)?std::stoul(argv[3]):16;
    uint32_t mp = (argc>4)?std::stoul(argv[4]):10;

    auto code = assemble(readFile(argv[1]));
    if (!code.ok()){
        std::cerr<<"asm failed\n";
        for(auto&e:code.errors) std::cerr<<"  "<<e<<"\n";
        return 2;
    }

    PipelinedCPU base; base.loadProgram(code.code); base.run();

    PipelinedCPU cached; cached.enableCache(bw, ns, mp);
    cached.loadProgram(code.code); cached.run();

    // correctness: cache must not change results
    bool match = true;
    for (int i=0;i<8;i++) if (base.reg(i)!=cached.reg(i)) match=false;

    const Cache* c = cached.cache();
    std::cout << "=== L1 CACHE REPORT ===\n";
    std::cout << "config                 : " << c->config() << "\n";
    std::cout << "result check vs no-cache: " << (match?"PASS (identical)":"FAIL") << "\n\n";
    std::cout << "memory accesses        : " << c->accesses() << "\n";
    std::cout << "  hits                 : " << c->hits() << "\n";
    std::cout << "  misses               : " << c->misses() << "\n";
    std::cout << "hit rate               : " << std::fixed << std::setprecision(1)
              << (c->hitRate()*100.0) << "%\n";
    std::cout << "AMAT (avg mem access)  : " << std::setprecision(3)
              << c->amat() << " cycles  (1.0 = perfect, "
              << mp << " = no cache)\n";
    std::cout << "cycles lost to mem     : " << cached.memStallCycles() << "\n\n";

    std::cout << "total cycles  no-cache : " << base.cycles() << "\n";
    std::cout << "total cycles  with L1  : " << cached.cycles() << "\n";
    return match?0:1;
}
