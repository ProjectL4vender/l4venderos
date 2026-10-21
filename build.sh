#!/usr/bin/env bash
# Build SystemC 3.0 BL0SS0M MOD.0 (freestanding i386 only) and create a legacy-BIOS ISO.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
mkdir -p build dist
# Select the GRUB/Multiboot boot mode; the kernel also attempts live VBE mode changes.
SYSTEMC_RESOLUTION="${SYSTEMC_RESOLUTION:-640x480}"
if [[ ! "$SYSTEMC_RESOLUTION" =~ ^([0-9]{3,4})x([0-9]{3,4})$ ]]; then
  echo "ERROR: SYSTEMC_RESOLUTION must look like 1920x1080 (got '$SYSTEMC_RESOLUTION')." >&2; exit 2
fi
BOOT_WIDTH="${BASH_REMATCH[1]}"; BOOT_HEIGHT="${BASH_REMATCH[2]}"
if (( BOOT_WIDTH < 320 || BOOT_WIDTH > 1920 || BOOT_HEIGHT < 240 || BOOT_HEIGHT > 1080 )); then
  echo "ERROR: supported requested range is 320x240 through 1920x1080." >&2; exit 2
fi
export SYSTEMC_RESOLUTION
# Remove a stale release ISO before starting so failed builds cannot leave an old image looking current.
rm -f "$ROOT/dist/SystemC-3.0-i386.iso" "$ROOT/dist/SystemC-3.0-i386.iso.tmp" "$ROOT/dist/.SystemC-3.0-i386.tmp.iso"
LOG="$ROOT/build/build.log"
: > "$LOG"
run() {
  printf ' +'; printf ' %q' "$@"; printf '\n'
  "$@" 2>&1 | tee -a "$LOG"
}

if [[ -n "${CC:-}" ]]; then
  COMPILER="$CC"
elif [[ "$(uname -s 2>/dev/null || true)" == Darwin* ]]; then
  COMPILER="$(command -v clang || true)"
  [[ -n "$COMPILER" ]] || { echo "ERROR: install LLVM/Clang (and LLD) on macOS." >&2; exit 1; }
elif command -v i686-elf-gcc >/dev/null 2>&1; then
  COMPILER=i686-elf-gcc
elif command -v gcc >/dev/null 2>&1; then
  COMPILER=gcc
elif command -v clang >/dev/null 2>&1; then
  COMPILER=clang
else
  echo "ERROR: no C compiler found. Install gcc or clang." >&2; exit 1
fi

if [[ -n "${LD:-}" ]]; then
  LINKER="$LD"
elif command -v i686-elf-ld >/dev/null 2>&1; then
  LINKER=i686-elf-ld
elif [[ "$(uname -s 2>/dev/null || true)" == Darwin* ]]; then
  if command -v ld.lld >/dev/null 2>&1; then LINKER=ld.lld
  elif command -v llvm-ld >/dev/null 2>&1; then LINKER=llvm-ld
  else echo "ERROR: macOS requires LLD for ELF32 linking (brew install llvm)." >&2; exit 1; fi
elif command -v ld >/dev/null 2>&1; then
  LINKER=ld
elif command -v ld.lld >/dev/null 2>&1; then
  LINKER=ld.lld
else
  echo "ERROR: linker not found. Ubuntu: sudo apt-get install binutils." >&2; exit 1
fi

COMPILER_ARGS=()
if [[ "$(basename "$COMPILER")" == clang* ]]; then COMPILER_ARGS=(--target=i386-unknown-none-elf); fi
CFLAGS=(-m32 -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-common -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables)

printf '[1/7] Compiling i386 kernel core...\n'
run "$COMPILER" "${COMPILER_ARGS[@]}" "${CFLAGS[@]}" -c source/kernel/kernel.c -o build/kernel.o
printf '[2/7] Compiling i386 networking stack...\n'
run "$COMPILER" "${COMPILER_ARGS[@]}" "${CFLAGS[@]}" -c source/net/net.c -o build/net.o
printf '[3/7] Assembling Multiboot v1 i386 entry...\n'
if command -v nasm >/dev/null 2>&1; then
  run nasm -D SC_BOOT_WIDTH="$BOOT_WIDTH" -D SC_BOOT_HEIGHT="$BOOT_HEIGHT" -f elf32 source/arch/boot.asm -o build/boot.o
else
  echo "NASM not found; using the GNU-as fallback source/arch/boot.S."
  run "$COMPILER" "${COMPILER_ARGS[@]}" -DSC_BOOT_WIDTH="$BOOT_WIDTH" -DSC_BOOT_HEIGHT="$BOOT_HEIGHT" -m32 -ffreestanding -fno-pic -fno-pie -c source/arch/boot.S -o build/boot.o
fi
printf '[4/7] Compiling live VBE mode-switch thunk...\n'
run "$COMPILER" "${COMPILER_ARGS[@]}" -m32 -ffreestanding -fno-pic -fno-pie -c source/arch/vbe.S -o build/vbe.o
printf '[5/7] Linking ELF32/i386 kernel...\n'
run "$LINKER" -m elf_i386 -T source/linker-i386.ld --build-id=none -o build/systemc-i386.krn build/boot.o build/kernel.o build/net.o build/vbe.o
printf '[6/7] Verifying kernel and staging outputs...\n'
PYTHON="${PYTHON:-python3}"
command -v "$PYTHON" >/dev/null 2>&1 || { echo "ERROR: Python 3 is required for the verifier." >&2; exit 1; }
run "$PYTHON" verify_i386_kernel.py build/systemc-i386.krn
cp build/systemc-i386.krn systemc-i386.krn
cp build/systemc-i386.krn prebuilt/systemc-i386.krn
cp build/systemc-i386.krn dist/systemc-i386.krn
if [[ "${SYSTEMC_KERNEL_ONLY:-0}" == 1 ]]; then
  echo "Kernel-only build complete: dist/systemc-i386.krn (ISO intentionally skipped)."
  exit 0
fi
printf '[7/7] Building and validating the legacy BIOS ISO...\n'
exec "$ROOT/build-iso.sh"
