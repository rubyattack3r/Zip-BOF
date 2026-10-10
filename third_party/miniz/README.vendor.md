# Vendored miniz

- Upstream: https://github.com/richgel999/miniz
- Version: 3.1.2
- Commit: `77d0dce8627735138c51770d1799a1ef48f2117d`
- License: MIT; see `LICENSE` in this directory.

## Contents

`LICENSE` and the modular `miniz*.c` and `miniz*.h` sources are copied
unmodified from the upstream commit. Upstream tests, examples, and build files
are not included.

`miniz_export.h` is local to this repository. Upstream generates it with CMake;
here it defines `MINIZ_EXPORT` as empty because miniz is always linked
statically.

## Build use

The sources are compiled directly into the host test executable, the native
BOFs, and the BOF-PEs. All builds define `MINIZ_NO_ZLIB_COMPATIBLE_NAMES`. The
native BOFs and BOF-PEs also define `MINIZ_NO_STDIO`, and the native BOFs
define `MINIZ_NO_TIME` and `MINIZ_NO_ZLIB_APIS` as well.

The native BOFs and BOF-PEs are built as single translation units that
include the miniz sources. `src/miniz_static.h` declares the miniz API
`static` there, so unused miniz functions are dropped without modifying the
vendored files.

## Updating

Replace the upstream files with those from the new commit, keep
`miniz_export.h`, and update the version and commit here and in
`THIRD_PARTY_NOTICES.md`.
