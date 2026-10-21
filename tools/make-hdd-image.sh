#!/usr/bin/env bash
# Builds a 256 MiB, MBR/FAT32, GRUB BIOS bootable disk image for SystemC's IDE-PIO installer.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMG="$ROOT/dist/systemc-hdd.img"
KERNEL="$ROOT/dist/systemc-i386.krn"
WALLPAPER="$ROOT/wallpapers/default.ppm"
if [[ "$(uname -s)" != Linux ]]; then
  echo "Disk-image creation requires Linux loop devices; skipped on this host." >&2
  exit 2
fi
for tool in losetup sfdisk mkfs.vfat mount umount grub-install; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 2; }
done
[[ -s "$KERNEL" ]] || { echo "Missing kernel: $KERNEL" >&2; exit 2; }
[[ -s "$WALLPAPER" ]] || { echo "Missing wallpaper: $WALLPAPER" >&2; exit 2; }
if [[ "$(id -u)" == 0 ]]; then SUDO=(); else command -v sudo >/dev/null 2>&1 || { echo "sudo is required to create the disk image." >&2; exit 2; }; SUDO=(sudo); fi
LOOP=""
MNT="$(mktemp -d "${TMPDIR:-/tmp}/systemc-hdd.XXXXXX")"
cleanup() {
  if mountpoint -q "$MNT" 2>/dev/null; then "${SUDO[@]}" umount "$MNT" || true; fi
  if [[ -n "$LOOP" ]]; then "${SUDO[@]}" losetup -d "$LOOP" 2>/dev/null || true; fi
  rmdir "$MNT" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
mkdir -p "$ROOT/dist"
rm -f "$IMG"
truncate -s 256M "$IMG"
# 256 MiB = 524288 sectors; reserve 1 MiB for GRUB's embedded core.img.
printf 'label: dos\nunit: sectors\n\nstart=2048, size=522240, type=c, bootable\n' | "${SUDO[@]}" sfdisk "$IMG"
LOOP=$("${SUDO[@]}" losetup --find --show --partscan "$IMG")
PART="${LOOP}p1"
for _ in $(seq 1 50); do [[ -b "$PART" ]] && break; sleep 0.1; done
if [[ ! -b "$PART" ]]; then
  if command -v partprobe >/dev/null 2>&1; then "${SUDO[@]}" partprobe "$LOOP" || true; fi
  for _ in $(seq 1 50); do [[ -b "$PART" ]] && break; sleep 0.1; done
fi
[[ -b "$PART" ]] || { echo "Could not create partition device for $LOOP" >&2; exit 2; }
"${SUDO[@]}" mkfs.vfat -F 32 -n SYSTEMC "$PART" >/dev/null
"${SUDO[@]}" mount "$PART" "$MNT"
"${SUDO[@]}" mkdir -p "$MNT/boot/grub"
"${SUDO[@]}" cp "$KERNEL" "$MNT/boot/systemc-i386.krn"
"${SUDO[@]}" cp "$WALLPAPER" "$MNT/boot/default-wallpaper.ppm"
"${SUDO[@]}" sh -c "cat > '$MNT/boot/grub/grub.cfg'" <<'GRUB'
set default=0
set timeout=3
insmod part_msdos
insmod fat
insmod multiboot
insmod all_video
set gfxmode=640x480x32,640x480x24,auto
if insmod gfxterm; then terminal_output gfxterm; fi
menuentry "Start SystemC 3.0 BL0SS0M MOD.0 (i386)" {
    search --no-floppy --file --set=root /boot/systemc-i386.krn
    multiboot /boot/systemc-i386.krn
    boot
}
menuentry "Reboot" { reboot }
GRUB
"${SUDO[@]}" grub-install --target=i386-pc --boot-directory="$MNT/boot" --recheck --no-floppy "$LOOP"
"${SUDO[@]}" umount "$MNT"
"${SUDO[@]}" losetup -d "$LOOP"
LOOP=""
chmod 644 "$IMG"
echo "Created GRUB BIOS disk image: $IMG (256 MiB)"
echo "This image is bundled into the live ISO for the explicit, destructive IDE-disk installer."
