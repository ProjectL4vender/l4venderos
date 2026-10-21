# SystemC 3.0 — BL0SS0M MOD.0 (Project L4vender, i386-only)

SystemC is a freestanding hobby operating system written in C and x86 assembly. It is not based on the Linux kernel or Linux userspace. GRUB is used only as the legacy-BIOS bootloader, loading the ELF32/i386 Multiboot v1 kernel. This source ZIP includes a verified prebuilt `systemc-i386.krn` and ISO build scripts; it does not include a prebuilt ISO because the ISO must be generated with local GRUB BIOS modules and xorriso.

## Desktop and apps

The desktop includes compact shortcuts, a Windows-7-height taskbar, Start menu, RTC clock, custom mouse pointer, right-click context menu, movable windows, themed colors, and a framebuffer backbuffer. Applications include Paint Studio, Notepad, Calculator, CMD-style Terminal, Files, Disk Utility, Browser, Network Status, Settings, Winver, About, System Monitor, Hardware Diagnostics, Calendar, Help Center, and Task Manager.

Paint supports mouse drawing with pencil, line, rectangle, eraser, fill, palette and clear. Text and drawings are kept in RAM; permanent file saving and a general filesystem are not yet implemented. The file manager is a virtual listing rather than a mountable filesystem.

## First-boot setup and sign-in

On first boot, enter a local username, password and machine name. The login screen asks for the session password. The current profile is stored only in RAM: there is no persistent account database, password hashing, or secure multi-user isolation, and setup repeats after a reboot. Do not use a real or reused password.

## Display settings

Open **Settings > Screen Resolution**, type width and height, and click **Apply Now**. The terminal command is:

```text
systemcsetup-cli --resolution 1024 768
```

Both paths attempt an immediate VBE mode switch through a real-mode BIOS thunk. They update the framebuffer only if VBE reports a matching supported 24/32-bit linear-framebuffer mode and sane pitch/address values; if the VM BIOS does not support it, SystemC keeps the current mode and reports that the mode is unavailable. Runtime VBE switching has been compile-checked but not hardware/VM boot-tested in this environment.

For the initial boot mode, set a GRUB/Multiboot request before building:

```sh
SYSTEMC_RESOLUTION=1024x768 ./build.sh
```

The requested mode must be supported by the virtual or physical graphics adapter. GRUB documents `gfxmode` as selecting modes exposed by the available video hardware ([GNU GRUB manual](https://www.gnu.org/software/grub/manual/grub/html_node/gfxmode.html)).

## Networking / ITWLMCLI

The kernel contains an experimental Intel E1000 driver and basic ARP, IPv4, DHCP, DNS, TCP, and plain HTTP support. Configure VMware with an **Intel E1000** virtual network adapter. Type the following in Terminal to reinitialize the built-in driver and immediately poll for a lease:

```text
itwlmcli install network auto
```

A supported adapter and DHCP server are required; success is reported only after the network code marks the lease online. This is not a dynamic driver module loader. The browser supports plain HTTP only: HTTPS/TLS is not implemented, so Google and other HTTPS-only sites will not load normally.

A supplemental network support data ISO can be made with:

```sh
./build-network-iso.sh
```

It outputs `dist/SystemC-3.0-network-support.iso`. It contains driver source and manifest metadata; it is **not** the bootable OS ISO. SystemC does not yet have an ISO9660/ATAPI package manager to load driver modules from the data disc.

## Build on Ubuntu / Ubuntu 18.04

```sh
sudo apt-get update
sudo apt-get install build-essential binutils nasm grub-pc-bin grub-common grub2-common xorriso mtools dosfstools util-linux python3
chmod +x build.sh build-iso.sh build-18.04.sh build-msys2.sh build-network-iso.sh
./build.sh
```

Output: `dist/SystemC-3.0-i386.iso`. The ISO builder checks the ISO-9660 Primary Volume Descriptor, reopens the image with xorriso, checks the BIOS El Torito entry, and extracts/compares the kernel, GRUB configuration and network manifest. These are structural checks, not a substitute for boot-testing in VMware. If ISO tools are absent, only the kernel-only build can be completed:

```sh
SYSTEMC_KERNEL_ONLY=1 ./build.sh
```

`./build-18.04.sh` installs the host dependencies for Ubuntu 18.04. `./build-msys2.sh` is intended for MSYS2 Bash. `build.bat` is a Windows entry point for MSYS2 Bash, not a native Windows compiler.

## Installer warning

The ISO installer writes a prepared disk image to a selected legacy IDE/PATA disk and destroys existing data. It does not support NVMe, AHCI-only, or USB storage. Only test with a disposable virtual disk.

## Validation status

The i386 kernel is built freestanding with warnings treated as errors; the release verifier checks ELF32, i386 machine type, and the Multiboot header. The live VBE thunk and physical network activity have not been tested in a VM in this build environment. A complete bootable ISO requires `grub-mkrescue` and `xorriso`; do not assume VMware boot success until the ISO has been generated and boot-tested.
