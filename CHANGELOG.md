# Changelog

All notable changes to this project are documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.1.0] - 2026-10-10

### Added

- `zip` command that builds an archive in memory and downloads it through
  Beacon, or writes it to the target with `--disk`.
- `unzip` command that extracts an archive into a new directory after
  validating paths and entries.
- Native BOF builds of `zip` and `unzip` for x86 and x64.
- BOF-PE builds of `zip` and `unzip` for x86 and x64, plus standalone packages
  that include a Beacon API compatibility DLL for the same architecture.
- Support for Unicode Windows paths and UTF-8 ZIP entry names.
- ZIP64 support for archives with more than 65,535 entries.
- BOF-PEs and standalone executables that import only `KERNEL32.dll`,
  `msvcrt.dll`, and `beacon.dll`, so they run without the Universal C
  Runtime.
