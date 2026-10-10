#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <version, for example v0.1.0>" >&2
    exit 2
fi

VERSION=$1
if ! printf '%s\n' "$VERSION" |
    grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z][0-9A-Za-z.-]*)?$'; then
    echo "invalid release version: $VERSION" >&2
    exit 2
fi

PROJECT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
RELEASE_DIR=${RELEASE_DIR:-"$PROJECT_DIR/release"}
PACKAGE_NAME="Zip-BOF-PE-$VERSION"
WORK_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/zip-bofpe-release.XXXXXX")
PACKAGE_ROOT="$WORK_ROOT/$PACKAGE_NAME"
trap 'rm -rf "$WORK_ROOT"' EXIT HUP INT TERM

package_archive() {
    package_name=$1
    package_root="$WORK_ROOT/$package_name"
    archive="$RELEASE_DIR/$package_name.zip"
    checksum="$archive.sha256"

    printf '%s\n' "$VERSION" > "$package_root/VERSION.txt"
    # Normalized timestamps and metadata make repeated packages byte-identical.
    find "$package_root" -exec touch -t 198001010000 {} +
    (
        cd "$WORK_ROOT"
        LC_ALL=C find "$package_name" -print |
            LC_ALL=C sort |
            zip -X -q "$WORK_ROOT/$package_name.zip" -@
    )

    mv "$WORK_ROOT/$package_name.zip" "$archive"
    if command -v sha256sum >/dev/null 2>&1; then
        digest=$(sha256sum "$archive" | awk '{print $1}')
    else
        digest=$(shasum -a 256 "$archive" | awk '{print $1}')
    fi
    printf '%s  %s\n' "$digest" "$package_name.zip" > "$WORK_ROOT/checksum"
    mv "$WORK_ROOT/checksum" "$checksum"

    sh "$PROJECT_DIR/scripts/verify-pe-release.sh" "$archive" "$checksum"
    printf 'created release package: %s\n' "$archive"
    printf 'created release checksum: %s\n' "$checksum"
}

for required in \
    "$PROJECT_DIR/dist/bofpe/zip.cna" \
    "$PROJECT_DIR/dist/bofpe/zip.x64.exe" \
    "$PROJECT_DIR/dist/bofpe/zip.x86.exe" \
    "$PROJECT_DIR/dist/bofpe/unzip.cna" \
    "$PROJECT_DIR/dist/bofpe/unzip.x64.exe" \
    "$PROJECT_DIR/dist/bofpe/unzip.x86.exe" \
    "$PROJECT_DIR/dist/bofpe/beacon.dll" \
    "$PROJECT_DIR/build/bofpe/x86/beacon.dll" \
    "$PROJECT_DIR/README.md" \
    "$PROJECT_DIR/CHANGELOG.md" \
    "$PROJECT_DIR/LICENSE" \
    "$PROJECT_DIR/THIRD_PARTY_NOTICES.md" \
    "$PROJECT_DIR/third_party/miniz/LICENSE"; do
    if [ ! -f "$required" ]; then
        echo "required release input is missing: $required" >&2
        exit 1
    fi
done

mkdir -p "$PACKAGE_ROOT" "$RELEASE_DIR"
cp "$PROJECT_DIR/dist/bofpe/zip.cna" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/dist/bofpe/zip.x64.exe" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/dist/bofpe/zip.x86.exe" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/dist/bofpe/unzip.cna" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/dist/bofpe/unzip.x64.exe" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/dist/bofpe/unzip.x86.exe" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/README.md" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/CHANGELOG.md" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/LICENSE" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/THIRD_PARTY_NOTICES.md" "$PACKAGE_ROOT/"
cp "$PROJECT_DIR/third_party/miniz/LICENSE" \
    "$PACKAGE_ROOT/MINIZ-LICENSE.txt"
package_archive "$PACKAGE_NAME"

for arch in x64 x86; do
    standalone_name="$PACKAGE_NAME-standalone-$arch"
    standalone_root="$WORK_ROOT/$standalone_name"
    mkdir -p "$standalone_root"
    cp "$PROJECT_DIR/dist/bofpe/zip.$arch.exe" "$standalone_root/zip.exe"
    cp "$PROJECT_DIR/dist/bofpe/unzip.$arch.exe" "$standalone_root/unzip.exe"
    if [ "$arch" = x64 ]; then
        cp "$PROJECT_DIR/dist/bofpe/beacon.dll" "$standalone_root/beacon.dll"
    else
        cp "$PROJECT_DIR/build/bofpe/x86/beacon.dll" \
            "$standalone_root/beacon.dll"
    fi
    cp "$PROJECT_DIR/README.md" "$standalone_root/"
    cp "$PROJECT_DIR/CHANGELOG.md" "$standalone_root/"
    cp "$PROJECT_DIR/LICENSE" "$standalone_root/"
    cp "$PROJECT_DIR/THIRD_PARTY_NOTICES.md" "$standalone_root/"
    cp "$PROJECT_DIR/third_party/miniz/LICENSE" \
        "$standalone_root/MINIZ-LICENSE.txt"
    package_archive "$standalone_name"
done
