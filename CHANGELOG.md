# Changelog — SystemC / Project L4vender

## 3.0 — BL0SS0M MOD.0 (i386-only)

- First-boot setup prompts for local username, password, and machine name; subsequent sign-in checks the password for the current boot session.
- Reworked the desktop theme with compact icons, a Windows 7-height taskbar, new pointer, Start menu, right-click context menu, movable windows, and buffered framebuffer presentation.
- Reworked Settings with a sidebar for colors and display resolution. Resolution changes call the kernel VBE mode-switch path immediately when the BIOS/VBE implementation supports the requested mode.
- `systemcsetup-cli --resolution WIDTH HEIGHT` invokes the same live VBE path; unavailable modes are rejected without claiming a successful switch.
- `itwlmcli install network auto` reinitializes the built-in Intel E1000 driver and polls immediately for a DHCP lease.
- Added Calendar, Help Center, Task Manager, Winver, About, System Monitor, and Hardware Diagnostics windows alongside Paint Studio, Notepad, Calculator, Files, Disk Utility, Browser, Terminal, and Network Status.
- Winver reports version, codename, copyright `(c) 2023-2026 L4vender Project`, estimated RAM, user, and machine name. About reports CPU identification, RAM, framebuffer size, architecture, and boot method.
- Keeps a freestanding C + x86 assembly kernel; GRUB remains the legacy BIOS bootloader. This project is not based on the Linux kernel.

## 2.1.8

- Replaced oversized shortcut cards with compact Windows-style desktop icons and a taskbar.
- Added right-click menu, theme settings, a CMD-like terminal, buffered framebuffer, and built-in E1000 networking commands.
- Added the supplemental network support ISO builder.

## 2.1.7

- Added taskbar, RTC-backed clock, desktop shortcuts, movable windows, and mouse cursor.
- Added GUI Paint Studio tools and retained calculator, terminal, file manager, disk utility, browser, Notepad, and network status.
- Added explicit ISO-9660 / El Torito validation for the legacy BIOS ISO.

## 2.1.6

- Removed Python 3.7-only future-annotations dependency from the kernel verifier.
- Added ELF32/i386 and Multiboot v1 header validation and a bundled prebuilt kernel.
