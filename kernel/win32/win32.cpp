/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Native In-Kernel Win32 Subsystem & KERNEL32 API Thunks Implementation
 * ============================================================================== */

#include "win32/win32.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "arch/x86_64/io.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "mm/heap.hpp"
#include "drivers/pit.hpp"

// Global state for context switching and unwinding on ExitProcess
extern "C" {
uint64_t g_win32_saved_kernel_rsp = 0;
uint64_t g_win32_exit_code = 0;
}

namespace win32 {

// Dedicated 4 KB-aligned memory pages for TEB & PEB
alignas(4096) static TEB g_teb;
alignas(4096) static PEB g_peb;

static char g_command_line[256] = "hello_win.exe";

void win32_set_command_line(const char* cmdline) {
    if (!cmdline) return;
    lib::strncpy(g_command_line, cmdline, sizeof(g_command_line) - 1);
    g_command_line[sizeof(g_command_line) - 1] = '\0';
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c + ('a' - 'A'));
    }
    return c;
}

static bool str_equals_ignore_case(const char* s1, const char* s2) {
    if (!s1 || !s2) return false;
    while (*s1 && *s2) {
        if (to_lower(*s1) != to_lower(*s2)) {
            return false;
        }
        s1++;
        s2++;
    }
    return to_lower(*s1) == to_lower(*s2);
}

void win32_init_thread_environment(uint64_t image_base, uint64_t stack_base, uint64_t stack_limit) {
    lib::memset(&g_peb, 0, sizeof(PEB));
    g_peb.InheritedAddressSpace = 0;
    g_peb.BeingDebugged = 0;
    g_peb.ImageBaseAddress = reinterpret_cast<void*>(image_base);

    lib::memset(&g_teb, 0, sizeof(TEB));
    g_teb.Self = &g_teb;
    g_teb.ProcessEnvironmentBlock = &g_peb;
    g_teb.StackBase = reinterpret_cast<void*>(stack_base);
    g_teb.StackLimit = reinterpret_cast<void*>(stack_limit);
    g_teb.UniqueProcess = 1;
    g_teb.UniqueThread = 1;
    g_teb.LastErrorValue = 0;

    // Configure CPU IA32_GS_BASE MSR to point to this TEB
    arch::wrmsr(arch::IA32_GS_BASE, reinterpret_cast<uint64_t>(&g_teb));

    lib::kprint_str("[WIN32] Initialized TEB at ");
    lib::kprint_ptr(&g_teb);
    lib::kprint_str(", PEB at ");
    lib::kprint_ptr(&g_peb);
    lib::kprint_str(", IA32_GS_BASE MSR set.\r\n");
}

// ------------------------------------------------------------------------------
// Win32 Symbol Resolution Dispatch Table
// ------------------------------------------------------------------------------

struct Win32SymbolEntry {
    const char* dll_name;
    const char* func_name;
    uint16_t    ordinal;
    uint64_t    func_ptr;
};

static const Win32SymbolEntry g_win32_symbols[] = {
    { "KERNEL32.DLL", "GetStdHandle",       0, reinterpret_cast<uint64_t>(&k32_GetStdHandle) },
    { "KERNEL32.DLL", "WriteFile",          0, reinterpret_cast<uint64_t>(&k32_WriteFile) },
    { "KERNEL32.DLL", "WriteConsoleA",      0, reinterpret_cast<uint64_t>(&k32_WriteConsoleA) },
    { "KERNEL32.DLL", "ExitProcess",        0, reinterpret_cast<uint64_t>(&k32_ExitProcess) },
    { "KERNEL32.DLL", "GetCommandLineA",    0, reinterpret_cast<uint64_t>(&k32_GetCommandLineA) },
    { "KERNEL32.DLL", "VirtualAlloc",       0, reinterpret_cast<uint64_t>(&k32_VirtualAlloc) },
    { "KERNEL32.DLL", "VirtualFree",        0, reinterpret_cast<uint64_t>(&k32_VirtualFree) },
    { "KERNEL32.DLL", "GetLastError",       0, reinterpret_cast<uint64_t>(&k32_GetLastError) },
    { "KERNEL32.DLL", "SetLastError",       0, reinterpret_cast<uint64_t>(&k32_SetLastError) },
    { "KERNEL32.DLL", "GetModuleHandleA",   0, reinterpret_cast<uint64_t>(&k32_GetModuleHandleA) },
    { "KERNEL32.DLL", "GetProcAddress",     0, reinterpret_cast<uint64_t>(&k32_GetProcAddress) },
    { "KERNEL32.DLL", "Sleep",              0, reinterpret_cast<uint64_t>(&k32_Sleep) },
    { "KERNEL32.DLL", "GetTickCount64",     0, reinterpret_cast<uint64_t>(&k32_GetTickCount64) },
    { "KERNEL32.DLL", "GetTickCount",       0, reinterpret_cast<uint64_t>(&k32_GetTickCount) },
    { "KERNEL32.DLL", "GetProcessHeap",     0, reinterpret_cast<uint64_t>(&k32_GetProcessHeap) },
    { "KERNEL32.DLL", "HeapAlloc",          0, reinterpret_cast<uint64_t>(&k32_HeapAlloc) },
    { "KERNEL32.DLL", "HeapFree",           0, reinterpret_cast<uint64_t>(&k32_HeapFree) },
};

