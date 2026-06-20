#include "tracer.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <deque>

// ---- ANSI escape helpers (color + cursor control) -------------------
namespace ansi {
    const char* RESET = "\033[0m";
    const char* BOLD  = "\033[1m";
    const char* DIM   = "\033[2m";
    const char* RED   = "\033[31m";
    const char* GREEN = "\033[32m";
    const char* YELLOW= "\033[33m";
    const char* BLUE  = "\033[34m";
    const char* MAGENTA="\033[35m";
    const char* CYAN  = "\033[36m";
    const char* WHITE = "\033[37m";
    const char* BG_BLUE = "\033[44m";
    std::string home() { return "\033[H"; }        // cursor to top-left
    std::string clearScreen() { return "\033[2J\033[H"; }
}

static std::string regName(int i){ return "R" + std::to_string(i); }

// ---- disassemble a decoded instruction back to text -----------------
std::string disassemble(const Instruction& in) {
    auto R = [](int i){ return "R" + std::to_string(i); };
    std::ostringstream o;
    switch (in.op) {
        case Opcode::NOP:   o << "NOP"; break;
        case Opcode::LOADI: o << "LOADI " << R(in.rd) << ", " << +in.imm; break;
        case Opcode::LOAD:  o << "LOAD  " << R(in.rd) << ", [" << +in.imm << "]"; break;
        case Opcode::STORE: o << "STORE " << R(in.rd) << ", [" << +in.imm << "]"; break;
        case Opcode::MOV:   o << "MOV   " << R(in.rd) << ", " << R(in.rs); break;
        case Opcode::ADD:   o << "ADD   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::SUB:   o << "SUB   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::AND:   o << "AND   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::OR:    o << "OR    " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::XOR:   o << "XOR   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::NOT:   o << "NOT   " << R(in.rd) << ", " << R(in.rs); break;
        case Opcode::SHL:   o << "SHL   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::SHR:   o << "SHR   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::CMP:   o << "CMP   " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::JMP:   o << "JMP   " << in.addr; break;
        case Opcode::JZ:    o << "JZ    " << in.addr; break;
        case Opcode::JNZ:   o << "JNZ   " << in.addr; break;
        case Opcode::PUSH:  o << "PUSH  " << R(in.rd); break;
        case Opcode::POP:   o << "POP   " << R(in.rd); break;
        case Opcode::HALT:  o << "HALT"; break;
        case Opcode::MUL:   o << "MUL   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::DIV:   o << "DIV   " << R(in.rd) << ", " << R(in.rs) << ", " << R(in.rt); break;
        case Opcode::CALL:  o << "CALL  " << in.addr; break;
        case Opcode::RET:   o << "RET"; break;
        case Opcode::OUT:   o << "OUT   " << R(in.rd); break;
    }
    return o.str();
}

// keep a rolling log of recent notes across frames
static std::deque<std::string> g_log;

void renderFrame(const CPU& cpu, const TraceEvent& ev,
                 const std::vector<uint16_t>& program, bool clear) {
    std::ostringstream s;
    if (clear) s << ansi::clearScreen(); else s << "\n";

    // ---- title bar --------------------------------------------------
    s << ansi::BOLD << ansi::CYAN
      << "+==============================================================+\n"
      << "|                  16-BIT CPU EMULATOR  -  LIVE                |\n"
      << "+==============================================================+\n"
      << ansi::RESET;
    s << ansi::DIM << " cycle " << cpu.cycles()
      << "    PC=" << cpu.pc()
      << "    SP=" << cpu.sp() << ansi::RESET << "\n\n";

    // ---- registers --------------------------------------------------
    s << ansi::BOLD << " REGISTERS\n" << ansi::RESET;
    for (int i = 0; i < NUM_REGS; ++i) {
        bool touched = (ev.in.rd == i &&
                        ev.in.op != Opcode::CMP &&
                        ev.in.op != Opcode::STORE);
        s << "  " << (touched ? ansi::GREEN : ansi::WHITE)
          << std::left << std::setw(3) << regName(i) << "= "
          << std::right << std::setw(6) << cpu.reg(i)
          << "  0x" << std::hex << std::setw(4) << std::setfill('0')
          << cpu.reg(i) << std::dec << std::setfill(' ')
          << ansi::RESET;
        if (i % 2 == 1) s << "\n";
    }

    // ---- flags ------------------------------------------------------
    Flags f = cpu.flags();
    s << "\n " << ansi::BOLD << "FLAGS  " << ansi::RESET
      << (f.zero?ansi::GREEN:ansi::DIM)     << "Z" << ansi::RESET << " "
      << (f.negative?ansi::GREEN:ansi::DIM) << "N" << ansi::RESET << " "
      << (f.carry?ansi::GREEN:ansi::DIM)    << "C" << ansi::RESET << "\n\n";

    // ---- program with current line highlighted ----------------------
    s << ansi::BOLD << " PROGRAM\n" << ansi::RESET;
    int start = 0, end = (int)program.size();
    // window around PC so long programs don't flood the screen
    if (end > 14) {
        start = std::max(0, (int)cpu.pc() - 6);
        end   = std::min((int)program.size(), start + 14);
    }
    for (int a = start; a < end; ++a) {
        Instruction di = decode(program[a]);
        bool here = (a == ev.pc);
        s << (here ? std::string(ansi::BG_BLUE)+ansi::WHITE+ansi::BOLD : std::string(ansi::DIM))
          << (here ? " > " : "   ")
          << std::setw(3) << a << " | "
          << std::left << std::setw(22) << disassemble(di) << std::right
          << ansi::RESET << "\n";
    }

    // ---- recent activity log ---------------------------------------
    g_log.push_back(ev.note);
    while (g_log.size() > 5) g_log.pop_front();
    s << "\n" << ansi::BOLD << " ACTIVITY\n" << ansi::RESET;
    for (auto& line : g_log)
        s << ansi::YELLOW << "   " << line << ansi::RESET << "\n";

    std::cout << s.str() << std::flush;
}

void dumpTrace(const CPU& cpu, const std::string& path) {
    std::ofstream out(path);
    // CSV header. Python reads this to plot register history over time.
    out << "step,pc,opcode";
    for (int i = 0; i < NUM_REGS; ++i) out << ",R" << i;
    out << ",sp,zero,neg,carry\n";

    int step = 0;
    for (const auto& ev : cpu.trace()) {
        out << step++ << "," << ev.pc << ","
            << static_cast<int>(ev.in.op);
        for (int i = 0; i < NUM_REGS; ++i) out << "," << ev.regs[i];
        out << "," << ev.sp << ","
            << ev.flags.zero << "," << ev.flags.negative << ","
            << ev.flags.carry << "\n";
    }
}
