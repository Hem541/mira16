#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>

// minic -> isa assembly.

struct CompileResult {
    std::string asmText;              // the generated assembly
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// Compile MiniC source into assembly text for our ISA.
CompileResult compile(const std::string& source);

#endif // COMPILER_H
