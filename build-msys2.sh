#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
if ! command -v pacman >/dev/null 2>&1; then
  echo "ERROR: run this script from an MSYS2 shell." >&2
  exit 1
fi
echo "Installing the freestanding C/assembly build tools..."
pacman -S --needed --noconfirm make nasm python clang lld binutils xorriso
if ! command -v clang >/dev/null 2>&1 || ! command -v ld.lld >/dev/null 2>&1; then
  echo "ERROR: Clang and LLD are required for the freestanding ELF32/i386 target." >&2
  exit 1
fi
if ! command -v grub-mkrescue >/dev/null 2>&1 || ! command -v xorriso >/dev/null 2>&1; then
  echo "ERROR: this MSYS2 environment lacks GRUB BIOS ISO tooling. Install/use a GRUB build containing i386-pc modules; kernel-only output is deliberately not named an ISO." >&2
  exit 2
fi
CC="$(command -v clang)" LD="$(command -v ld.lld)" SYSTEMC_SKIP_INSTALLER_IMAGE=1 ./build.sh
