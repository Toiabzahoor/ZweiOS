/* ==============================================================================
 * ZweiOS Linker Script (x86_64 Higher-Half Kernel)
 * Virtual Base: 0xFFFFFFFF80000000 (-2GB) | Physical Load Base: 0x00100000 (1MB)
 * ============================================================================== */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYSICAL_BASE = 0x00100000; /* 1 MB Load Address */

SECTIONS
{
    /* Physical address region for early bootstrap (32-bit mode) */
    . = KERNEL_PHYSICAL_BASE;

    .boot ALIGN(4K) : {
        KEEP(*(.boot_header))
        *(.boot_text)
        *(.boot_data)
        *(.boot_bss)
    }

    /* Transition Location Counter to Higher-Half Virtual Address Space */
    . = KERNEL_VIRTUAL_BASE + .;
    . = ALIGN(4K);

    __kernel_start = .;

    /* Executable Code Segment (.text) */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) {
        __text_start = .;
        *(.multiboot2)
        *(.limine_requests_start_marker)
        *(.limine_requests)
        *(.limine_requests_end_marker)
        *(.text .text.*)
        __text_end = .;
    }

    /* Read-Only Data Segment (.rodata) */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE) {
        __rodata_start = .;
        *(.rodata .rodata.*)

        /* C++ Global Static Constructors */
        . = ALIGN(8);
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array) SORT_BY_INIT_PRIORITY(.ctors)))
        __init_array_end = .;

        /* C++ Global Static Destructors */
        . = ALIGN(8);
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array) SORT_BY_INIT_PRIORITY(.dtors)))
        __fini_array_end = .;

        __rodata_end = .;
    }

    /* Initialized Mutable Data Segment (.data) */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    /* Uninitialized Data Segment (.bss) */
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    . = ALIGN(4K);
    __kernel_end = .;
    __kernel_size = __kernel_end - __kernel_start;

    /* Discard unneeded host metadata */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.note.*)
    }
}
