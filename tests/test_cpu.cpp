#include "cpu.h"
#include "assembler.h"
#include "isa.h"
#include <iostream>
#include <cassert>

// scalar cpu tests.

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << "\n"; ++failures; } \
    else         { std::cout << "ok  : " << msg << "\n"; } \
} while (0)

// run source, return the CPU for inspection
static CPU runSrc(const std::string& src) {
    auto a = assemble(src);
    assert(a.ok());
    CPU cpu; cpu.enableTrace(false);
    cpu.loadProgram(a.code);
    cpu.run();
    return cpu;
}

int main() {
    // ---- encode/decode round trip ----------------------------------
    {
        uint16_t w = encodeR(Opcode::ADD, 1, 2, 3);
        Instruction in = decode(w);
        CHECK(in.op==Opcode::ADD && in.rd==1 && in.rs==2 && in.rt==3,
              "ADD encodes/decodes round-trip");
    }

    // ---- arithmetic -------------------------------------------------
    CHECK(runSrc("LOADI R1,5\nLOADI R2,3\nADD R3,R1,R2\nHALT").reg(3)==8,
          "5 + 3 = 8");
    CHECK(runSrc("LOADI R1,10\nLOADI R2,4\nSUB R3,R1,R2\nHALT").reg(3)==6,
          "10 - 4 = 6");
    CHECK(runSrc("LOADI R1,6\nLOADI R2,7\nMUL R3,R1,R2\nHALT").reg(3)==42,
          "6 * 7 = 42");
    CHECK(runSrc("LOADI R1,20\nLOADI R2,5\nDIV R3,R1,R2\nHALT").reg(3)==4,
          "20 / 5 = 4");

    // ---- divide by zero sets carry, result 0 -----------------------
    {
        CPU c = runSrc("LOADI R1,20\nLOADI R2,0\nDIV R3,R1,R2\nHALT");
        CHECK(c.reg(3)==0 && c.flags().carry, "div by zero -> 0, carry set");
    }

    // ---- logic ------------------------------------------------------
    CHECK(runSrc("LOADI R1,12\nLOADI R2,10\nAND R3,R1,R2\nHALT").reg(3)==(12&10),
          "12 & 10 bitwise");
    CHECK(runSrc("LOADI R1,1\nLOADI R2,4\nSHL R3,R1,R2\nHALT").reg(3)==16,
          "1 << 4 = 16");

    // ---- R0 is hardwired to zero -----------------------------------
    CHECK(runSrc("LOADI R0,99\nHALT").reg(0)==0,
          "writes to R0 are dropped (stays 0)");

    // ---- memory store/load -----------------------------------------
    {
        CPU c = runSrc("LOADI R1,77\nSTORE R1,[50]\nLOAD R2,[50]\nHALT");
        CHECK(c.reg(2)==77, "store then load round-trips through memory");
    }

    // ---- stack push/pop --------------------------------------------
    {
        CPU c = runSrc("LOADI R1,5\nPUSH R1\nLOADI R1,0\nPOP R2\nHALT");
        CHECK(c.reg(2)==5, "push 5 then pop -> 5");
    }

    // ---- conditional loop (multiply by adding) ---------------------
    CHECK(runSrc(
        "LOADI R1,6\nLOADI R2,7\nLOADI R3,0\nLOADI R6,1\nLOADI R7,0\n"
        "loop: CMP R2,R7\nJZ done\nADD R3,R3,R1\nSUB R2,R2,R6\nJMP loop\n"
        "done: HALT").reg(3)==42,
        "loop: 6*7 via repeated addition = 42");

    // ---- subroutine CALL/RET + GCD ---------------------------------
    {
        CPU c = runSrc(
            "LOADI R1,48\nLOADI R2,36\nLOADI R7,0\nCALL gcd\nHALT\n"
            "gcd: CMP R2,R7\nJZ gd\nDIV R3,R1,R2\nMUL R4,R3,R2\n"
            "SUB R5,R1,R4\nMOV R1,R2\nMOV R2,R5\nJMP gcd\ngd: RET");
        CHECK(c.reg(1)==12, "gcd(48,36) = 12 via CALL/RET subroutine");
    }

    std::cout << "\n" << (failures==0 ? "ALL TESTS PASSED" :
                          std::to_string(failures)+" TEST(S) FAILED") << "\n";
    return failures==0 ? 0 : 1;
}
