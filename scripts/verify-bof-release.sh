#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <release.zip> [release.zip.sha256]" >&2
    exit 2
fi

ARCHIVE=$1
CHECKSUM=${2:-"$ARCHIVE.sha256"}
ARCHIVE_DIR=$(CDPATH='' cd -- "$(dirname -- "$ARCHIVE")" && pwd)
ARCHIVE_BASE=$(basename -- "$ARCHIVE")
CHECKSUM_BASE=$(basename -- "$CHECKSUM")
PACKAGE_NAME=${ARCHIVE_BASE%.zip}
ACTUAL=$(mktemp "${TMPDIR:-/tmp}/zip-bof-release-actual.XXXXXX")
EXPECTED=$(mktemp "${TMPDIR:-/tmp}/zip-bof-release-expected.XXXXXX")
trap 'rm -f "$ACTUAL" "$EXPECTED"' EXIT HUP INT TERM

test -f "$ARCHIVE"
test -f "$CHECKSUM"
unzip -tq "$ARCHIVE" >/dev/null
unzip -Z1 "$ARCHIVE" | LC_ALL=C sort > "$ACTUAL"
printf '%s\n' \
    "$PACKAGE_NAME/" \
    "$PACKAGE_NAME/CHANGELOG.md" \
    "$PACKAGE_NAME/LICENSE" \
    "$PACKAGE_NAME/MINIZ-LICENSE.txt" \
    "$PACKAGE_NAME/README.md" \
    "$PACKAGE_NAME/THIRD_PARTY_NOTICES.md" \
    "$PACKAGE_NAME/VERSION.txt" \
    "$PACKAGE_NAME/zip.cna" \
    "$PACKAGE_NAME/zip.x64.o" \
    "$PACKAGE_NAME/zip.x86.o" \
    "$PACKAGE_NAME/unzip.cna" \
    "$PACKAGE_NAME/unzip.x64.o" \
    "$PACKAGE_NAME/unzip.x86.o" |
    LC_ALL=C sort > "$EXPECTED"
diff -u "$EXPECTED" "$ACTUAL"

if command -v sha256sum >/dev/null 2>&1; then
    (cd "$ARCHIVE_DIR" && sha256sum -c "$CHECKSUM_BASE")
else
    (cd "$ARCHIVE_DIR" && shasum -a 256 -c "$CHECKSUM_BASE")
fi

printf 'verified release package: %s\n' "$ARCHIVE"
