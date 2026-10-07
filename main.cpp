#include "membenchlib.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <string>

namespace {

enum Option {
    kRead = 256,
    kStride,
    kBankMask,
    kBankSetMask,
    kRowBitStart,
    kReset,
    kResetOnly,
    kNoWait,
    kTimeoutSeconds,
};

void print_usage(const char* program, std::ostream& out)
{
    out << "Usage: " << program << " --pattern PATTERN --requests N [options]\n"
        << "       " << program << " --reset-only\n\n"
        << "Patterns: linear, strided, random, bank\n\n"
        << "  -p, --pattern PATTERN       Access pattern\n"
        << "  -n, --requests N            Number of requests (1..4294967295)\n"
        << "  -m, --mlp N                 Memory-level parallelism (1..255; default: 1)\n"
        << "  -w, --write                 Write requests (default: read)\n"
        << "      --read                  Read requests\n"
        << "      --stride N              Stride for strided access\n"
        << "      --bank-mask N           Bank bit mask for bank access\n"
        << "      --bank-set-mask N       Bank selection bits for bank access\n"
        << "      --row-bit-start N       First row bit for bank access (0..27)\n"
        << "      --reset                 Reset before configuring the run\n"
        << "      --reset-only            Reset without starting a run\n"
        << "      --no-wait               Return immediately after starting\n"
        << "      --timeout-seconds N     Stop waiting after N seconds (default: 30; 0 disables)\n"
        << "      --progress              Print progress while waiting\n"
        << "  -h, --help                  Show this help\n\n"
        << "Numbers may be decimal or prefixed with 0x for hexadecimal.\n";
}

bool parse_number(const char* value, uint64_t maximum, uint64_t& result)
{
    if (!value[0] || value[0] == '-' || value[0] == '+') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const int base = value[0] == '0' && (value[1] == 'x' || value[1] == 'X') ? 16 : 10;
    const unsigned long long parsed = std::strtoull(value, &end, base);
    if (errno == ERANGE || *end != '\0' || parsed > maximum) {
        return false;
    }
    result = parsed;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    static const option options[] = {
        {"pattern", required_argument, nullptr, 'p'},
        {"requests", required_argument, nullptr, 'n'},
        {"mlp", required_argument, nullptr, 'm'},
        {"write", no_argument, nullptr, 'w'},
        {"read", no_argument, nullptr, kRead},
        {"stride", required_argument, nullptr, kStride},
        {"bank-mask", required_argument, nullptr, kBankMask},
        {"bank-set-mask", required_argument, nullptr, kBankSetMask},
        {"row-bit-start", required_argument, nullptr, kRowBitStart},
        {"reset", no_argument, nullptr, kReset},
        {"reset-only", no_argument, nullptr, kResetOnly},
        {"no-wait", no_argument, nullptr, kNoWait},
        {"timeout-seconds", required_argument, nullptr, kTimeoutSeconds},
        {"progress", no_argument, nullptr, 'P'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    std::string pattern;
    uint64_t requests = 0;
    uint64_t mlp = 1;
    uint64_t stride = 0;
    uint64_t bank_mask = 0;
    uint64_t bank_set_mask = 0;
    uint64_t row_bit_start = 0;
    uint64_t timeout_seconds = 30;
    bool requests_given = false;
    bool mlp_given = false;
    bool access_given = false;
    bool timeout_given = false;
    bool stride_given = false;
    bool bank_mask_given = false;
    bool bank_set_mask_given = false;
    bool row_bit_start_given = false;
    bool write = false;
    bool reset = false;
    bool reset_only = false;
    bool wait = true;
    bool progress = false;

    opterr = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "p:n:m:wPh", options, nullptr)) != -1) {
        uint64_t* number = nullptr;
        uint64_t maximum = std::numeric_limits<uint32_t>::max();
        const char* name = nullptr;
        switch (opt) {
        case 'p': pattern = optarg; break;
        case 'n': number = &requests; name = "requests"; requests_given = true; break;
        case 'm': number = &mlp; maximum = UINT8_MAX; name = "mlp"; mlp_given = true; break;
        case 'w': write = true; access_given = true; break;
        case kRead: write = false; access_given = true; break;
        case kStride: number = &stride; name = "stride"; stride_given = true; break;
        case kBankMask: number = &bank_mask; name = "bank-mask"; bank_mask_given = true; break;
        case kBankSetMask:
            number = &bank_set_mask;
            name = "bank-set-mask"; bank_set_mask_given = true; break;
        case kRowBitStart:
            number = &row_bit_start; maximum = 27;
            name = "row-bit-start"; row_bit_start_given = true; break;
        case kReset: reset = true; break;
        case kResetOnly: reset_only = true; break;
        case kNoWait: wait = false; break;
        case kTimeoutSeconds:
            number = &timeout_seconds; maximum = 86400;
            name = "timeout-seconds"; timeout_given = true; break;
        case 'P': progress = true; break;
        case 'h': print_usage(argv[0], std::cout); return 0;
        default:
            std::cerr << "Unknown or incomplete option\n";
            print_usage(argv[0], std::cerr);
            return 2;
        }
        if (number && !parse_number(optarg, maximum, *number)) {
            std::cerr << "Invalid value for --" << name << ": " << optarg << '\n';
            return 2;
        }
    }