static constexpr size_t NUM_WIN32_SYMBOLS = sizeof(g_win32_symbols) / sizeof(g_win32_symbols[0]);

uint64_t win32_resolve_symbol(const char* dll_name, const char* func_name, uint16_t ordinal) {
    for (size_t i = 0; i < NUM_WIN32_SYMBOLS; ++i) {
        const auto& entry = g_win32_symbols[i];

        // Check DLL name match (case-insensitive)
        if (!str_equals_ignore_case(dll_name, entry.dll_name)) {
            // Also accept without .dll extension (e.g. "KERNEL32" vs "KERNEL32.DLL")
            size_t len1 = lib::strlen(dll_name);
            size_t len2 = lib::strlen(entry.dll_name);
            bool match = false;
            if (len1 + 4 == len2 && lib::strncmp(entry.dll_name + len1, ".DLL", 4) == 0) {
                if (lib::strncmp(dll_name, entry.dll_name, len1) == 0) {
                    match = true;
                }
            }
            if (!match) continue;
        }

        // Match by function name if present
        if (func_name && entry.func_name) {
            if (lib::strcmp(func_name, entry.func_name) == 0) {
                return entry.func_ptr;
            }
        }

        // Match by ordinal if specified
        if (ordinal > 0 && entry.ordinal == ordinal) {
            return entry.func_ptr;
        }
    }

    return 0;
}

} // namespace win32

// ------------------------------------------------------------------------------
// In-Kernel KERNEL32 API Thunk Implementations (MS x64 ABI)
// ------------------------------------------------------------------------------

extern "C" {

win32::HANDLE __attribute__((ms_abi)) k32_GetStdHandle(win32::DWORD nStdHandle) {
    if (nStdHandle == win32::STD_INPUT_HANDLE_VAL) {
        return win32::PSEUDO_STDIN_HANDLE;
    }
    if (nStdHandle == win32::STD_OUTPUT_HANDLE_VAL) {
        return win32::PSEUDO_STDOUT_HANDLE;
    }
    if (nStdHandle == win32::STD_ERROR_HANDLE_VAL) {
        return win32::PSEUDO_STDERR_HANDLE;
    }
    return nullptr;
}

win32::BOOL __attribute__((ms_abi)) k32_WriteFile(
    win32::HANDLE hFile,
    win32::LPCVOID lpBuffer,
    win32::DWORD nNumberOfBytesToWrite,
    win32::LPDWORD lpNumberOfBytesWritten,
    win32::LPVOID lpOverlapped
) {
    (void)lpOverlapped;

    if (!lpBuffer) {
        return win32::FALSE_VAL;
    }

    // Output to stdout or stderr
    if (hFile == win32::PSEUDO_STDOUT_HANDLE ||
        hFile == win32::PSEUDO_STDERR_HANDLE ||
        hFile == reinterpret_cast<win32::HANDLE>(1ULL) ||
        hFile == reinterpret_cast<win32::HANDLE>(2ULL)) {

        const char* p = reinterpret_cast<const char*>(lpBuffer);
        for (win32::DWORD i = 0; i < nNumberOfBytesToWrite; ++i) {
            char c = p[i];
            drivers::serial_putc(c);
            drivers::vga_putc(c);
        }

        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        }
        return win32::TRUE_VAL;
    }

    return win32::FALSE_VAL;
}

win32::BOOL __attribute__((ms_abi)) k32_WriteConsoleA(
    win32::HANDLE hConsoleOutput,
    const void* lpBuffer,
    win32::DWORD nNumberOfCharsToWrite,
    win32::LPDWORD lpNumberOfCharsWritten,
    win32::LPVOID lpReserved
) {
    (void)lpReserved;
    return k32_WriteFile(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, nullptr);
}

