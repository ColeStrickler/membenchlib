#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: bank_attack.sh --bank-bits BIT[,BIT...] [options]

Run same-bank, different-row accesses for every MLP value from 1 through 16.

Required:
  --bank-bits BITS       Physical address bit positions selecting the bank
                         (for example: 6,7,8)

Options:
  -n, --requests N       Requests per MLP value (default: 1000000)
  --bank-value N         Logical bank number encoded across BITS (default: 0)
  --row-bit-start N      First varying row-address bit; defaults to one bit
                         above the highest bank bit
  --timeout-seconds N    Timeout for each run (default: 30; 0 disables)
  --binary PATH          membenchpress binary (default: beside this script)
  --write                Issue writes instead of reads
  -h, --help             Show this help

Example:
  sudo ./bank_attack.sh --bank-bits 6,7,8 --bank-value 3 \
      --row-bit-start 18 --requests 10000000

For bank bits 6,7,8, bank value 3 fixes physical bits 6 and 7 to one and
bit 8 to zero. Bits from row-bit-start through bit 27 vary between accesses.
EOF
}

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
binary="${script_dir}/membenchpress"
bank_bits=""
bank_value=0
requests=1000000
row_bit_start=""
timeout_seconds=30
access_option="--read"

while (($# > 0)); do
    case "$1" in
    --bank-bits)
        (($# >= 2)) || { echo "Missing value for --bank-bits" >&2; exit 2; }
        bank_bits=$2
        shift 2
        ;;
    --bank-value)
        (($# >= 2)) || { echo "Missing value for --bank-value" >&2; exit 2; }
        bank_value=$2
        shift 2
        ;;
    -n|--requests)
        (($# >= 2)) || { echo "Missing value for $1" >&2; exit 2; }
        requests=$2
        shift 2
        ;;
    --row-bit-start)
        (($# >= 2)) || { echo "Missing value for --row-bit-start" >&2; exit 2; }
        row_bit_start=$2
        shift 2
        ;;
    --timeout-seconds)
        (($# >= 2)) || { echo "Missing value for --timeout-seconds" >&2; exit 2; }
        timeout_seconds=$2
        shift 2
        ;;
    --binary)
        (($# >= 2)) || { echo "Missing value for --binary" >&2; exit 2; }
        binary=$2
        shift 2
        ;;
    --write)
        access_option="--write"
        shift
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown option: $1" >&2
        usage >&2
        exit 2
        ;;
    esac
done

[[ -n "$bank_bits" ]] || { echo "--bank-bits is required" >&2; usage >&2; exit 2; }
[[ "$requests" =~ ^[1-9][0-9]*$ ]] || { echo "--requests must be a positive integer" >&2; exit 2; }
[[ "$bank_value" =~ ^[0-9]+$ ]] || { echo "--bank-value must be a nonnegative integer" >&2; exit 2; }
[[ "$timeout_seconds" =~ ^[0-9]+$ ]] || { echo "--timeout-seconds must be a nonnegative integer" >&2; exit 2; }
[[ -x "$binary" ]] || { echo "Benchmark binary is not executable: $binary" >&2; exit 1; }

IFS=',' read -r -a bits <<< "$bank_bits"
((${#bits[@]} > 0)) || { echo "At least one bank bit is required" >&2; exit 2; }

bank_mask=0
bank_set_mask=0
max_bank_bit=-1
declare -A seen_bits=()

for index in "${!bits[@]}"; do
    bit=${bits[$index]}
    [[ "$bit" =~ ^[0-9]+$ ]] || { echo "Invalid bank bit: $bit" >&2; exit 2; }
    ((bit >= 6 && bit <= 26)) || {
        echo "Bank bit $bit must be between 6 and 26" >&2
        exit 2
    }
    [[ -z "${seen_bits[$bit]+present}" ]] || { echo "Duplicate bank bit: $bit" >&2; exit 2; }
    seen_bits[$bit]=1

    ((bank_mask |= 1 << bit))
    ((bit > max_bank_bit)) && max_bank_bit=$bit
    (((bank_value >> index) & 1)) && ((bank_set_mask |= 1 << bit))
done

max_bank_value=$(((1 << ${#bits[@]}) - 1))
((bank_value <= max_bank_value)) || {
    echo "--bank-value must be between 0 and $max_bank_value for ${#bits[@]} bank bits" >&2
    exit 2
}

if [[ -z "$row_bit_start" ]]; then
    row_bit_start=$((max_bank_bit + 1))
fi
[[ "$row_bit_start" =~ ^[0-9]+$ ]] || { echo "--row-bit-start must be an integer" >&2; exit 2; }
((row_bit_start > max_bank_bit && row_bit_start <= 27)) || {
    echo "--row-bit-start must be greater than the highest bank bit and no greater than 27" >&2
    exit 2
}

printf 'Bank bits: %s; bank value: %u; bank mask: 0x%x; set mask: 0x%x\n' \
    "$bank_bits" "$bank_value" "$bank_mask" "$bank_set_mask"
printf 'Varying row bits %u..27; %u requests per MLP value\n' "$row_bit_start" "$requests"

for mlp in {1..16}; do
    printf '\n===== MLP %u =====\n' "$mlp"
    "$binary" \
        --pattern bank \
        --requests "$requests" \
        --mlp "$mlp" \
        "$access_option" \
        --bank-mask "$bank_mask" \
        --bank-set-mask "$bank_set_mask" \
        --row-bit-start "$row_bit_start" \
        --timeout-seconds "$timeout_seconds"
done
