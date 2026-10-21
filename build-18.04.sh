#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
if ! command -v apt-get >/dev/null 2>&1; then
  echo "ERROR: build-18.04.sh is intended for Ubuntu 18.04 or a compatible APT-based host." >&2
  exit 1
fi
PACKAGES=(build-essential binutils nasm grub-pc-bin grub-common grub2-common xorriso mtools dosfstools util-linux python3)
echo "Installing SystemC build dependencies (does not change the host's bootloader)."
# Ubuntu 18.04's AppStream post-update hook can abort apt-get update even after
# downloading package indexes. Retry with the hook disabled for this invocation only.
if ! sudo apt-get update; then
  echo "APT update failed; retrying with the AppStream post-update hook disabled for this command only..." >&2
  sudo apt-get -o 'APT::Update::Post-Invoke-Success::=' update
fi
sudo apt-get install -y "${PACKAGES[@]}"
for tool in gcc ld grub-mkrescue grub-install xorriso python3; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: '$tool' is still unavailable after dependency installation." >&2; exit 1; }
done
exec ./build.sh
