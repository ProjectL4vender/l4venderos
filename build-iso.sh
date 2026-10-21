#!/usr/bin/env bash
# Create an ISO9660 image with an explicit BIOS El Torito boot path (i386-pc GRUB).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
SYSTEMC_RESOLUTION="${SYSTEMC_RESOLUTION:-640x480}"
if [[ ! "$SYSTEMC_RESOLUTION" =~ ^([0-9]{3,4})x([0-9]{3,4})$ ]]; then
  echo "ERROR: SYSTEMC_RESOLUTION must look like 1920x1080 (got '$SYSTEMC_RESOLUTION')." >&2; exit 2
fi
BOOT_WIDTH="${BASH_REMATCH[1]}"; BOOT_HEIGHT="${BASH_REMATCH[2]}"
if (( BOOT_WIDTH < 320 || BOOT_WIDTH > 1920 || BOOT_HEIGHT < 240 || BOOT_HEIGHT > 1080 )); then
  echo "ERROR: requested boot mode is outside 320x240 through 1920x1080." >&2; exit 2
fi
KERNEL="$ROOT/dist/systemc-i386.krn"
ISO="$ROOT/dist/SystemC-3.0-i386.iso"
TMPISO="$ROOT/dist/.SystemC-3.0-i386.tmp.iso"
OLD_TMPISO="$ROOT/dist/SystemC-3.0-i386.iso.tmp"
# Do not leave an old final image behind if this run cannot build a fresh one.
rm -f "$ISO" "$TMPISO" "$OLD_TMPISO"
[[ -s "$KERNEL" ]] || { echo "ERROR: missing $KERNEL; run ./build.sh first." >&2; exit 1; }
GRUB_MKRESCUE="${GRUB_MKRESCUE:-grub-mkrescue}"
for t in "$GRUB_MKRESCUE" xorriso; do command -v "$t" >/dev/null 2>&1 || { echo "ERROR: '$t' is required to create a bootable ISO. Ubuntu: sudo apt-get install grub-pc-bin grub-common xorriso mtools." >&2; exit 2; }; done
# Fail early when a GRUB Legacy/third-party command named grub-mkrescue is ahead
# of GRUB 2 in PATH; those tools can emit a raw disk image instead of an ISO.
GRUB_VERSION="$($GRUB_MKRESCUE --version 2>&1 || true)"
if ! printf '%s\n' "$GRUB_VERSION" | grep -Eq 'GRUB.*[[:space:]]2\.[0-9]+'; then
  echo "ERROR: '$GRUB_MKRESCUE' does not identify itself as GNU GRUB 2 (reported: $GRUB_VERSION)." >&2
  echo "Install the GRUB 2 BIOS ISO tools, e.g.: sudo apt-get install grub-pc-bin grub-common grub2-common xorriso mtools" >&2
  echo "Check command resolution with: type -a grub-mkrescue; command -v grub-mkrescue" >&2
  exit 2
fi
PYTHON="${PYTHON:-python3}"
command -v "$PYTHON" >/dev/null 2>&1 || { echo "ERROR: Python 3 is required to validate ISO-9660 descriptors." >&2; exit 2; }
GRUB_PC_DIR="${SYSTEMC_GRUB_PC_DIR:-}"
if [[ -z "$GRUB_PC_DIR" ]]; then
  for d in /usr/lib/grub/i386-pc /usr/local/lib/grub/i386-pc /opt/homebrew/lib/grub/i386-pc /mingw64/lib/grub/i386-pc /usr/local/opt/grub/lib/grub/i386-pc; do
    if [[ -r "$d/boot.img" && -r "$d/cdboot.img" && -r "$d/multiboot.mod" && -r "$d/normal.mod" ]]; then GRUB_PC_DIR="$d"; break; fi
  done
fi
if [[ -z "$GRUB_PC_DIR" || ! -r "$GRUB_PC_DIR/boot.img" || ! -r "$GRUB_PC_DIR/cdboot.img" || ! -r "$GRUB_PC_DIR/multiboot.mod" || ! -r "$GRUB_PC_DIR/normal.mod" ]]; then
  echo "ERROR: BIOS GRUB modules were not found (need i386-pc/boot.img, cdboot.img, normal.mod, multiboot.mod). Install grub-pc-bin or set SYSTEMC_GRUB_PC_DIR." >&2
  exit 2
