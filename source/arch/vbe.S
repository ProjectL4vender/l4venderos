/* Runtime VBE mode switch thunk for legacy BIOS/i386.
 * Uses a low-memory scratch block only while the CPU is back in real mode.
 * ABI: int vbe_set_mode(u32 width, u32 height, struct sc_vbe_result *out)
 */
.section .text
.code32
.global vbe_set_mode
.type vbe_set_mode, @function
vbe_set_mode:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, vbe_saved_esp
    /* The 16-bit bridge is copied to low memory; its relative branches remain valid. */
    cld
    movl $vbe_pm16, %esi
    movl $0x7000, %edi
    movl $(vbe_pm32_return - vbe_pm16), %ecx
    rep movsb
    movl 8(%ebp), %eax
    movw %ax, 0x6500
    movl 12(%ebp), %eax
    movw %ax, 0x6502
    movl 16(%ebp), %eax
    movl %eax, vbe_result_ptr
    sidtl vbe_saved_idtr

    /* Copy a full-base GDTR and a real-mode IVT IDTR to low scratch memory. */
    movw vbe_gdt_descriptor, %ax
    movw %ax, 0x6400
    movl vbe_gdt_descriptor+2, %eax
    movl %eax, 0x6402
    movw $0x03ff, 0x6408
    movl $0, 0x640a
    movl $0, 0x6300

    cli
    lgdtl vbe_gdt_descriptor
    /* Selector 0x18 is a flat, 16-bit code segment; execute copied bridge at 0000:7000. */
    ljmpl $0x18, $0x7000

.code16
vbe_pm16:
    movl %cr0, %eax
    andl $0xfffffffe, %eax
    movl %eax, %cr0
    /* Real-mode far jump into the copied bridge at physical 0000:7000+offset. */
    .byte 0xea
    .word (0x7000 + vbe_rm_start - vbe_pm16)
    .word 0x0000

vbe_rm_start:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    lidtl 0x6408

    /* Clear VBE controller and scratch status. */
    movw $0x6000, %di
    movw $256, %cx
    rep stosw
    movw $0, 0x6300

    /* VBE controller info: request the BIOS's supported mode list. */
    movw $0x6000, %di
    movw $0x4f00, %ax
    sti
    int $0x10
    cli
    xorw %dx, %dx
    movw %dx, %ds
    movw %dx, %es
    cmpw $0x004f, %ax
    jne vbe_rm_fail
    cmpl $0x41534556, 0x6000      /* "VESA" */
    jne vbe_rm_fail
    movw 0x600e, %ax              /* VideoModePtr offset */
    movw %ax, 0x6020
    movw 0x6010, %ax              /* VideoModePtr segment */
    movw %ax, 0x6022
    movw $0, 0x6026              /* selected VBE mode */
    movw $0, 0x6028              /* 0 none, 1 24bpp fallback, 2 32bpp */

vbe_rm_mode_loop:
    movw 0x6020, %si
    movw 0x6022, %ax
    movw %ax, %es
    movw %es:(%si), %cx
    cmpw $0xffff, %cx
    je vbe_rm_modes_done
    addw $2, %si
    movw %si, 0x6020
    movw %cx, 0x6024

    /* Function 4F01 writes a 256-byte mode-info structure at 0000:6100. */
    xorw %ax, %ax
    movw %ax, %es
    movw $0x6100, %di
    movw 0x6024, %cx
    movw $0x4f01, %ax
    sti
    int $0x10
    cli
    xorw %dx, %dx
    movw %dx, %ds
    movw %dx, %es
    cmpw $0x004f, %ax
    jne vbe_rm_mode_loop
    movw 0x6100, %ax
    andw $0x0081, %ax            /* supported + linear framebuffer */
    cmpw $0x0081, %ax
    jne vbe_rm_mode_loop
    movw 0x6112, %ax
    cmpw 0x6500, %ax
    jne vbe_rm_mode_loop
    movw 0x6114, %ax
    cmpw 0x6502, %ax
    jne vbe_rm_mode_loop
    cmpb $24, 0x6119
    je vbe_rm_try_24
    cmpb $32, 0x6119
    jne vbe_rm_mode_loop
    /* Prefer 32bpp; save the mode info and stop scanning. */
    movw 0x6024, %ax
    movw %ax, 0x6026
    movw $2, 0x6028
    jmp vbe_rm_save_mode_info
