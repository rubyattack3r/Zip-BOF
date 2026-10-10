# Zip BOF Project Context

## Overview

This repository builds `zip` and `unzip` in two formats: native Cobalt Strike
BOFs and BOF-PEs. Both formats support x86 and x64 Beacons. Share the archive
core, but keep the format-specific entry points, CNA files, and release bundles
separate.

## Architecture

- `src/zip_bof.c` and `src/unzip_bof.c` are native BOF entry points;
  `src/zip_bofpe.c` and `src/unzip_bofpe.c` are BOF-PE entry points.
- `src/zip_archive.c` and `src/unzip_archive.c` implement archive operations;
  `src/zip_path.c` handles Windows paths, and `src/unzip_name.c` validates ZIP
  entry names.
- Each native BOF and BOF-PE is one translation unit: `src/*_bof_unit.c`
  and `src/*_bofpe_unit.c` include the sources, and `src/miniz_static.h`
  makes the miniz API static so unused code is dropped.
- Neither format links a C runtime. `src/bof_runtime.c` and
  `include/bof_runtime.h` support native BOFs; `src/pe_runtime.c`,
  `include/pe_runtime.h`, and `src/msvcrt.def` support BOF-PEs, which import
  C functions from `msvcrt.dll` only. `src/stack_probe.c` provides the stack
  probes for both. `src/beacon_compat.c` provides the standalone BOF-PE
  compatibility DLL, and `src/zip_cli.c` is the host test executable.
- `cna/` contains native BOF commands; `cna/bofpe/` contains BOF-PE commands.
  Both select the matching x86 or x64 artifact. Do not load both CNA sets
  together because they register the same commands.
- `tests/` contains archive and entry-name tests. `scripts/` contains
  verification and release packaging checks. `.github/` contains CI,
  CodeQL, release workflows, and Dependabot configuration.
- `third_party/miniz/` is vendored upstream code. Avoid modifying it unless
  updating the dependency deliberately; retain its license notices.

## Behavior and Safety Invariants

- Preserve `zip <source> <destination.zip> [level 0-9] [--disk]` and
  `unzip <archive.zip> <new-directory>` unless a user-facing change is intended.
- `zip` downloads the archive through Beacon by default; `--disk` writes it
  atomically on the target. Preserve the documented limits and ZIP64 support.
- `unzip` must not overwrite an existing destination. Reject traversal,
  absolute or ambiguous entry paths, links, encrypted entries, and unsupported
  compression. Preserve extraction limits and document partial-output behavior.
- Declare every native BOF import, Beacon API and `MODULE$function` alike,
  with `__declspec(dllimport)` so the object references `__imp_` symbols, the
  only form Beacon's loader resolves. `scripts/verify-bof.sh` rejects any
  other undefined symbol; lenient loaders such as bemu accept bare symbols,
  so passing there does not prove this.
- Keep Windows filesystem paths as UTF-16 and ZIP entry names and Beacon
  messages as UTF-8. Preserve validation of relative, fully qualified drive,
  and UNC paths and rejection of ambiguous or device-namespace paths.
- Changes to archive parsing, path handling, or extraction need focused tests
  for malformed inputs, both formats, and both architectures where applicable.

## Build and Verification

Use the `flake.nix` development shell; Zig compiles every target. Run these
checks after relevant changes:

```sh
nix develop -c make verify
nix develop -c make release VERSION=v0.0.0-ci
```

`make verify` runs `cppcheck`, `shellcheck`, and `actionlint`, the host tests
with Zig and again under AddressSanitizer and UndefinedBehaviorSanitizer
(built with clang, since Zig ships no AddressSanitizer runtime), and both BOF
formats for both architectures. Every build uses `-Werror`. CI runs the same
commands in the development shell; pin every GitHub Action to a commit SHA
with a `# ratchet:` comment.

For behavior the host tests cannot cover, also test on Windows.

## Documentation and Changelog

- Keep `README.md`, CNA help, and distribution artifacts consistent with behavior.
- Follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/): put
  user-facing changes under `Unreleased`, group them by applicable types such
  as `Added`, `Changed`, `Fixed`, and `Security`, and move them to a dated
  version section only when releasing. Do not use the changelog as a work log.

## Releasing

1. Move the `Unreleased` entries in `CHANGELOG.md` to a new
   `## [X.Y.Z] - YYYY-MM-DD` section.
2. Commit, then tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.
3. The release workflow runs `make release`, publishes the bundles and
   checksums with that changelog section as release notes, and attests their
   build provenance. It fails if the changelog has no section for the tag.
