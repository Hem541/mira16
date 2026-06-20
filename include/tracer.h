#ifndef TRACER_H
#define TRACER_H

#include "cpu.h"
#include <string>

// live terminal dashboard + csv trace dump.

std::string disassemble(const Instruction& in);

// Draw the live dashboard. `clear` controls whether we repaint over the
// previous frame (animation) or just append (logging).
void renderFrame(const CPU& cpu, const TraceEvent& ev,
                 const std::vector<uint16_t>& program, bool clear);

// Write every recorded TraceEvent to a CSV the viz/trace_plot.py reads.
void dumpTrace(const CPU& cpu, const std::string& path);

#endif // TRACER_H
