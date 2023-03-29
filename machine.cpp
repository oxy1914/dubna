//
// BESM-6: Big Electronic Calculating Machine, model 6.
//
// Copyright (c) 2023 Serge Vakulenko
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
#include "machine.h"

// Static fields.
bool Machine::verbose = false;
uint64_t Machine::simulated_instructions = 0;

// Limit of instructions, by default.
const uint64_t Machine::DEFAULT_LIMIT = 100ULL * 1000 * 1000 * 1000;

//
// Initialize the machine.
//
Machine::Machine(Memory &memory) :
    progress_time_last(std::chrono::steady_clock::now()),
    memory(memory),
    cpu(*this, memory)
{
}

//
// Deallocate the machine: disable tracing.
//
Machine::~Machine()
{
    redirect_trace(nullptr, "");
    enable_trace("");
}

//
// Every few seconds, print a message to stderr, to track the simulation progress.
//
void Machine::show_progress()
{
    //
    // Check the real time every few thousand cycles.
    //
    static const uint64_t PROGRESS_INCREMENT = 10000;

    if (simulated_instructions >= progress_count + PROGRESS_INCREMENT) {
        progress_count += PROGRESS_INCREMENT;

        // How much time has passed since the last check?
        auto time_now = std::chrono::steady_clock::now();
        auto delta = time_now - progress_time_last;
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(delta).count();

        // Emit message every 5 seconds.
        if (sec >= 5) {
            std::cerr << "----- Progress " << simulated_instructions << " -----" << std::endl;
            progress_time_last = time_now;
        }
    }
}

//
// Run the machine until completion.
//
void Machine::run()
{
    // Show initial state.
    trace_registers();

    try {
        for (;;) {
            bool done = cpu.step();

            if (progress_message_enabled) {
                show_progress();
            }

            if (done) {
                // Halted by 'стоп' instruction.
                return;
            }
        }

    } catch (const Processor::Exception &ex) {
        // Unexpected situation in the machine.
        cpu.stack_correction();

        auto const *message = ex.what();
        if (!message[0]) {
            // Empty message - legally halted by extracode e74.
            return;
        }
        std::cerr << "Error: " << message << std::endl;
        trace_exception(message);
        throw;

    } catch (std::exception &ex) {
        // Something else.
        std::cerr << "Error: " << ex.what() << std::endl;
        throw;
    }
}

//
// Fetch instruction word.
//
Word Machine::mem_fetch(unsigned addr)
{
    if (addr == 0) {
        throw Processor::Exception("Jump to zero");
    }

    Word val = memory.load(addr);

    if (!cpu.on_right_instruction()) {
        trace_fetch(addr, val);
    }
    return val & BITS48;
}

//
// Write word to memory.
//
void Machine::mem_store(unsigned addr, Word val)
{
    addr &= BITS(15);
    if (addr == 0)
        return;

    memory.store(addr, val);
    trace_memory_write(addr, val);
}

//
// Read word from memory.
//
Word Machine::mem_load(unsigned addr)
{
    addr &= BITS(15);
    if (addr == 0)
        return 0;

    Word val = memory.load(addr);
    trace_memory_read(addr, val);

    return val & BITS48;
}

//
// Load input file.
// Throw exception on failure.
//
void Machine::load(const std::string &filename)
{
    //TODO: load job file
}

//
// Load input job from stream.
//
void Machine::load(std::istream &input)
{
    //TODO: load job from stream
}

//
// Disk read/write.
//
void Machine::disk_io(char op, unsigned disk_unit, unsigned zone, unsigned sector, unsigned addr, unsigned nwords)
{
    if (disk_unit >= NDRUMS)
        throw std::runtime_error("Invalid disk unit " + to_octal(disk_unit));

    if (!disks[disk_unit]) {
        // Disk must be previously configured using disk_mount().
        throw std::runtime_error("Disk unit " + to_octal(disk_unit + 030) + " is not mounted");
    }

    if (op == 'r') {
        disks[disk_unit]->disk_to_memory(zone, sector, addr, nwords);

        // Debug: dump the data.
        if (dump_io_flag) {
            memory.dump(++dump_serial_num, disk_unit + 030, zone, sector, addr, nwords);
        }
    } else {
        disks[disk_unit]->memory_to_disk(zone, sector, addr, nwords);
    }
}

//
// Drum read/write.
//
void Machine::drum_io(char op, unsigned drum_unit, unsigned zone, unsigned sector, unsigned addr, unsigned nwords)
{
    if (drum_unit >= NDRUMS)
        throw std::runtime_error("Invalid drum unit " + to_octal(drum_unit));

    if (!drums[drum_unit]) {
        // Allocate new drum on first request.
        drums[drum_unit] = std::make_unique<Drum>(memory);
    }

    if (op == 'r') {
        drums[drum_unit]->drum_to_memory(zone, sector, addr, nwords);
    } else {
        drums[drum_unit]->memory_to_drum(zone, sector, addr, nwords);
    }
}

//
// Open binary image and assign it to the disk unit.
//
void Machine::disk_mount(unsigned disk_unit, const std::string &filename, bool write_permit)
{
    if (disk_unit < 030 || disk_unit >= 070)
        throw std::runtime_error("Invalid disk unit " + to_octal(disk_unit) + " in disk_mount()");

    disk_unit -= 030;
    if (disks[disk_unit])
        throw std::runtime_error("Disk unit " + to_octal(disk_unit + 030) + " is already mounted");

    // Open binary image as disk.
    disks[disk_unit] = std::make_unique<Disk>(memory, filename, write_permit);
}
