//
// Disk unit for BESM-6.
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "machine.h"

//
// Open binary image as disk.
//
Disk::Disk(Memory &memory, const std::string &path, bool write_permit) :
    memory(memory),
    path(path),
    write_permit(write_permit)
{
    // Open file.
    int open_flag = write_permit ? O_RDWR : O_RDONLY;
    file_descriptor = open(path.c_str(), open_flag);
    if (file_descriptor < 0)
        throw std::runtime_error("Cannot open " + path +
                                 (write_permit ? " for write" : " for read"));

    // Get file size.
    struct stat stat;
    fstat(file_descriptor, &stat);
    num_zones = stat.st_size / DISK_ZONE_NWORDS;
}

// Close file in destructor.
Disk::~Disk()
{
    close(file_descriptor);
}

//
// Disk read: transfer data to memory.
//
void Disk::disk_to_memory(unsigned zone, unsigned sector, unsigned addr, unsigned nwords)
{
    Word *destination = memory.get_ptr(addr);
    unsigned offset_nwords = (DISK_ZONE_NWORDS * zone) +      // start of the zone record
                             8 +                              // skip OS info
                             (DISK_ZONE_NWORDS / 4 * sector); // sector offset

    if (lseek(file_descriptor, offset_nwords * sizeof(Word), SEEK_SET) < 0)
        throw std::runtime_error("Disk seek error");

    unsigned nbytes = nwords * sizeof(Word);
    if (read(file_descriptor, destination, nbytes) != nbytes)
        throw std::runtime_error("Disk read error");
}

//
// Disk write: transfer data from memory.
//
void Disk::memory_to_disk(unsigned zone, unsigned sector, unsigned addr, unsigned nwords)
{
    if (!write_permit)
        throw std::runtime_error("Cannot write to read-only disk");

    Word *source = memory.get_ptr(addr);
    unsigned offset_nwords = (DISK_ZONE_NWORDS * zone) +      // start of the zone record
                             8 +                              // skip OS info
                             (DISK_ZONE_NWORDS / 4 * sector); // sector offset

    if (lseek(file_descriptor, offset_nwords * sizeof(Word), SEEK_SET) < 0)
        throw std::runtime_error("Disk seek error");

    unsigned nbytes = nwords * sizeof(Word);
    if (write(file_descriptor, source, nbytes) != nbytes)
        throw std::runtime_error("Disk write error");
}
