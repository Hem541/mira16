#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "isa.h"
#include <string>
#include <vector>

// assembly text -> machine words. two passes for labels.

struct AssembleResult {
    std::vector<uint16_t> code;   // the machine code
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

AssembleResult assemble(const std::string& source);

#endif // ASSEMBLER_H
