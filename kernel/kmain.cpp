/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Target: x86_64 Long Mode (Freestanding C++20)
 * Subsystem: Kernel Master Entry Point & Subsystem Initializer
 * ============================================================================== */

#include <stdint.h>
#include <stddef.h>

#include "arch/x86_64/io.hpp"
#include "arch/x86_64/gdt.hpp"
#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/isr.hpp"
#include "arch/x86_64/cpuid.hpp"

#include "drivers/vga.hpp"
#include "drivers/serial.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/pit.hpp"
#include "drivers/rtc.hpp"

#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "mm/heap.hpp"

#include "lib/kprintf.hpp"
#include "lib/panic.hpp"
#include "syscall/syscall.hpp"
#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"
#include "drivers/ata.hpp"
#include "shell/shell.hpp"

// Global C++ Static Constructor Invocation
typedef void (*constructor_fn)();
extern "C" constructor_fn __init_array_start[];
extern "C" constructor_fn __init_array_end[];

static void call_global_constructors() {
    for (constructor_fn* fn = __init_array_start; fn < __init_array_end; ++fn) {
        if (*fn) {
            (*fn)();
        }
    }
}

// Kernel Main Entry Point
extern "C" void kmain(uint64_t magic, void* boot_info) {
    (void)magic;

    // 0. Static C++ global constructors
    call_global_constructors();

    // 1. Core Boot Banner
    drivers::serial_puts("[ZweiOS] initializing ZweiOS v1.0 - made by toiabzahoor\r\n");

    // 2. CPU Architecture & Segmentation (GDT & TSS)
    arch::gdt_init();

    // 3. Interrupts & Exceptions (IDT & ISRs)
    arch::idt_init();
    arch::isr_init();

    // 4. Programmable Interrupt Controller (8259 PIC Remap)
    arch::pic_init();

    // 5. VGA Text Console (0xB8000)
    drivers::vga_init();
    drivers::vga_puts("[ZweiOS] initializing ZweiOS v1.0 - made by toiabzahoor\n\r");

    // 6. Serial COM1 UART (0x3F8)
    drivers::serial_init();

    // 7. PS/2 8042 Keyboard Driver
    drivers::keyboard_init();

    // 8. 8254 Programmable Interval Timer (PIT) Driver
    drivers::pit_init();

    // 9. Real-Time Clock (CMOS RTC) Driver
    drivers::rtc_init();

    // 10. Physical Memory Manager (PMM)
    mm::pmm_init(boot_info);

    // 11. Virtual Memory Manager (VMM 4-level paging & HHDM)
    mm::vmm_init();

    // 12. Kernel Dynamic Heap Allocator
    mm::heap_init();

    // 13. Fast System Call Engine (IA32_LSTAR / MSRs)
    syscall::syscall_init();

    // 14. Virtual File System (VFS) & Root RAMFS
    fs::vfs_init();
    fs::ramfs_init();

    // 15. ATA / IDE Storage Controller
    drivers::ata_init();

    // 16. Interactive Kernel Shell
    shell::shell_init();

    // 17. Run Interactive Shell Event Loop
    shell::shell_run();
}

