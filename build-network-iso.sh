#!/usr/bin/env bash
# Build the separate SystemC ITWLM network-driver/source ISO (supplemental, data-only).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
mkdir -p build dist
for tool in xorriso python3; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "ERROR: '$tool' is required. Ubuntu: sudo apt-get install xorriso python3" >&2
    exit 2
  }
done
STAGE="$ROOT/build/network-iso-root"
ISO="$ROOT/dist/SystemC-3.0-network-support.iso"
TMP="$ROOT/dist/.SystemC-3.0-network-support.tmp.iso"
rm -rf "$STAGE"
rm -f "$ISO" "$TMP"
mkdir -p "$STAGE/ITWLM/source/net"
cp "$ROOT/network-drivers/README.txt" "$STAGE/README.txt"
cp "$ROOT/network-drivers/manifest.ini" "$STAGE/ITWLM/manifest.ini"
cp "$ROOT/source/net/net.c" "$STAGE/ITWLM/source/net/net.c"
cp "$ROOT/source/net/net.h" "$STAGE/ITWLM/source/net/net.h"
cp "$ROOT/network-drivers/itwlmcli.txt" "$STAGE/ITWLM/itwlmcli.txt"
printf 'Building supplemental, data-only network support ISO...\n'
xorriso -as mkisofs -R -J -V SYSTEMCNET30 -o "$TMP" "$STAGE"
[[ -s "$TMP" ]] || { echo "ERROR: xorriso did not create the network ISO." >&2; exit 1; }
python3 "$ROOT/tools/verify_iso9660.py" "$TMP"
xorriso -indev "$TMP" -pvd_info >/dev/null 2>&1 || { echo "ERROR: xorriso cannot reopen its output." >&2; exit 1; }
REPORT="$ROOT/build/network-iso-report.txt"
xorriso -indev "$TMP" -pvd_info > "$REPORT" 2>&1
grep -Eiq "Volume Id[[:space:]]*:[[:space:]]*'?SYSTEMCNET30'?" "$REPORT" || { echo "ERROR: unexpected volume label in network ISO." >&2; cat "$REPORT" >&2; exit 1; }
CHECK="$ROOT/build/network-iso-check"
rm -rf "$CHECK" && mkdir -p "$CHECK"
xorriso -osirrox on -indev "$TMP" -extract /ITWLM/manifest.ini "$CHECK/manifest.ini" >/dev/null 2>&1 || { echo "ERROR: network ISO does not contain /ITWLM/manifest.ini." >&2; exit 1; }
cmp "$ROOT/network-drivers/manifest.ini" "$CHECK/manifest.ini"
mv "$TMP" "$ISO"
echo "Supplemental SystemC network-driver ISO created: $ISO"
echo "Note: this is a data ISO, not a bootable operating-system ISO."
