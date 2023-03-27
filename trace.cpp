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
bool Machine::debug_instructions; // trace machine instuctions
bool Machine::debug_extracodes;   // trace extracodes (except e75)
bool Machine::debug_registers;    // trace CPU registers
bool Machine::debug_memory;       // trace memory read/write
bool Machine::debug_fetch;        // trace instruction fetch

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
    debug_instructions = false;
    debug_extracodes = false;
    debug_registers = false;
    debug_memory = false;
    debug_fetch = false;

    if (trace_mode) {
        // Parse the mode string and enable all requested trace flags.
        for (unsigned i = 0; trace_mode[i]; i++) {
            char ch = trace_mode[i];
            switch (ch) {
            case 'i': debug_instructions = true; break;
            case 'e': debug_extracodes = true; break;
            case 'f': debug_fetch = true; break;
            case 'm': debug_memory = true; break;
            case 'r': debug_registers = true; break;
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

//
// Trace output
//
void Machine::print_exception(const char *message)
{
    auto &out = Machine::get_trace_stream();
    out << "--- " << message << std::endl;
}

//
// Print instruction fetch.
//
void Machine::print_fetch(unsigned addr, Word val)
{
    auto &out = Machine::get_trace_stream();
    auto save_flags = out.flags();

    out << "      Fetch [" << std::oct
        << std::setfill('0') << std::setw(5) << addr << "] = ";
    besm6_print_instruction_octal(out, (val >> 24) & BITS(24));
    out << ' ';
    besm6_print_instruction_octal(out, val & BITS(24));
    out << std::endl;

    // Restore.
    out.flags(save_flags);
}

//
// Print memory read/write.
//
void Machine::print_memory_access(unsigned addr, Word val, const char *opname)
{
    auto &out = Machine::get_trace_stream();
    auto save_flags = out.flags();

    out << "      Memory " << opname << " [" << std::oct
        << std::setfill('0') << std::setw(5) << addr << "] = ";
    besm6_print_word_octal(out, val);
    out << std::endl;

    // Restore.
    out.flags(save_flags);
}

//
// Print instruction address, opcode from RK and mnemonics.
//
void Processor::print_instruction()
{
    auto &out = Machine::get_trace_stream();
    auto save_flags = out.flags();

    out << std::oct << std::setfill('0') << std::setw(5) << core.PC
        << ' '  << (core.right_instr_flag ? 'R' : 'L') << ": ";
    besm6_print_instruction_octal(out, RK);
    out << ' ';
    besm6_print_instruction_mnemonics(out, RK);
    out << std::endl;

    // Restore.
    out.flags(save_flags);
}

//
// Print changes in CPU registers.
//
void Processor::print_registers()
{
    auto &out = Machine::get_trace_stream();
    auto save_flags = out.flags();

    if (core.ACC != prev.ACC) {
        out << "      Write ACC = ";
        besm6_print_word_octal(out, core.ACC);
        out << std::endl;
    }
    if (core.RMR != prev.RMR) {
        out << "      Write RMR = ";
        besm6_print_word_octal(out, core.RMR);
        out << std::endl;
    }
    for (unsigned i = 0; i < 16; i++) {
        if (core.M[i] != prev.M[i]) {
            out << "      Write M" << std::oct << i << " = "
                << std::setfill('0') << std::setw(5) << core.M[i] << std::endl;
        }
    }
    if (core.RAU != prev.RAU) {
        out << "      Write RAU = " << std::oct
            << std::setfill('0') << std::setw(2) << core.RAU << std::endl;
    }
    if (core.apply_mod_reg != prev.apply_mod_reg) {
        if (core.apply_mod_reg) {
            out << "      Write MOD = " << std::oct
            << std::setfill('0') << std::setw(5) << core.MOD;
        } else {
            out << "      Clear MOD";
        }
        out << std::endl;
    }

    // Update previous state.
    prev = core;

    // Restore output flags.
    out.flags(save_flags);
}
