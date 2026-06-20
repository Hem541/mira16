#include "cpu.h"
#include "pipeline.h"
#include "assembler.h"
#include <iostream>
int main(){
    // A loop that hammers the SAME memory address repeatedly: should be
    // cache-friendly (one cold miss, then all hits).
    std::string src =
        "LOADI R1,5\n"        // value
        "LOADI R4,20\n"       // loop count
        "LOADI R6,1\n"
        "LOADI R7,0\n"
        "STORE R1,[100]\n"    // addr 100
        "LOAD R2,[100]\n"     // read it back (same addr each iter)
        "SUB R4,R4,R6\n"
        "CMP R4,R7\n"
        "JNZ 4\n"             // loop back to STORE
        "HALT\n";
    auto a = assemble(src);

    // correctness: result must match scalar regardless of cache
    CPU scalar; scalar.enableTrace(false); scalar.loadProgram(a.code); scalar.run();

    PipelinedCPU noCache; noCache.loadProgram(a.code); noCache.run();

    PipelinedCPU withCache; withCache.enableCache(4,16,10);
    withCache.loadProgram(a.code); withCache.run();

    bool m1=true,m2=true;
    for(int i=0;i<8;i++){ if(scalar.reg(i)!=noCache.reg(i))m1=false;
                          if(scalar.reg(i)!=withCache.reg(i))m2=false; }

    std::cout<<"correctness: noCache="<<(m1?"PASS":"FAIL")
             <<"  withCache="<<(m2?"PASS":"FAIL")<<"\n\n";

    std::cout<<"NO CACHE (memory free):  cycles="<<noCache.cycles()
             <<" retired="<<noCache.retired()<<" IPC="<<noCache.ipc()<<"\n";

    std::cout<<"WITH CACHE (miss=10cyc): cycles="<<withCache.cycles()
             <<" retired="<<withCache.retired()
             <<" memStalls="<<withCache.memStallCycles()<<"\n";
    auto* c = withCache.cache();
    std::cout<<"  cache: hits="<<c->hits()<<" misses="<<c->misses()
             <<" hitRate="<<c->hitRate()<<" AMAT="<<c->amat()<<"\n";
    std::cout<<"  config: "<<c->config()<<"\n";

    // contrast: a cache-HOSTILE program that strides far apart every access
    std::cout<<"\n--- cache-hostile striding program ---\n";
    std::string hostile =
        "LOADI R1,1\nLOADI R4,8\nLOADI R6,1\nLOADI R7,0\nLOADI R5,0\n";
    // build stores to spread-out addresses by adding 64 each time
    hostile +=
        "STORE R1,[0]\n"
        "LOAD R2,[0]\n"
        "STORE R1,[64]\n"
        "LOAD R2,[64]\n"
        "STORE R1,[128]\n"
        "LOAD R2,[128]\n"
        "HALT\n";
    auto h = assemble(hostile);
    PipelinedCPU hc; hc.enableCache(4,16,10); hc.loadProgram(h.code); hc.run();
    auto* hcc = hc.cache();
    std::cout<<"  hits="<<hcc->hits()<<" misses="<<hcc->misses()
             <<" hitRate="<<hcc->hitRate()<<" AMAT="<<hcc->amat()<<"\n";

    return (m1&&m2)?0:1;
}