fi
if command -v grub-file >/dev/null 2>&1; then grub-file --is-x86-multiboot "$KERNEL" || { echo "ERROR: grub-file rejects the i386 kernel as non-Multiboot." >&2; exit 1; }; fi

# Build the on-disk installer image from the newly built kernel. On non-Linux hosts,
# the live ISO is still generated, but the in-OS raw-disk installer isn't bundled.
if [[ "${SYSTEMC_SKIP_INSTALLER_IMAGE:-0}" == 1 ]]; then
  rm -f "$ROOT/dist/systemc-hdd.img"
elif [[ "$(uname -s)" == Linux && -x "$ROOT/tools/make-hdd-image.sh" ]]; then
  rm -f "$ROOT/dist/systemc-hdd.img"
  echo "Preparing a 256 MiB GRUB BIOS installer image (sudo may ask for your password)..."
  if ! "$ROOT/tools/make-hdd-image.sh"; then
    echo "WARNING: raw-disk installer image could not be built. The live ISO will still be generated; the installer menu will report that it is unavailable." >&2
    rm -f "$ROOT/dist/systemc-hdd.img"
  fi
else
  rm -f "$ROOT/dist/systemc-hdd.img"
  echo "NOTE: raw-disk installer image creation needs Linux loop devices. Live ISO will build, but in-OS disk installation will be unavailable on this host." >&2
fi

# Never reuse stale files from a previous build. VMware must see a valid El Torito BIOS image.
rm -rf "$ROOT/build/iso"
mkdir -p "$ROOT/build/iso/boot/grub" "$ROOT/build/iso/itwlm/network"
cp "$KERNEL" "$ROOT/build/iso/boot/systemc-i386.krn"
cp "$ROOT/wallpapers/default.ppm" "$ROOT/build/iso/boot/default-wallpaper.ppm"
cp "$ROOT/source/net/net.c" "$ROOT/build/iso/itwlm/network/net.c"
cp "$ROOT/source/net/net.h" "$ROOT/build/iso/itwlm/network/net.h"
cat > "$ROOT/build/iso/itwlm/network/manifest.ini" <<'NETMANIFEST'
[SystemC.NetworkDriverPackage]
PackageVersion=3.0
Driver=e1000
Architecture=i386
Transport=PCI-Ethernet
InstallCommand=itwlmcli install network auto
Note=The kernel statically links the E1000 driver. The command reinitializes it and synchronously polls for a DHCP lease; this is not dynamic module loading.
NETMANIFEST
if [[ -s "$ROOT/dist/systemc-hdd.img" ]]; then cp "$ROOT/dist/systemc-hdd.img" "$ROOT/build/iso/boot/systemc-hdd.img"; fi
cat > "$ROOT/build/iso/boot/grub/grub.cfg" <<GRUB
set default=0
set timeout=5
set timeout_style=menu
insmod part_msdos
insmod iso9660
insmod multiboot
insmod all_video
set gfxmode=${BOOT_WIDTH}x${BOOT_HEIGHT}x32,${BOOT_WIDTH}x${BOOT_HEIGHT}x24,640x480x32,640x480x24,auto
set gfxpayload=keep
if insmod gfxterm; then
    terminal_output gfxterm
fi

menuentry "Start SystemC 3.0 BL0SS0M MOD.0 (i386)" {
    multiboot /boot/systemc-i386.krn
    boot
}
GRUB
if [[ -s "$ROOT/build/iso/boot/systemc-hdd.img" ]]; then
cat >> "$ROOT/build/iso/boot/grub/grub.cfg" <<'GRUB'

menuentry "Install SystemC to IDE disk (ERASES DATA)" {
    multiboot /boot/systemc-i386.krn install
    module /boot/systemc-hdd.img systemc-hdd-image
    boot
}
GRUB
else
cat >> "$ROOT/build/iso/boot/grub/grub.cfg" <<'GRUB'

menuentry "Install SystemC (installer image unavailable)" {
    multiboot /boot/systemc-i386.krn install
    boot
}
GRUB
fi
cat >> "$ROOT/build/iso/boot/grub/grub.cfg" <<'GRUB'

