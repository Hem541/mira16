#include "compiler.h"
#include "assembler.h"
#include "cpu.h"
#include "pipeline.h"
#include <iostream>
#include <fstream>
#include <sstream>

// minicc driver. compile + run a .mc file.

static std::string readFile(const std::string& p){
    std::ifstream f(p); std::stringstream ss; ss<<f.rdbuf(); return ss.str();
}

int main(int argc, char** argv){
    if (argc < 2){
        std::cerr<<"usage: "<<argv[0]<<" <file.mc> [--asm|--pipe]\n";
        return 1;
    }
    bool showAsm=false, usePipe=false;
    for (int i=2;i<argc;i++){
        std::string a=argv[i];
        if (a=="--asm") showAsm=true;
        if (a=="--pipe") usePipe=true;
    }

    std::string src = readFile(argv[1]);

    // STAGE 1-3: compile MiniC -> assembly
    auto c = compile(src);
    if (!c.ok()){
        std::cerr<<"compile error:\n";
        for (auto&e:c.errors) std::cerr<<"  "<<e<<"\n";
        return 2;
    }

    if (showAsm){
        std::cout<<c.asmText;
        return 0;
    }

    // STAGE 4: assemble -> machine code
    auto a = assemble(c.asmText);
    if (!a.ok()){
        std::cerr<<"internal: generated asm failed to assemble:\n";
        for (auto&e:a.errors) std::cerr<<"  "<<e<<"\n";
        return 3;
    }

    std::cout<<"compiled "<<argv[1]<<" -> "<<a.code.size()<<" instructions\n";
    std::cout<<"program output:\n";

    // STAGE 5: run on the scalar CPU (the reference execution model).
    CPU cpu; cpu.enableTrace(false); cpu.loadProgram(a.code); cpu.run();
    for (auto v: cpu.outputs()) std::cout<<"  "<<v<<"\n";
    std::cout<<"["<<cpu.cycles()<<" instructions executed]\n";
    (void)usePipe;
    return 0;
}
