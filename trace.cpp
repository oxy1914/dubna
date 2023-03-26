//
// Instruction and register tracing.
//
// Copyright (c) 2022-2023 Leonid Broukhis, Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <iostream>
#include <fstream>
#include "machine.h"

//
// Flag to enable tracing.
//
bool Machine::trace_instructions; // trace machine instuctions
bool Machine::trace_extracodes;   // trace extracodes (except e75)
bool Machine::trace_registers;    // trace CPU registers
bool Machine::trace_memory;       // trace memory read/write
bool Machine::trace_fetch;        // trace instruction fetch
bool Machine::trace_exceptions;   // trace exceptions

//
// Emit trace to this stream.
//
std::ofstream Machine::trace_stream;

//
// Enable trace with given modes.
//  i - trace instructions
//  e - trace extracodes
//  f - trace fetch
//  r - trace registers
//  m - trace memory read/write
//  x - trace exceptions
//
void Machine::enable_trace(const char *trace_mode)
{
    // Disable all trace options.
    trace_instructions = false;
    trace_extracodes = false;
    trace_registers = false;
    trace_memory = false;
    trace_fetch = false;
    trace_exceptions = false;

    if (trace_mode) {
        // Parse the mode string and enable all requested trace flags.
        for (unsigned i = 0; trace_mode[i]; i++) {
            char ch = trace_mode[i];
            switch (ch) {
            case 'i': trace_instructions = true; break;
            case 'e': trace_extracodes = true; break;
            case 'f': trace_fetch = true; break;
            case 'm': trace_memory = true; break;
            case 'x': trace_exceptions = true; break;
            case 'r': trace_registers = true; break;
            default:
                throw std::runtime_error("Wrong trace option: " + std::string(1, ch));
            }
        }
    }
}

//
// Redirect trace output to a given file.
//
void Machine::redirect_trace(const char *file_name, const char *default_mode)
{
    if (trace_stream.is_open()) {
        // Close previous file.
        trace_stream.close();
    }
    if (file_name && file_name[0]) {
        // Open new trace file.
        trace_stream.open(file_name);
        if (!trace_stream.is_open())
            throw std::runtime_error("Cannot write to " + std::string(file_name));
    }

    if (!trace_enabled()) {
        // Set default mode.
        enable_trace(default_mode);
    }
}

std::ostream &Machine::get_trace_stream()
{
    if (trace_stream.is_open()) {
        return trace_stream;
    }
    return std::cout;
}

void Machine::close_trace()
{
    if (trace_stream.is_open()) {
        // Close output.
        trace_stream.close();
    }

    // Disable trace options.
    enable_trace("");
}

#if 0
#include "el_svs_internal.h"

void svs_trace_opcode(struct ElSvsProcessor *cpu, int paddr)
{
    // Print instruction.
    fprintf(cpu->log_output, "cpu%d %05o %07o %c: ",
        cpu->index, cpu->core.PC, paddr,
        (cpu->core.RUU & RUU_RIGHT_INSTR) ? 'R' : 'L');
    svs_fprint_insn(cpu->log_output, cpu->RK);
    fprintf(cpu->log_output, " ");
    svs_fprint_cmd(cpu->log_output, cpu->RK);
    fprintf(cpu->log_output, "\n");
}

//
// Print 32-bit value as octal.
//
static void fprint_32bits(FILE *of, uint64_t value)
{
    fprintf(of, "%03o %04o %04o",
        (int) (value >> 24) & 0377,
        (int) (value >> 12) & 07777,
        (int) value & 07777);
}

//
// Print 48-bit value as octal.
//
void svs_fprint_48bits(FILE *of, uint64_t value)
{
    fprintf(of, "%04o %04o %04o %04o",
        (int) (value >> 36) & 07777,
        (int) (value >> 24) & 07777,
        (int) (value >> 12) & 07777,
        (int) value & 07777);
}

