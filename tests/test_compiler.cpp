#include "compiler.h"
#include "assembler.h"
#include "cpu.h"
#include <iostream>
#include <vector>
#include <string>

static std::vector<int> runMiniC(const std::string& src, bool show=false){
    auto c = compile(src);
    if (!c.ok()){
        std::cout<<"COMPILE ERROR:\n";
        for(auto&e:c.errors) std::cout<<"  "<<e<<"\n";
        return {};
    }
    if (show) std::cout<<"--- generated asm ---\n"<<c.asmText<<"---\n";
    auto a = assemble(c.asmText);
    if (!a.ok()){
        std::cout<<"ASM ERROR:\n";
        for(auto&e:a.errors) std::cout<<"  "<<e<<"\n";
        std::cout<<c.asmText<<"\n";
        return {};
    }
    CPU cpu; cpu.enableTrace(false); cpu.loadProgram(a.code); cpu.run();
    std::vector<int> outs;
    for (auto v: cpu.outputs()) outs.push_back((int)v);
    return outs;
}

static void check(const std::string& name, const std::string& src,
                  std::vector<int> expect, bool show=false){
    auto got = runMiniC(src, show);
    bool ok = (got==expect);
    std::cout<<(ok?"PASS ":"FAIL ")<<name<<"  expected[";
    for(size_t i=0;i<expect.size();i++) std::cout<<expect[i]<<(i+1<expect.size()?",":"");
    std::cout<<"] got[";
    for(size_t i=0;i<got.size();i++) std::cout<<got[i]<<(i+1<got.size()?",":"");
    std::cout<<"]\n";
}

int main(){
    // arithmetic + precedence
    check("arith", "print 2 + 3 * 4;", {14});
    check("parens", "print (2 + 3) * 4;", {20});
    check("vars", "var x = 5; var y = x * 3; print y;", {15});
    check("reassign", "var x = 10; x = x - 3; print x;", {7});

    // if true / false
    check("if-true",  "var x = 5; if (x > 3) { print 1; }", {1});
    check("if-false", "var x = 2; if (x > 3) { print 1; }", {});
    check("if-else",  "var x = 2; if (x > 3) { print 1; } else { print 2; }", {2});

    // all six relops
    check("eq", "if (5 == 5) { print 1; }", {1});
    check("ne", "if (5 != 4) { print 1; }", {1});
    check("lt", "if (3 < 5) { print 1; }", {1});
    check("gt", "if (5 > 3) { print 1; }", {1});
    check("le", "if (5 <= 5) { print 1; }", {1});
    check("ge", "if (5 >= 6) { print 1; } else { print 0; }", {0});

    // while loop: count down 3,2,1
    check("while", "var i = 3; while (i > 0) { print i; i = i - 1; }", {3,2,1});

    // a real program: sum 1..5 = 15
    check("sum1to5",
        "var i = 1; var s = 0; while (i <= 5) { s = s + i; i = i + 1; } print s;",
        {15});

    // factorial 5 = 120  (fits in 8-bit immediate range for the loop var)
    check("factorial5",
        "var n = 5; var f = 1; while (n > 0) { f = f * n; n = n - 1; } print f;",
        {120});

    return 0;
}
