#include "membenchlib.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <system_error>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

namespace {

bool trace_mmio()
{
    return std::getenv("MEMBENCH_TRACE_MMIO") != nullptr;
}

void write8(void* base, uint32_t offset, uint8_t value)
{
    if (trace_mmio()) {
        fprintf(stderr, "MMIO write8 +0x%02x = 0x%02x: begin\n", offset, value);
        fflush(stderr);
    }
    WRITE_UINT8(reinterpret_cast<uintptr_t>(base) + offset, value);
    if (trace_mmio()) fprintf(stderr, "MMIO write8 +0x%02x: done\n", offset);
}

void write32(void* base, uint32_t offset, uint32_t value)
{
    if (trace_mmio()) {
        fprintf(stderr, "MMIO write32 +0x%02x = 0x%08x: begin\n", offset, value);
        fflush(stderr);
    }
    WRITE_UINT32(reinterpret_cast<uintptr_t>(base) + offset, value);
    if (trace_mmio()) fprintf(stderr, "MMIO write32 +0x%02x: done\n", offset);
}

} // namespace

membench::MembenchLib::MembenchLib()
{
    if (trace_mmio()) fprintf(stderr, "Opening /dev/mem: begin\n");
    fd_DevMem = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_DevMem == -1)
        throw std::system_error(errno, std::generic_category(), "open /dev/mem");
    if (trace_mmio()) fprintf(stderr, "Opening /dev/mem: done; mapping 0x%x: begin\n", MEMBENCH_CTRL_BASE);
    ctrl =
            mmap(
                nullptr,
                0x1000,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd_DevMem,
                MEMBENCH_CTRL_BASE);
    if (ctrl == MAP_FAILED) {
        const int error = errno;
        close(fd_DevMem);
        throw std::system_error(error, std::generic_category(), "mmap membench control");
    }
    if (trace_mmio()) fprintf(stderr, "Mapping 0x%x: done\n", MEMBENCH_CTRL_BASE);
}

membench::MembenchLib::~MembenchLib()
{
    munmap(ctrl, 0x1000);
    close(fd_DevMem);
}

void membench::MembenchLib::SetupStridedAccess(uint32_t req_count, bool write, uint32_t stride, uint8_t mlp)
{
    write8(ctrl, MEMBENCH_CTRL_ACCESSPATTERN, ACCESS_PATTERN::STRIDED);
    write32(ctrl, MEMBENCH_CTRL_STRIDE, stride);
    write8(ctrl, MEMBENCH_CTRL_DOWRITE, write);
    write8(ctrl, MEMBENCH_CTRL_SETMLP, mlp);
    write32(ctrl, MEMBENCH_CTRL_REQCOUNT, req_count);
}

void membench::MembenchLib::SetupLinearAccess(uint32_t req_count, uint8_t mlp, bool write)
{
    write8(ctrl, MEMBENCH_CTRL_ACCESSPATTERN, ACCESS_PATTERN::LINEAR);
    write8(ctrl, MEMBENCH_CTRL_SETMLP, mlp);
    write32(ctrl, MEMBENCH_CTRL_REQCOUNT, req_count);
    write8(ctrl, MEMBENCH_CTRL_DOWRITE, write);
}

void membench::MembenchLib::SetupRandomAccess(uint32_t req_count, uint8_t mlp, bool write)
{
    write8(ctrl, MEMBENCH_CTRL_ACCESSPATTERN, ACCESS_PATTERN::RANDOM);
    write8(ctrl, MEMBENCH_CTRL_SETMLP, mlp);
    write32(ctrl, MEMBENCH_CTRL_REQCOUNT, req_count);
    write8(ctrl, MEMBENCH_CTRL_DOWRITE, write);
}


/*
    Bank mask needs to mask all bank bits

    Row bit start gives the position where rows are differentiated

    Bank designates the bank to access
*/
void membench::MembenchLib::SetupBankAccess(uint32_t req_count, uint32_t bank_mask, uint8_t bank_set_mask, uint8_t row_bit_start, uint8_t mlp, bool write)
{
    uint32_t row_mask = 0;
    for (int i = row_bit_start; i < 32; i++)
        row_mask |= (1u << i);
    uint32_t unset_bits = bank_mask | row_mask;

    write8(ctrl, MEMBENCH_CTRL_ACCESSPATTERN, ACCESS_PATTERN::BANK);
    write8(ctrl, MEMBENCH_CTRL_SETMLP, mlp);
    write32(ctrl, MEMBENCH_CTRL_REQCOUNT, req_count);
    write8(ctrl, MEMBENCH_CTRL_DOWRITE, write);


    write32(ctrl, MEMBENCH_CTRL_MASKSET, bank_set_mask);
    write32(ctrl, MEMBENCH_CTRL_MASKUNSET, unset_bits);
    write8(ctrl, MEMBENCH_CTRL_ROWBITSTART, row_bit_start);

}

void membench::MembenchLib::Start()
{
    write8(ctrl, MEMBENCH_CTRL_ACTIVE, true);
}

void membench::MembenchLib::Reset()
{
    write8(ctrl, MEMBENCH_CTRL_RST, true);
}

uint32_t membench::MembenchLib::WaitUntilDone(bool print_progress, uint32_t timeout_seconds)
{
    const auto started = std::chrono::steady_clock::now();
    uint32_t last_reported = 0;
    uint32_t tmp = 0;
    bool reported = false;
    while ((tmp = READ_UINT32(reinterpret_cast<uint64_t>(ctrl) + MEMBENCH_CTRL_REQCOUNT)) != 0) {
        if (print_progress && (!reported || (tmp < last_reported && last_reported - tmp >= 100000)))
        {
            printf("Requests remaining %u\n", tmp);
            fflush(stdout);
            last_reported = tmp;
            reported = true;
        }
        if (timeout_seconds != 0 &&
            std::chrono::steady_clock::now() - started >= std::chrono::seconds(timeout_seconds))
            return tmp;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}