void __attribute__((ms_abi)) k32_ExitProcess(win32::UINT uExitCode) {
    lib::kprint_str("[WIN32] ExitProcess called with status: ");
    lib::kprint_udec(static_cast<uint64_t>(uExitCode));
    lib::kprint_str("\r\n");

    g_win32_exit_code = static_cast<uint64_t>(uExitCode);
    win32_exit_trampoline();
}

const char* __attribute__((ms_abi)) k32_GetCommandLineA(void) {
    return win32::g_command_line;
}

win32::LPVOID __attribute__((ms_abi)) k32_VirtualAlloc(
    win32::LPVOID lpAddress,
    win32::SIZE_T dwSize,
    win32::DWORD flAllocationType,
    win32::DWORD flProtect
) {
    (void)flAllocationType;
    (void)flProtect;

    if (dwSize == 0) {
        return nullptr;
    }

    // Allocate anonymous 4KB pages
    size_t num_pages = (dwSize + mm::PAGE_SIZE - 1) / mm::PAGE_SIZE;
    uint64_t phys = mm::pmm_alloc_contiguous_frames(num_pages);
    if (phys == 0) {
        return nullptr;
    }

    uint64_t virt = lpAddress ? reinterpret_cast<uint64_t>(lpAddress) : phys;
    for (size_t i = 0; i < num_pages; ++i) {
        mm::vmm_map_page(virt + (i * mm::PAGE_SIZE), phys + (i * mm::PAGE_SIZE),
                         mm::PTE_PRESENT | mm::PTE_WRITABLE | mm::PTE_USER);
    }

    lib::memset(reinterpret_cast<void*>(virt), 0, num_pages * mm::PAGE_SIZE);
    return reinterpret_cast<win32::LPVOID>(virt);
}

win32::BOOL __attribute__((ms_abi)) k32_VirtualFree(
    win32::LPVOID lpAddress,
    win32::SIZE_T dwSize,
    win32::DWORD dwFreeType
) {
    (void)lpAddress;
    (void)dwSize;
    (void)dwFreeType;
    return win32::TRUE_VAL;
}

win32::DWORD __attribute__((ms_abi)) k32_GetLastError(void) {
    return win32::g_teb.LastErrorValue;
}

void __attribute__((ms_abi)) k32_SetLastError(win32::DWORD dwErrCode) {
    win32::g_teb.LastErrorValue = dwErrCode;
}

win32::HANDLE __attribute__((ms_abi)) k32_GetModuleHandleA(const char* lpModuleName) {
    if (!lpModuleName || *lpModuleName == '\0') {
        return win32::g_peb.ImageBaseAddress;
    }
    return nullptr;
}

void* __attribute__((ms_abi)) k32_GetProcAddress(win32::HANDLE hModule, const char* lpProcName) {
    (void)hModule;
    if (!lpProcName) return nullptr;
    uint64_t addr = win32::win32_resolve_symbol("KERNEL32.DLL", lpProcName, 0);
    return reinterpret_cast<void*>(addr);
}

void __attribute__((ms_abi)) k32_Sleep(win32::DWORD dwMilliseconds) {
    if (dwMilliseconds == 0) {
        return;
    }
    // Convert ms to timer ticks (1 tick = 10 ms at 100 Hz)
    uint64_t ticks = (static_cast<uint64_t>(dwMilliseconds) + 9) / 10;
    drivers::timer_sleep_ticks(ticks);
}

uint64_t __attribute__((ms_abi)) k32_GetTickCount64(void) {
    return drivers::timer_get_ticks() * 10ULL;
}

win32::DWORD __attribute__((ms_abi)) k32_GetTickCount(void) {
    return static_cast<win32::DWORD>(drivers::timer_get_ticks() * 10ULL);
}

win32::HANDLE __attribute__((ms_abi)) k32_GetProcessHeap(void) {
    return reinterpret_cast<win32::HANDLE>(0x1000ULL);
}

win32::LPVOID __attribute__((ms_abi)) k32_HeapAlloc(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::SIZE_T dwBytes) {
    (void)hHeap;
    if (dwBytes == 0) return nullptr;
    void* ptr = mm::kmalloc(dwBytes);
    if (ptr && (dwFlags & 0x08)) { // HEAP_ZERO_MEMORY = 0x00000008
        lib::memset(ptr, 0, dwBytes);
    }
    return ptr;
}

win32::BOOL __attribute__((ms_abi)) k32_HeapFree(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::LPVOID lpMem) {
    (void)hHeap;
    (void)dwFlags;
    if (!lpMem) return win32::FALSE_VAL;
    mm::kfree(lpMem);
    return win32::TRUE_VAL;
}

} // extern "C"
