#!/bin/sh
set -eu

NM="${LLVM_NM:-llvm-nm}"
OBJDUMP="${LLVM_OBJDUMP:-llvm-objdump}"

verify_object() {
    object=$1
    format=$2
    entry=$3
    "$OBJDUMP" -f "$object" | grep -Fq "file format $format"
    "$NM" -g "$object" | grep -Eq " T ${entry}\$"
    # Beacon's loader resolves only __imp_ symbols: every import must be a
    # dllimport of the Beacon API or a KERNEL32$ or MSVCRT$ function.
    if "$NM" -u "$object" | grep -Ev \
        ' __imp__?(Beacon[A-Za-z0-9_]+|KERNEL32\$[A-Za-z0-9_]+(@[0-9]+)?|MSVCRT\$[A-Za-z0-9_]+)$' \
        | grep -q .; then
        echo "unexpected or non-dllimport BOF imports in $object:" >&2
        "$NM" -u "$object" >&2
        exit 1
    fi
}

verify_object dist/zip.x64.o coff-x86-64 go
verify_object dist/zip.x86.o coff-i386 _go
verify_object dist/unzip.x64.o coff-x86-64 go
verify_object dist/unzip.x86.o coff-i386 _go

test -f dist/zip.cna
test -f dist/unzip.cna
grep -Fq 'script_resource("unzip." . $arch . ".o")' dist/unzip.cna
grep -Fq 'bof_pack($bid, "ZZ"' dist/unzip.cna
grep -Fq 'beacon_inline_execute($bid, $bof, "go", $args)' dist/unzip.cna