//
// Печать регистров процессора, изменившихся с прошлого вызова.
//
void svs_trace_registers(struct ElSvsProcessor *cpu)
{
    int i;

    if (cpu->core.ACC != cpu->prev.ACC) {
        fprintf(cpu->log_output, "cpu%d       Write ACC = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.ACC);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.RMR != cpu->prev.RMR) {
        fprintf(cpu->log_output, "cpu%d       Write RMR = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.RMR);
        fprintf(cpu->log_output, "\n");
    }
    for (i = 0; i < SVS_NREGS; i++) {
        if (cpu->core.M[i] != cpu->prev.M[i])
            fprintf(cpu->log_output, "cpu%d       Write M%o = %05o\n",
                cpu->index, i, cpu->core.M[i]);
    }
    if (cpu->core.RAU != cpu->prev.RAU)
        fprintf(cpu->log_output, "cpu%d       Write RAU = %02o\n",
            cpu->index, cpu->core.RAU);
    if ((cpu->core.RUU & ~RUU_RIGHT_INSTR) != (cpu->prev.RUU & ~RUU_RIGHT_INSTR))
        fprintf(cpu->log_output, "cpu%d       Write RUU = %03o\n",
            cpu->index, cpu->core.RUU);
    for (i = 0; i < 8; i++) {
        if (cpu->core.RP[i] != cpu->prev.RP[i]) {
            fprintf(cpu->log_output, "cpu%d       Write RP%o = ",
                cpu->index, i);
            svs_fprint_48bits(cpu->log_output, cpu->core.RP[i]);
            fprintf(cpu->log_output, "\n");
        }
        if (cpu->core.RPS[i] != cpu->prev.RPS[i]) {
            fprintf(cpu->log_output, "cpu%d       Write RPS%o = ",
                cpu->index, i);
            svs_fprint_48bits(cpu->log_output, cpu->core.RPS[i]);
            fprintf(cpu->log_output, "\n");
        }
    }
    if (cpu->core.RZ != cpu->prev.RZ) {
        fprintf(cpu->log_output, "cpu%d       Write RZ = ", cpu->index);
        fprint_32bits(cpu->log_output, cpu->core.RZ);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.bad_addr != cpu->prev.bad_addr) {
        fprintf(cpu->log_output, "cpu%d       Write EADDR = %03o\n",
            cpu->index, cpu->core.bad_addr);
    }
    if (cpu->core.TagR != cpu->prev.TagR) {
        fprintf(cpu->log_output, "cpu%d       Write TAG = %03o\n",
            cpu->index, cpu->core.TagR);
    }
    if (cpu->core.PP != cpu->prev.PP) {
        fprintf(cpu->log_output, "cpu%d       Write PP = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.PP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.OPP != cpu->prev.OPP) {
        fprintf(cpu->log_output, "cpu%d       Write OPP = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.OPP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.POP != cpu->prev.POP) {
        fprintf(cpu->log_output, "cpu%d       Write POP = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.POP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.OPOP != cpu->prev.OPOP) {
        fprintf(cpu->log_output, "cpu%d       Write OPOP = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.OPOP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.RKP != cpu->prev.RKP) {
        fprintf(cpu->log_output, "cpu%d       Write RKP = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.RKP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.RPR != cpu->prev.RPR) {
        fprintf(cpu->log_output, "cpu%d       Write RPR = ", cpu->index);
        svs_fprint_48bits(cpu->log_output, cpu->core.RPR);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.GRVP != cpu->prev.GRVP) {
        fprintf(cpu->log_output, "cpu%d       Write GRVP = ", cpu->index);
        fprint_32bits(cpu->log_output, cpu->core.GRVP);
        fprintf(cpu->log_output, "\n");
    }
    if (cpu->core.GRM != cpu->prev.GRM) {
        fprintf(cpu->log_output, "cpu%d       Write GRM = ", cpu->index);
        fprint_32bits(cpu->log_output, cpu->core.GRM);
        fprintf(cpu->log_output, "\n");
    }

    cpu->prev = cpu->core;
}
#endif