vbe_rm_try_24:
    cmpw $0, 0x6028
    jne vbe_rm_mode_loop
    movw 0x6024, %ax
    movw %ax, 0x6026
    movw $1, 0x6028
vbe_rm_save_mode_info:
    pushw %ds
    popw %es
    movw $0x6100, %si
    movw $0x6200, %di
    movw $128, %cx
    rep movsw
    cmpw $2, 0x6028
    je vbe_rm_select_mode
    jmp vbe_rm_mode_loop

vbe_rm_modes_done:
    cmpw $0, 0x6028
    je vbe_rm_fail
vbe_rm_select_mode:
    /* Validate the saved candidate before changing the live display mode. */
    movzbl 0x6219, %ebx
    movzwl 0x6500, %ecx
    cmpl $32, %ebx
    je vbe_rm_pitch_32
    cmpl $24, %ebx
    jne vbe_rm_fail
    imull $3, %ecx, %ecx
    jmp vbe_rm_pitch_check
vbe_rm_pitch_32:
    imull $4, %ecx, %ecx
vbe_rm_pitch_check:
    movzwl 0x6210, %eax
    cmpl %ecx, %eax
    jb vbe_rm_fail
    cmpl $8192, %eax
    ja vbe_rm_fail
    cmpl $0, 0x6228
    je vbe_rm_fail
    /* Set selected VBE mode with bit 14 requesting the linear framebuffer. */
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw 0x6026, %bx
    orw $0x4000, %bx
    movw $0x4f02, %ax
    sti
    int $0x10
    cli
    xorw %dx, %dx
    movw %dx, %ds
    movw %dx, %es
    cmpw $0x004f, %ax
    jne vbe_rm_fail
    /* Basic result sanity checks before reporting success. */
    cmpw $0, 0x6212
    je vbe_rm_fail
    cmpw $0, 0x6214
    je vbe_rm_fail
    cmpw $0, 0x6210
    je vbe_rm_fail
    cmpl $0, 0x6228
    je vbe_rm_fail
    movl $1, 0x6300
    jmp vbe_rm_return_pm

vbe_rm_fail:
    movl $0, 0x6300

vbe_rm_return_pm:
    cli
    lgdtl 0x6400
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $vbe_pm32_return

.code32
vbe_pm32_return:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    lidtl vbe_saved_idtr
    movl vbe_saved_esp, %esp
    movl vbe_result_ptr, %edi
    cmpl $1, 0x6300
    jne vbe_pm32_fail
    movzwl 0x6212, %eax
    movl %eax, 0(%edi)
    movzwl 0x6214, %eax
    movl %eax, 4(%edi)
    movzwl 0x6210, %eax
    movl %eax, 8(%edi)
    movzbl 0x6219, %eax
    movl %eax, 12(%edi)
    movl 0x6228, %eax
    movl %eax, 16(%edi)
    movl $1, %eax
    jmp vbe_pm32_epilogue
vbe_pm32_fail:
    xorl %eax, %eax
vbe_pm32_epilogue:
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret
.size vbe_set_mode, .-vbe_set_mode

.align 8
vbe_gdt:
    .quad 0x0000000000000000
    .quad 0x00CF9A000000FFFF      /* flat 32-bit code */
    .quad 0x00CF92000000FFFF      /* flat 32-bit data */
    .quad 0x00009A000000FFFF      /* 16-bit code, base 0, 64 KiB limit */
    .quad 0x000092000000FFFF      /* 16-bit data, base 0, 64 KiB limit */
vbe_gdt_end:
vbe_gdt_descriptor:
    .word vbe_gdt_end - vbe_gdt - 1
    .long vbe_gdt
vbe_saved_esp: .long 0
vbe_result_ptr: .long 0
vbe_saved_idtr: .space 6