menuentry "Boot Windows from HDD (if installed)" {
    insmod part_msdos
    insmod fat
    insmod ntfs
    insmod chain
    search --no-floppy --file --set=root /bootmgr
    chainloader /bootmgr
}

menuentry "Reboot" { reboot }
GRUB

rm -f "$TMPISO"
MODULES="biosdisk iso9660 normal multiboot part_msdos fat ntfs chain reboot vbe gfxterm all_video"
echo "Creating BIOS-only ISO using GRUB modules from: $GRUB_PC_DIR"
"$GRUB_MKRESCUE" --directory="$GRUB_PC_DIR" --modules="$MODULES" --output="$TMPISO" "$ROOT/build/iso"
[[ -s "$TMPISO" ]] || { echo "ERROR: grub-mkrescue did not produce an ISO." >&2; exit 1; }
# Do NOT use `file | grep ISO 9660` as the validator here: hybrid BIOS ISOs
# can put GRUB's MBR code in sector 0, and `file` may report only that MBR
# signature even when a valid ISO-9660 PVD exists at logical block 16.
if command -v file >/dev/null 2>&1; then
  echo "Image identification (diagnostic only): $(file -b "$TMPISO" 2>/dev/null || true)"
fi
"$PYTHON" "$ROOT/tools/verify_iso9660.py" "$TMPISO" || {
  echo "ERROR: generated output has no valid ISO-9660 Primary Volume Descriptor; refusing to publish it." >&2
  if command -v file >/dev/null 2>&1; then file "$TMPISO" >&2 || true; fi
  echo "grub-mkrescue: $GRUB_VERSION" >&2
  exit 1
}
PVD_REPORT="$ROOT/build/iso-pvd-report.txt"
xorriso -indev "$TMPISO" -pvd_info >"$PVD_REPORT" 2>&1 || { echo "ERROR: xorriso could not reopen the generated ISO-9660 image." >&2; cat "$PVD_REPORT" >&2; exit 1; }
REPORT="$ROOT/build/el-torito-report.txt"
xorriso -indev "$TMPISO" -report_el_torito plain >"$REPORT" 2>&1 || { cat "$REPORT" >&2; exit 1; }
if ! grep -Eiq 'El Torito boot img[[:space:]]*:.*BIOS' "$REPORT"; then
  echo "ERROR: ISO lacks a BIOS El Torito boot entry; refusing to publish it." >&2
  cat "$REPORT" >&2
  exit 1
fi
CHECKDIR="$(mktemp -d "${TMPDIR:-/tmp}/systemc-iso-check.XXXXXX")"
trap 'rm -rf "$CHECKDIR"; rm -f "$TMPISO"' EXIT INT TERM
xorriso -osirrox on -indev "$TMPISO" -extract /boot/systemc-i386.krn "$CHECKDIR/kernel.krn" >/dev/null 2>&1
cmp "$KERNEL" "$CHECKDIR/kernel.krn"
xorriso -osirrox on -indev "$TMPISO" -extract /boot/grub/grub.cfg "$CHECKDIR/grub.cfg" >/dev/null 2>&1
cmp "$ROOT/build/iso/boot/grub/grub.cfg" "$CHECKDIR/grub.cfg"
xorriso -osirrox on -indev "$TMPISO" -extract /itwlm/network/manifest.ini "$CHECKDIR/network-manifest.ini" >/dev/null 2>&1 || { echo "ERROR: network manifest missing from ISO." >&2; exit 1; }
cmp "$ROOT/build/iso/itwlm/network/manifest.ini" "$CHECKDIR/network-manifest.ini"
if [[ -s "$ROOT/build/iso/boot/systemc-hdd.img" ]]; then
  xorriso -osirrox on -indev "$TMPISO" -extract /boot/systemc-hdd.img "$CHECKDIR/systemc-hdd.img" >/dev/null 2>&1
  cmp "$ROOT/build/iso/boot/systemc-hdd.img" "$CHECKDIR/systemc-hdd.img"
fi
mv "$TMPISO" "$ISO"
echo "ISO-9660 PVD and BIOS El Torito boot entry verified; embedded kernel and GRUB config compare byte-for-byte."
echo "Bootable legacy BIOS ISO created: $ISO"
cat "$REPORT"
