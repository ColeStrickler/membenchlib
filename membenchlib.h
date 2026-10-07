#ifndef MEMBENCH_LIB_HPP
#define MEMBENCH_LIB_HPP
#include <stdint.h>
#include <stdio.h>

#ifndef ACCESS_HELPERS
#define ACCESS_HELPERS
#define WRITE_BOOL(addr, value)(*(volatile bool*)(addr) = value)
#define WRITE_UINT8(addr, value)(*(volatile uint8_t*)(addr) = value)
#define WRITE_UINT16(addr, value)(*(volatile uint16_t*)(addr) = value)
#define WRITE_UINT32(addr, value)(*(volatile uint32_t*)(addr) = value)
#define WRITE_UINT64(addr, value)(*(volatile uint64_t*)(addr) = value)

#define READ_BOOL(addr)(*(volatile bool*)(addr))
#define READ_UINT8(addr)(*(volatile uint8_t*)(addr))
#define READ_UINT16(addr)(*(volatile uint16_t*)(addr))
#define READ_UINT32(addr)(*(volatile uint32_t*)(addr))
#define READ_UINT64(addr)(*(volatile uint64_t*)(addr))
#endif

namespace membench
{
/*
    MMIO Control
*/

#define MEMBENCH_CTRL_BASE 0x500000
#define MEMBENCH_CTRL_ACCESSPATTERN 0x0
#define MEMBENCH_CTRL_RST 0x8
#define MEMBENCH_CTRL_ACTIVE 0x10
#define MEMBENCH_CTRL_STRIDE 0x18
#define MEMBENCH_CTRL_REQCOUNT 0x20
#define MEMBENCH_CTRL_DOWRITE 0x28
#define MEMBENCH_CTRL_SETMLP 0x30
#define MEMBENCH_CTRL_MASKSET 0x38
#define MEMBENCH_CTRL_MASKUNSET 0x40
#define MEMBENCH_CTRL_ROWBITSTART 0x48


enum ACCESS_PATTERN : uint8_t
{
    LINEAR,
    STRIDED,
    RANDOM,
    BANK
};



class MembenchLib
{
public:
    MembenchLib();
    ~MembenchLib();
    void SetupStridedAccess(uint32_t req_count, bool write, uint32_t stride, uint8_t mlp);
    void SetupLinearAccess(uint32_t req_count, uint8_t mlp, bool write);
    void SetupRandomAccess(uint32_t req_count, uint8_t mlp, bool write);
    void SetupBankAccess(uint32_t req_count, uint32_t bank_mask, uint32_t bank_set_mask, uint8_t row_bit_start, uint8_t mlp, bool write);
    void Start();
    void Reset();
    uint32_t WaitUntilDone(bool print_progress, uint32_t timeout_seconds, uint32_t reqCount);
private:
    int fd_DevMem;
    void* ctrl;

    
};



}


#endif
