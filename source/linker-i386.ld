OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    }

    .text ALIGN(0x1000) :
    {
        *(.text .text.*)
    }

    .rodata ALIGN(0x1000) :
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(0x1000) :
    {
        *(.data .data.*)
        *(.got .got.*)
    }

    .bss ALIGN(0x1000) (NOLOAD) :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.gnu_debuglink)
        *(.gnu.lto_*)
    }
}
