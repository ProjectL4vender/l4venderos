/* GNU assembler fallback for hosts without NASM. Keep in sync with boot.asm. */
.section .multiboot,"a"
.align 4
.long 0x1BADB002
.long 0x00000007
.long -(0x1BADB002 + 0x00000007)
.long 0
.long SC_BOOT_WIDTH
.long SC_BOOT_HEIGHT
.long 32

.section .text
.code32
.global _start
.extern kernel_main
_start:
    cli
    cld
    movl $stack_top, %esp
    xorl %ebp, %ebp
    pushl %ebx
    pushl %eax
    call kernel_main
1:
    cli
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
