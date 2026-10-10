ZIG ?= zig
HOST_CC ?= $(ZIG) cc
# Zig ships no AddressSanitizer runtime; clang builds the sanitizer tests.
SANITIZE_CC ?= clang
LLVM_DLLTOOL ?= llvm-dlltool
LLVM_NM ?= llvm-nm
LLVM_OBJCOPY ?= llvm-objcopy
LLVM_OBJDUMP ?= llvm-objdump
VERSION ?=

# The win7 targets set _WIN32_WINNT to 0x0601.
TARGET_x64 := x86_64-windows.win7-gnu
TARGET_x86 := x86-windows.win7-gnu
DLLTOOL_MACHINE_x64 := i386:x86-64
DLLTOOL_MACHINE_x86 := i386

CPPFLAGS := -Iinclude -Ithird_party/miniz -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
WARNINGS := -Wall -Wextra -Wpedantic -Wno-type-limits -Werror
HOST_CFLAGS := -Os -std=c11 -D_POSIX_C_SOURCE=200809L $(WARNINGS)
# Single-translation-unit builds leave unused static miniz functions behind.
UNIT_CFLAGS := -Oz -g0 -std=c11 $(WARNINGS) -Wno-unused-function -DNDEBUG \
	-fno-addrsig
BOF_CFLAGS := $(UNIT_CFLAGS) -DZIP_BOF -DMINIZ_NO_STDIO -DMINIZ_NO_TIME \
	-DMINIZ_NO_ZLIB_APIS -Wno-dollar-in-identifier-extension -fno-builtin \
	-fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-include include/bof_runtime.h
PE_CFLAGS := $(UNIT_CFLAGS) -DMINIZ_NO_STDIO -ffunction-sections -fdata-sections \
	-include include/pe_runtime.h
PE_LDFLAGS := -nostdlib -Wl,--gc-sections -s
PE_EXE_LDFLAGS := $(PE_LDFLAGS) -Wl,--subsystem,console \
	-Wl,--entry,wmainCRTStartup

MINIZ_SOURCES := \
	third_party/miniz/miniz.c \
	third_party/miniz/miniz_tdef.c \
	third_party/miniz/miniz_tinfl.c \
	third_party/miniz/miniz_zip.c
UNIT_DEPS := $(wildcard src/*.c src/*.h include/*.h) $(MINIZ_SOURCES) \
	$(wildcard third_party/miniz/*.h)
ARCHES := x64 x86

.PHONY: all native test sanitize lint bof bofpe verify-bof verify-bofpe verify \
	release demo clean

all: bof bofpe

native:
	mkdir -p build
	$(HOST_CC) $(CPPFLAGS) $(HOST_CFLAGS) \
		src/zip_path.c src/zip_archive.c src/zip_cli.c $(MINIZ_SOURCES) \
		-o build/zip-native

test: native
	sh tests/test_zip.sh
	$(HOST_CC) $(HOST_CFLAGS) -Iinclude \
		src/unzip_name.c tests/test_unzip_name.c -o build/test-unzip-name
	build/test-unzip-name

# Native BOFs: one relocatable COFF object per command and architecture.
dist/zip.%.o: src/zip_bof_unit.c $(UNIT_DEPS)
	mkdir -p dist
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(BOF_CFLAGS) -c $< -o $@
	$(LLVM_OBJCOPY) --strip-unneeded $@

dist/unzip.%.o: src/unzip_bof_unit.c $(UNIT_DEPS)
	mkdir -p dist
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(BOF_CFLAGS) -c $< -o $@
	$(LLVM_OBJCOPY) --strip-unneeded $@

# Also copies cna/bofpe/*.cna to dist/bofpe/.
dist/%.cna: cna/%.cna
	mkdir -p $(dir $@)
	cp $< $@

bof: $(foreach arch,$(ARCHES),dist/zip.$(arch).o dist/unzip.$(arch).o) \
	dist/zip.cna dist/unzip.cna

verify-bof: bof
	LLVM_NM="$(LLVM_NM)" LLVM_OBJDUMP="$(LLVM_OBJDUMP)" \
		sh scripts/verify-bof.sh

# BOF-PEs: no C runtime; C functions are imported from msvcrt.dll.
build/bofpe/%/libmsvcrt.a: src/msvcrt.def
	mkdir -p $(dir $@)
	$(LLVM_DLLTOOL) -m $(DLLTOOL_MACHINE_$*) -d $< -l $@

build/bofpe/%/beacon_compat.o: src/beacon_compat.c $(UNIT_DEPS)
	mkdir -p $(dir $@)
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(PE_CFLAGS) -DBUILD_BEACON \
		-c $< -o $@

build/bofpe/%/beacon_runtime.o: src/pe_runtime.c $(UNIT_DEPS)
	mkdir -p $(dir $@)
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(PE_CFLAGS) -DBUILD_BEACON \
		-c $< -o $@

build/bofpe/%/beacon.dll: build/bofpe/%/beacon_compat.o \
	build/bofpe/%/beacon_runtime.o build/bofpe/%/libmsvcrt.a
	$(ZIG) cc -target $(TARGET_$*) -shared $(PE_LDFLAGS) \
		build/bofpe/$*/beacon_compat.o build/bofpe/$*/beacon_runtime.o \
		-Lbuild/bofpe/$* -lmsvcrt -lkernel32 \
		-Wl,--out-implib,build/bofpe/$*/libbeacon.a -o $@

build/bofpe/%/libbeacon.a: build/bofpe/%/beacon.dll
	@test -f $@

build/bofpe/%/zip.o: src/zip_bofpe_unit.c $(UNIT_DEPS)
	mkdir -p $(dir $@)
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(PE_CFLAGS) -c $< -o $@

build/bofpe/%/unzip.o: src/unzip_bofpe_unit.c $(UNIT_DEPS)
	mkdir -p $(dir $@)
	$(ZIG) cc -target $(TARGET_$*) $(CPPFLAGS) $(PE_CFLAGS) -c $< -o $@

# Link in build/ because lld also writes an import library for the exported
# go function; only the executables are copied to dist/.
build/bofpe/%/zip.exe: build/bofpe/%/zip.o build/bofpe/%/libbeacon.a \
	build/bofpe/%/libmsvcrt.a
	$(ZIG) cc -target $(TARGET_$*) $(PE_EXE_LDFLAGS) $< \
		-Lbuild/bofpe/$* -lbeacon -lmsvcrt -lkernel32 -o $@

build/bofpe/%/unzip.exe: build/bofpe/%/unzip.o build/bofpe/%/libbeacon.a \
	build/bofpe/%/libmsvcrt.a
	$(ZIG) cc -target $(TARGET_$*) $(PE_EXE_LDFLAGS) $< \
		-Lbuild/bofpe/$* -lbeacon -lmsvcrt -lkernel32 -o $@

dist/bofpe/zip.%.exe: build/bofpe/%/zip.exe
	mkdir -p dist/bofpe
	cp $< $@

dist/bofpe/unzip.%.exe: build/bofpe/%/unzip.exe
	mkdir -p dist/bofpe
	cp $< $@

dist/bofpe/beacon.dll: build/bofpe/x64/beacon.dll
	mkdir -p dist/bofpe
	cp $< $@

bofpe: $(foreach arch,$(ARCHES),dist/bofpe/zip.$(arch).exe \
	dist/bofpe/unzip.$(arch).exe) dist/bofpe/beacon.dll \
	build/bofpe/x86/beacon.dll dist/bofpe/zip.cna dist/bofpe/unzip.cna

verify-bofpe: bofpe
	LLVM_OBJDUMP="$(LLVM_OBJDUMP)" sh scripts/verify-pe.sh \
		dist/bofpe/zip.x64.exe dist/bofpe/beacon.dll dist/bofpe/zip.cna x64
	LLVM_OBJDUMP="$(LLVM_OBJDUMP)" sh scripts/verify-pe.sh \
		dist/bofpe/zip.x86.exe build/bofpe/x86/beacon.dll dist/bofpe/zip.cna x86
	LLVM_OBJDUMP="$(LLVM_OBJDUMP)" sh scripts/verify-pe.sh \
		dist/bofpe/unzip.x64.exe dist/bofpe/beacon.dll dist/bofpe/unzip.cna x64 unzip
	LLVM_OBJDUMP="$(LLVM_OBJDUMP)" sh scripts/verify-pe.sh \
		dist/bofpe/unzip.x86.exe build/bofpe/x86/beacon.dll dist/bofpe/unzip.cna x86 unzip

# Host tests under AddressSanitizer and UndefinedBehaviorSanitizer.
SANITIZE_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all \
	-fno-omit-frame-pointer -g -O1 -std=c11 -D_POSIX_C_SOURCE=200809L $(WARNINGS)

sanitize:
	mkdir -p build/sanitize
	$(SANITIZE_CC) $(CPPFLAGS) $(SANITIZE_FLAGS) \
		src/zip_path.c src/zip_archive.c src/zip_cli.c $(MINIZ_SOURCES) \
		-o build/sanitize/zip-native
	ZIP_BIN=build/sanitize/zip-native sh tests/test_zip.sh
	$(SANITIZE_CC) $(SANITIZE_FLAGS) -Iinclude \
		src/unzip_name.c tests/test_unzip_name.c -o build/sanitize/test-unzip-name
	build/sanitize/test-unzip-name

lint:
	cppcheck --quiet --error-exitcode=1 --std=c11 --inline-suppr \
		--enable=warning,portability,performance \
		--suppress=missingIncludeSystem -Iinclude -i third_party src tests
	shellcheck --severity=warning scripts/*.sh tests/*.sh
	actionlint

verify: lint test sanitize verify-bof verify-bofpe

release: verify-bof verify-bofpe
	@test -n "$(VERSION)" || { echo "VERSION is required (for example, VERSION=v0.1.0)" >&2; exit 2; }
	sh scripts/package-bof-release.sh "$(VERSION)"
	sh scripts/package-pe-release.sh "$(VERSION)"

# Renders docs/demo/demo-{dark,light}.{svg,gif} from docs/demo/demo.toml with
# beaconcast; run in the demo shell: nix develop .#demo -c make demo
demo:
	for mode in dark light; do \
		beaconcast render --mode $$mode \
			--svg docs/demo/demo-$$mode.svg \
			--gif docs/demo/demo-$$mode.gif docs/demo/demo.toml; \
	done

clean:
	rm -rf build dist release
