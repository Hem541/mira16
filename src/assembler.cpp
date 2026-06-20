#include "assembler.h"
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>

// ---- small text helpers ---------------------------------------------
static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
// split on whitespace and commas
static std::vector<std::string> tokenize(const std::string& line) {
    std::string clean = line;
    for (char& c : clean) if (c == ',') c = ' ';
    std::istringstream iss(clean);
    std::vector<std::string> out; std::string t;
    while (iss >> t) out.push_back(t);
    return out;
}
// "R3" -> 3 ; returns -1 if not a register token
static int regNum(const std::string& tok) {
    if (tok.size() < 2 || (tok[0] != 'R' && tok[0] != 'r')) return -1;
    try { return std::stoi(tok.substr(1)); } catch (...) { return -1; }
}

// name -> opcode
static const std::unordered_map<std::string, Opcode> MNEMONICS = {
    {"NOP",Opcode::NOP},{"LOADI",Opcode::LOADI},{"LOAD",Opcode::LOAD},
    {"STORE",Opcode::STORE},{"MOV",Opcode::MOV},{"ADD",Opcode::ADD},
    {"SUB",Opcode::SUB},{"AND",Opcode::AND},{"OR",Opcode::OR},
    {"XOR",Opcode::XOR},{"NOT",Opcode::NOT},{"SHL",Opcode::SHL},
    {"SHR",Opcode::SHR},{"CMP",Opcode::CMP},{"JMP",Opcode::JMP},
    {"JZ",Opcode::JZ},{"JNZ",Opcode::JNZ},{"PUSH",Opcode::PUSH},
    {"POP",Opcode::POP},{"HALT",Opcode::HALT},
    {"MUL",Opcode::MUL},{"DIV",Opcode::DIV},
    {"CALL",Opcode::CALL},{"RET",Opcode::RET},{"OUT",Opcode::OUT}
};

// Strip a trailing ':' label at the start of a token list. Returns the
// label name (without colon) or "" and removes it from `toks`.
static std::string peelLabel(std::vector<std::string>& toks) {
    if (toks.empty()) return "";
    std::string& first = toks.front();
    if (!first.empty() && first.back() == ':') {
        std::string name = first.substr(0, first.size() - 1);
        toks.erase(toks.begin());
        return name;
    }
    return "";
}

AssembleResult assemble(const std::string& source) {
    AssembleResult res;

    // Break into raw lines, dropping comments (';' to end of line).
    std::vector<std::string> rawLines;
    {
        std::istringstream src(source);
        std::string line;
        while (std::getline(src, line)) {
            size_t semi = line.find(';');
            if (semi != std::string::npos) line = line.substr(0, semi);
            rawLines.push_back(line);
        }
    }

    // ---- PASS 1: assign an address to every instruction, map labels.
    std::unordered_map<std::string, uint16_t> labels;
    struct Pending { std::vector<std::string> toks; int srcLine; };
    std::vector<Pending> program;
    uint16_t addr = 0;

    for (size_t i = 0; i < rawLines.size(); ++i) {
        auto toks = tokenize(trim(rawLines[i]));
        if (toks.empty()) continue;

        // A line may begin with one or more labels (e.g. "start: loop:")
        std::string lbl;
        while (!(lbl = peelLabel(toks)).empty())
            labels[upper(lbl)] = addr;     // label points at next instr's addr

        if (toks.empty()) continue;        // line was label-only
        program.push_back({toks, (int)i + 1});
        ++addr;                            // each instruction = 1 word
    }

    // ---- PASS 2: encode each instruction, resolving label operands.
    auto resolve = [&](std::string tok, uint16_t& out) -> bool {
        // memory operands may be written [addr]; strip the brackets.
        if (!tok.empty() && tok.front() == '[') tok.erase(tok.begin());
        if (!tok.empty() && tok.back()  == ']') tok.pop_back();
        // numeric literal?
        try { out = (uint16_t)std::stoi(tok, nullptr, 0); return true; }
        catch (...) {}
        auto it = labels.find(upper(tok));   // else a label
        if (it != labels.end()) { out = it->second; return true; }
        return false;
    };

    for (auto& p : program) {
        const auto& toks = p.toks;
        std::string m = upper(toks[0]);
        auto it = MNEMONICS.find(m);
        if (it == MNEMONICS.end()) {
            res.errors.push_back("line " + std::to_string(p.srcLine) +
                                 ": unknown instruction '" + toks[0] + "'");
            res.code.push_back(0);
            continue;
        }
        Opcode op = it->second;
        Format f  = formatOf(op);
        uint16_t word = 0;

        auto fail = [&](const std::string& why){
            res.errors.push_back("line " + std::to_string(p.srcLine) + ": " + why);
        };

        if (f == Format::NONE) {                 // NOP, HALT
            word = encodeNone(op);
        }
        else if (f == Format::R) {               // OP rd, rs, rt
            if (op == Opcode::NOT || op == Opcode::MOV) {
                // two-register form: OP rd, rs
                int rd = regNum(toks.size()>1?toks[1]:"");
                int rs = regNum(toks.size()>2?toks[2]:"");
                if (rd<0||rs<0) fail("expected 'rd, rs'");
                word = encodeR(op, rd<0?0:rd, rs<0?0:rs, 0);
            } else {
                int rd = regNum(toks.size()>1?toks[1]:"");
                int rs = regNum(toks.size()>2?toks[2]:"");
                int rt = regNum(toks.size()>3?toks[3]:"");
                if (op==Opcode::CMP) {            // CMP rs, rt (no rd)
                    rt = regNum(toks.size()>2?toks[2]:"");
                    rs = regNum(toks.size()>1?toks[1]:"");
                    if (rs<0||rt<0) fail("expected 'rs, rt'");
                    word = encodeR(op, 0, rs<0?0:rs, rt<0?0:rt);
                } else {
                    if (rd<0||rs<0||rt<0) fail("expected 'rd, rs, rt'");
                    word = encodeR(op, rd<0?0:rd, rs<0?0:rs, rt<0?0:rt);
                }
            }
        }
        else if (f == Format::I) {               // OP rd, imm  (or PUSH/POP rd)
            int rd = regNum(toks.size()>1?toks[1]:"");
            if (rd < 0) fail("expected register as first operand");
            uint16_t imm = 0;
            if (op==Opcode::PUSH || op==Opcode::POP || op==Opcode::OUT) {
                imm = 0;                          // no immediate needed
            } else {
                if (toks.size()<3 || !resolve(toks[2], imm))
                    fail("expected immediate/address operand");
            }
            word = encodeI(op, rd<0?0:rd, (uint8_t)imm);
        }
        else if (f == Format::J) {               // OP addr/label
            uint16_t a = 0;
            if (toks.size()<2 || !resolve(toks[1], a))
                fail("expected jump target");
            word = encodeJ(op, a);
        }

        res.code.push_back(word);
    }

    return res;
}
