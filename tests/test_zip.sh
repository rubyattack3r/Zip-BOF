#!/bin/sh
set -eu

ZIP_BIN="${ZIP_BIN:-./build/zip-native}"
case "$ZIP_BIN" in
    /*) ;;
    *) ZIP_BIN="$(pwd)/${ZIP_BIN#./}" ;;
esac
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/zip-bof-test.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

mkdir -p "$TEST_ROOT/input/nested" "$TEST_ROOT/input/empty"
printf 'alpha\n' > "$TEST_ROOT/input/alpha.txt"
printf 'bravo\n' > "$TEST_ROOT/input/nested/bravo.txt"
printf 'charlie\n' > "$TEST_ROOT/input/name with spaces.txt"
ln -s alpha.txt "$TEST_ROOT/input/link-to-alpha"

"$ZIP_BIN" "$TEST_ROOT/input" "$TEST_ROOT/input/result archive.zip" 6
"$ZIP_BIN" "$TEST_ROOT/input" "$TEST_ROOT/input/result archive.zip" 9

unzip -Z1 "$TEST_ROOT/input/result archive.zip" | sort > "$TEST_ROOT/actual-entries"
printf '%s\n' \
    'input/' \
    'input/alpha.txt' \
    'input/empty/' \
    'input/name with spaces.txt' \
    'input/nested/' \
    'input/nested/bravo.txt' | sort > "$TEST_ROOT/expected-entries"
diff -u "$TEST_ROOT/expected-entries" "$TEST_ROOT/actual-entries"

unzip -p "$TEST_ROOT/input/result archive.zip" input/alpha.txt > "$TEST_ROOT/alpha.out"
printf 'alpha\n' > "$TEST_ROOT/alpha.expected"
diff -u "$TEST_ROOT/alpha.expected" "$TEST_ROOT/alpha.out"

"$ZIP_BIN" "$TEST_ROOT/input/alpha.txt" "$TEST_ROOT/single.zip" 0
unzip -Z1 "$TEST_ROOT/single.zip" > "$TEST_ROOT/single-entries"
printf 'alpha.txt\n' > "$TEST_ROOT/single-expected"
diff -u "$TEST_ROOT/single-expected" "$TEST_ROOT/single-entries"

"$ZIP_BIN" "$TEST_ROOT/input/alpha.txt" \
    "$TEST_ROOT/disk-output.zip" 6 --disk
unzip -t "$TEST_ROOT/disk-output.zip" >/dev/null
"$ZIP_BIN" "$TEST_ROOT/input/alpha.txt" \
    "$TEST_ROOT/disk-default-level.zip" --disk >/dev/null
unzip -t "$TEST_ROOT/disk-default-level.zip" >/dev/null

mkdir -p "$TEST_ROOT/UnicodeSamples/café/例"
printf 'Unicode fixture\n' > "$TEST_ROOT/UnicodeSamples/café/例/пример.txt"
(
    cd "$TEST_ROOT"
    "$ZIP_BIN" "./UnicodeSamples/" "résumé.zip" 6 >/dev/null
)
bsdtar -tf "$TEST_ROOT/résumé.zip" \
    > "$TEST_ROOT/unicode-entries"
printf '%s\n' \
    'UnicodeSamples/' \
    'UnicodeSamples/café/' \
    'UnicodeSamples/café/例/' \
    'UnicodeSamples/café/例/пример.txt' \
    > "$TEST_ROOT/unicode-expected"
diff -u "$TEST_ROOT/unicode-expected" "$TEST_ROOT/unicode-entries"
perl tests/check_zip_utf8_flags.pl "$TEST_ROOT/résumé.zip" >/dev/null
mkdir -p "$TEST_ROOT/unicode-round-trip"
bsdtar -xf "$TEST_ROOT/résumé.zip" \
    -C "$TEST_ROOT/unicode-round-trip"
cmp "$TEST_ROOT/UnicodeSamples/café/例/пример.txt" \
    "$TEST_ROOT/unicode-round-trip/UnicodeSamples/café/例/пример.txt"

if "$ZIP_BIN" "$TEST_ROOT/input/alpha.txt" "$TEST_ROOT/input/alpha.txt" 6 >/dev/null 2>&1; then
    echo "expected identical source and destination to fail" >&2
    exit 1
fi
diff -u "$TEST_ROOT/alpha.expected" "$TEST_ROOT/input/alpha.txt"

if "$ZIP_BIN" "$TEST_ROOT/input" "$TEST_ROOT/bad.zip" 10 >/dev/null 2>&1; then
    echo "expected invalid compression level to fail" >&2
    exit 1
fi
if "$ZIP_BIN" "$TEST_ROOT/input" "$TEST_ROOT/bad.zip" 6 --unknown \
    >/dev/null 2>&1; then
    echo "expected unknown option to fail" >&2
    exit 1
fi

dd if=/dev/zero of="$TEST_ROOT/large.bin" bs=1 count=0 seek=68157440 2>/dev/null
if "$ZIP_BIN" "$TEST_ROOT/large.bin" "$TEST_ROOT/large-memory.zip" 1 >/dev/null 2>&1; then
    echo "expected in-memory input limit to reject a 65 MiB input" >&2
    exit 1
fi
"$ZIP_BIN" "$TEST_ROOT/large.bin" \
    "$TEST_ROOT/large-disk.zip" 1 --disk >/dev/null
unzip -t "$TEST_ROOT/large-disk.zip" >/dev/null

# A directory with 65,535 files plus its root entry crosses the classic ZIP
# limit and must contain a complete ZIP64 end record. Keep the files empty so
# the default in-memory BOF path remains well below its 64 MiB input limit.
ZIP64_FILE_COUNT=65535
ZIP64_ENTRY_COUNT=65536
mkdir -p "$TEST_ROOT/zip64-many"
TEST_MANY_DIR="$TEST_ROOT/zip64-many" \
TEST_MANY_COUNT="$ZIP64_FILE_COUNT" \
perl -e '
    use strict;
    use warnings;
    my $directory = $ENV{TEST_MANY_DIR};
    my $count = $ENV{TEST_MANY_COUNT};
    for (my $index = 0; $index < $count; ++$index) {
        my $path = sprintf("%s/f%05d", $directory, $index);
        open(my $file, ">", $path) or die "cannot create $path: $!\n";
        close($file) or die "cannot close $path: $!\n";
    }
'
printf 'ZIP64 round-trip payload\n' \
    > "$TEST_ROOT/zip64-many/f32767"
(
    cd "$TEST_ROOT"
    find zip64-many -type f -print | LC_ALL=C sort
) > "$TEST_ROOT/zip64-source-files"
"$ZIP_BIN" "$TEST_ROOT/zip64-many" \
    "$TEST_ROOT/zip64-many.zip" 0 >/dev/null
"$ZIP_BIN" "$TEST_ROOT/zip64-many" \
    "$TEST_ROOT/zip64-many-disk.zip" 0 --disk >/dev/null
ZIP64_ARCHIVE_INDEX=0
for ZIP64_ARCHIVE in \
    "$TEST_ROOT/zip64-many.zip" \
    "$TEST_ROOT/zip64-many-disk.zip"
do
    unzip -tq "$ZIP64_ARCHIVE" >/dev/null
    ACTUAL_ZIP64_ENTRIES=$(
        unzip -Z1 "$ZIP64_ARCHIVE" | wc -l | tr -d ' '
    )
    if [ "$ACTUAL_ZIP64_ENTRIES" -ne "$ZIP64_ENTRY_COUNT" ]; then
        echo "ZIP64 archive has $ACTUAL_ZIP64_ENTRIES entries;" \
            "expected $ZIP64_ENTRY_COUNT" >&2
        exit 1
    fi
    perl tests/check_zip64.pl "$ZIP64_ARCHIVE" \
        "$ZIP64_ENTRY_COUNT" >/dev/null

    ZIP64_EXTRACTED="$TEST_ROOT/zip64-extracted-$ZIP64_ARCHIVE_INDEX"
    mkdir -p "$ZIP64_EXTRACTED"
    unzip -q "$ZIP64_ARCHIVE" -d "$ZIP64_EXTRACTED"
    (
        cd "$ZIP64_EXTRACTED"
        find zip64-many -type f -print | LC_ALL=C sort
    ) > "$ZIP64_EXTRACTED/files"
    diff -u "$TEST_ROOT/zip64-source-files" \
        "$ZIP64_EXTRACTED/files"
    cmp "$TEST_ROOT/zip64-many/f32767" \
        "$ZIP64_EXTRACTED/zip64-many/f32767"
    ZIP64_ARCHIVE_INDEX=$((ZIP64_ARCHIVE_INDEX + 1))
done

printf 'zip archive tests passed\n'