    if (optind != argc) {
        std::cerr << "Unexpected argument: " << argv[optind] << '\n';
        return 2;
    }
    if (reset_only) {
        if (!pattern.empty() || requests_given || mlp_given || access_given || stride_given ||
            bank_mask_given || bank_set_mask_given || row_bit_start_given || timeout_given || progress || !wait) {
            std::cerr << "--reset-only cannot be combined with run options\n";
            return 2;
        }
        try {
            membench::MembenchLib bench;
            bench.Reset();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }
    if (pattern != "linear" && pattern != "strided" && pattern != "random" && pattern != "bank") {
        std::cerr << "--pattern must be linear, strided, random, or bank\n";
        return 2;
    }
    if (!requests_given || requests == 0 || mlp == 0) {
        std::cerr << "--requests and --mlp must be greater than zero\n";
        return 2;
    }
    if (pattern == "strided" ? (!stride_given || stride == 0) : stride_given) {
        std::cerr << "--stride is required only for strided access and must be greater than zero\n";
        return 2;
    }
    const bool bank_option_given = bank_mask_given || bank_set_mask_given || row_bit_start_given;
    if (pattern == "bank" ? (!bank_mask_given || !bank_set_mask_given || !row_bit_start_given || bank_mask == 0)
                          : bank_option_given) {
        std::cerr << "--bank-mask, --bank-set-mask, and --row-bit-start are required only for bank access; bank mask must be nonzero\n";
        return 2;
    }
    if (pattern == "bank" && ((bank_set_mask & ~bank_mask) != 0 || (bank_mask & 0x3f) != 0 ||
                              (bank_mask >> row_bit_start) != 0)) {
        std::cerr << "bank selection bits must be within bank-mask; bank bits must be between cache-line and row bits\n";
        return 2;
    }
    if ((progress || timeout_given) && !wait) {
        std::cerr << "--progress and --timeout-seconds require waiting for completion\n";
        return 2;
    }

    try {
        std::cerr << "Configuring " << pattern << " benchmark (" << requests << " requests)\n";
        membench::MembenchLib bench;
        if (reset) bench.Reset();
        if (pattern == "linear") {
            bench.SetupLinearAccess(static_cast<uint32_t>(requests), static_cast<uint8_t>(mlp), write);
        } else if (pattern == "strided") {
            bench.SetupStridedAccess(static_cast<uint32_t>(requests), write, static_cast<uint32_t>(stride), static_cast<uint8_t>(mlp));
        } else if (pattern == "random") {
            bench.SetupRandomAccess(static_cast<uint32_t>(requests), static_cast<uint8_t>(mlp), write);
        } else {
            bench.SetupBankAccess(static_cast<uint32_t>(requests), static_cast<uint32_t>(bank_mask),
                                  static_cast<uint32_t>(bank_set_mask), static_cast<uint8_t>(row_bit_start),
                                  static_cast<uint8_t>(mlp), write);
        }
        bench.Start();
        std::cerr << "Benchmark started" << (wait ? "; waiting for completion\n" : "\n");
        if (wait) {
            const uint32_t remaining = bench.WaitUntilDone(progress, static_cast<uint32_t>(timeout_seconds), requests);
            if (remaining != 0) {
                if (remaining == UINT32_MAX)
                    std::cerr << "Timed out waiting for outstanding responses\n";
                else
                    std::cerr << "Timed out with " << remaining << " requests remaining\n";
                return 1;
            }
            std::cerr << "Benchmark complete\n";
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
