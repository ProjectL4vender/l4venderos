BITS 32

; SystemC 3.0 BL0SS0M MOD.0 - L4vender — Multiboot v1 entry (i386 only)
; GRUB loads this ELF kernel in 32-bit protected mode.

%ifndef SC_BOOT_WIDTH
%define SC_BOOT_WIDTH 640
%endif
%ifndef SC_BOOT_HEIGHT
%define SC_BOOT_HEIGHT 480
%endif

SECTION .multiboot progbits alloc noexec nowrite align=4
ALIGN 4
    dd 0x1BADB002                  ; Multiboot header magic
    dd 0x00000007                  ; align modules, memory info, request video mode
    dd -(0x1BADB002 + 0x00000007)  ; checksum
    dd 0                            ; linear graphics mode
    dd SC_BOOT_WIDTH                ; width
    dd SC_BOOT_HEIGHT               ; height
    dd 32                           ; bits per pixel

SECTION .text progbits alloc exec nowrite align=16
GLOBAL _start
EXTERN kernel_main

_start:
    cli
    cld
    mov esp, stack_top
    xor ebp, ebp
    ; cdecl: kernel_main(magic, multiboot_info_address)
    push ebx
    push eax
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt

SECTION .bss nobits alloc noexec write align=16
ALIGNB 16
stack_bottom: resb 16384
stack_top:
