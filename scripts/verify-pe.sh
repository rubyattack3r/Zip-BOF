#!/bin/sh
set -eu

OBJDUMP="${LLVM_OBJDUMP:-llvm-objdump}"
PE="${1:-dist/zip.x64.exe}"
BEACON_DLL="${2:-dist/beacon.dll}"
CNA="${3:-dist/zip.cna}"
ARCH="${4:-x64}"
COMMAND="${5:-zip}"
case "$ARCH" in
    x64) MACHINE='x86_64' ;;
    x86) MACHINE='i386' ;;
    *) echo "unsupported architecture: $ARCH" >&2; exit 2 ;;
esac
PE_HEADERS="$(mktemp "${TMPDIR:-/tmp}/zip-bofpe-headers.XXXXXX")"
DLL_HEADERS="$(mktemp "${TMPDIR:-/tmp}/zip-bofpe-dll-headers.XXXXXX")"
trap 'rm -f "$PE_HEADERS" "$DLL_HEADERS"' EXIT HUP INT TERM

"$OBJDUMP" -f "$PE" | grep -Fxq "architecture: $MACHINE"
"$OBJDUMP" -p "$PE" > "$PE_HEADERS"
"$OBJDUMP" -p "$BEACON_DLL" > "$DLL_HEADERS"

grep -Eq '^[[:space:]]+[0-9]+[[:space:]]+0x[0-9a-f]+[[:space:]]+go$' "$PE_HEADERS"
grep -Fq 'DLL Name: beacon.dll' "$PE_HEADERS"

# The PEs link no C runtime; msvcrt.dll keeps them working without the UCRT.
for headers in "$PE_HEADERS" "$DLL_HEADERS"; do
    if grep 'DLL Name:' "$headers" | grep -Ev \
        'DLL Name: (beacon|KERNEL32|msvcrt)\.dll$' | grep -q .; then
        echo "unexpected DLL import in $PE or $BEACON_DLL:" >&2
        grep 'DLL Name:' "$headers" >&2
        exit 1
    fi
done

BEACON_SYMBOLS='BeaconDataExtract BeaconDataParse BeaconPrintf'
if [ "$COMMAND" = zip ]; then
    BEACON_SYMBOLS="$BEACON_SYMBOLS BeaconDataInt BeaconDownload"
fi
for symbol in $BEACON_SYMBOLS; do
    grep -Fq "$symbol" "$PE_HEADERS"
    grep -Fq "$symbol" "$DLL_HEADERS"
done

test -f "$CNA"
case "$COMMAND" in
    zip)
        grep -Fq 'script_resource("zip." . $arch . ".exe")' "$CNA"
        grep -Fq 'bof_pack($bid, "ZZii"' "$CNA"
        ;;
    unzip)
        grep -Fq 'script_resource("unzip." . $arch . ".exe")' "$CNA"
        grep -Fq 'bof_pack($bid, "ZZ"' "$CNA"
        ;;
    *) echo "unsupported command: $COMMAND" >&2; exit 2 ;;
esac
grep -Fq 'beacon_inline_execute_pe($bid, $pe, "go", $args)' "$CNA"

for symbol in GetFullPathNameW GetFileAttributesW; do
    grep -Fq "$symbol" "$PE_HEADERS"
done
if [ "$COMMAND" = zip ]; then
    for symbol in CompareStringOrdinal DeleteFileW GetFileAttributesExW \
        FindFirstFileW FindNextFileW MoveFileExW WideCharToMultiByte; do
        grep -Fq "$symbol" "$PE_HEADERS"
    done
else
    for symbol in CreateDirectoryW MultiByteToWideChar SetFilePointerEx \
        ReadFile WriteFile; do
        grep -Fq "$symbol" "$PE_HEADERS"
    done
fi
for symbol in DeleteFileA GetFullPathNameA GetFileAttributesA \
    GetFileAttributesExA FindFirstFileA FindNextFileA MoveFileExA; do
    if grep -Fq "$symbol" "$PE_HEADERS"; then
        echo "unexpected ANSI path import in $PE: $symbol" >&2
        exit 1
    fi
done

printf 'verified %s %s PE, Beacon imports, and CNA package: %s\n' \
    "$ARCH" "$COMMAND" "$PE"
