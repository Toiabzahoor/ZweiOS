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
#include "drivers/mouse.hpp"
#include "drivers/vbe.hpp"
#include "drivers/pit.hpp"
#include "drivers/rtc.hpp"

#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "mm/heap.hpp"

#include "lib/kprintf.hpp"
#include "lib/panic.hpp"
#include "lib/string.hpp"
#include "syscall/syscall.hpp"
#include "proc/process.hpp"
#include "proc/sched.hpp"
#include "fs/vfs.hpp"
#include "fs/ramfs.hpp"
#include "fs/partition.hpp"
#include "fs/fat32.hpp"
#include "drivers/ata.hpp"
#include "gui/gfx.hpp"
#include "gui/wm.hpp"
#include "shell/shell.hpp"
#include "toolchain/compiler.hpp"
#include "net/net.hpp"

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

extern "C" void kmain(uint64_t magic, void* boot_info) {
    (void)magic;

    call_global_constructors();

    drivers::serial_puts("[ZweiOS] initializing ZweiOS v1.0 - made by toiabzahoor\r\n");

    arch::gdt_init();

    arch::idt_init();
    arch::isr_init();

    arch::pic_init();

    drivers::serial_init();

    drivers::keyboard_init();

    drivers::pit_init();

    drivers::rtc_init();

    drivers::vbe_init();
    if (drivers::vbe_is_available()) {
        if (!drivers::vbe_set_mode(1920, 1080, 32)) {
            drivers::vbe_set_mode(1024, 768, 32);
        }
    }
    gui::gfx_init();
    gui::wm_init();

    drivers::vga_init();
    drivers::vga_puts("[ZweiOS] initializing ZweiOS v1.0 - made by toiabzahoor\n\r");

    arch::isr_register_handler(44, [](arch::cpu_registers_t*) {
        drivers::mouse_handle_irq();
        arch::pic_send_eoi(12);
    });

    mm::pmm_init(boot_info);

    mm::vmm_init();

    mm::heap_init();

    syscall::syscall_init();

    proc::process_init();
    proc::sched_init();

    fs::vfs_init();
    fs::ramfs_init();

    drivers::ata_init();
    fs::partition_init();
    fs::fat32_init();

    if (drivers::ata_is_available()) {
        fs::partition_probe(0);
        size_t part_cnt = fs::partition_get_count();
        char next_drive = 'D';
        for (size_t i = 0; i < part_cnt; ++i) {
            fs::PartitionInfo pinfo;
            if (fs::partition_get(i, &pinfo)) {
                if (pinfo.fs_type == fs::FilesystemType::FAT32) {
                    fs::VNode* fat_root = fs::fat32_mount(pinfo.drive_id, pinfo.start_lba, pinfo.sector_count);
                    if (fat_root) {
                        char mpath[32] = "/mnt/disk0";
                        mpath[9] = static_cast<char>('0' + (i % 10));
                        fs::vfs_mount(mpath, next_drive, fat_root);

                        char malias[32] = "/disk0";
                        malias[5] = static_cast<char>('0' + (i % 10));
                        fs::vfs_mount(malias, '\0', fat_root);

                        drivers::serial_puts("[FAT32] Volume mounted at /mnt/disk0 (Drive D:)\r\n");

                        next_drive = static_cast<char>(next_drive + 1);
                    }
                }
            }
        }
    }

    net::net_init();
    toolchain::toolchain_init();
    shell::shell_init();

    if (drivers::vbe_is_available()) {
        gui::wm_init();
        if (gui::wm_start()) {
            gui::wm_run_loop();
        }
    }

    shell::shell_run();
}
