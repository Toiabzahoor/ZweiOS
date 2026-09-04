/* ==============================================================================
 * ZweiOS - Bare-Metal x86_64 Operating System
 * Component: Native In-Kernel Win32 Subsystem & KERNEL32 API Thunks
 * ============================================================================== */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace win32 {

// Win32 Base Types
using HANDLE  = void*;
using DWORD   = uint32_t;
using BOOL    = int32_t;
using LPCVOID = const void*;
using LPVOID  = void*;
using SIZE_T  = uint64_t;
using LPDWORD = uint32_t*;
using UINT    = uint32_t;

inline constexpr BOOL TRUE_VAL  = 1;
inline constexpr BOOL FALSE_VAL = 0;

// Standard Handle Identifiers (passed to GetStdHandle)
inline constexpr DWORD STD_INPUT_HANDLE_VAL  = static_cast<DWORD>(-10); // 0xFFFFFFF6
inline constexpr DWORD STD_OUTPUT_HANDLE_VAL = static_cast<DWORD>(-11); // 0xFFFFFFF5
inline constexpr DWORD STD_ERROR_HANDLE_VAL  = static_cast<DWORD>(-12); // 0xFFFFFFF4

// Pseudo-handles returned by GetStdHandle
inline const HANDLE PSEUDO_STDIN_HANDLE  = reinterpret_cast<HANDLE>(0x10ULL);
inline const HANDLE PSEUDO_STDOUT_HANDLE = reinterpret_cast<HANDLE>(0x11ULL);
inline const HANDLE PSEUDO_STDERR_HANDLE = reinterpret_cast<HANDLE>(0x12ULL);

// ------------------------------------------------------------------------------
// Process Environment Block (PEB) & Thread Environment Block (TEB) Layout
// ------------------------------------------------------------------------------

struct PEB {
    uint8_t  InheritedAddressSpace;      // 0x00
    uint8_t  ReadImageFileExecOptions;   // 0x01
    uint8_t  BeingDebugged;              // 0x02
    uint8_t  SpareBool;                  // 0x03
    uint32_t Reserved1;                  // 0x04
    void*    Mutant;                     // 0x08
    void*    ImageBaseAddress;           // 0x10
    void*    Ldr;                        // 0x18
    void*    ProcessParameters;          // 0x20
    void*    SubSystemData;              // 0x28
    void*    ProcessHeap;                // 0x30
    uint8_t  Reserved2[456];             // Padding up to standard PEB size
};

struct TEB {
    void*    ExceptionList;              // 0x00
    void*    StackBase;                  // 0x08
    void*    StackLimit;                 // 0x10
    void*    SubSystemTib;               // 0x18
    void*    FiberData;                  // 0x20
    void*    ArbitraryUserPointer;       // 0x28
    TEB*     Self;                       // 0x30 (pvSelf: points to itself)
    void*    EnvironmentPointer;         // 0x38
    uint64_t UniqueProcess;              // 0x40
    uint64_t UniqueThread;               // 0x48
    void*    ActiveRpcHandle;            // 0x50
    void*    ThreadLocalStoragePointer;  // 0x58
    PEB*     ProcessEnvironmentBlock;    // 0x60 (PEB pointer)
    uint32_t LastErrorValue;             // 0x68
    uint8_t  Reserved[4096 - 0x6C];      // Pad out to 4096 bytes (1 page)
};

static_assert(offsetof(TEB, Self) == 0x30, "TEB.Self must be at offset 0x30 (gs:[0x30])");
static_assert(offsetof(TEB, ProcessEnvironmentBlock) == 0x60, "TEB.PEB must be at offset 0x60 (gs:[0x60])");

// ------------------------------------------------------------------------------
// Win32 Subsystem Control & Registration APIs
// ------------------------------------------------------------------------------

void win32_init_thread_environment(uint64_t image_base, uint64_t stack_base, uint64_t stack_limit);
void win32_set_command_line(const char* cmdline);
uint64_t win32_resolve_symbol(const char* dll_name, const char* func_name, uint16_t ordinal);

} // namespace win32

// ------------------------------------------------------------------------------
// In-Kernel KERNEL32 Exported Thunks (Microsoft x64 Calling Convention)
// ------------------------------------------------------------------------------

extern "C" {

win32::HANDLE  __attribute__((ms_abi)) k32_GetStdHandle(win32::DWORD nStdHandle);
win32::BOOL    __attribute__((ms_abi)) k32_WriteFile(win32::HANDLE hFile, win32::LPCVOID lpBuffer, win32::DWORD nNumberOfBytesToWrite, win32::LPDWORD lpNumberOfBytesWritten, win32::LPVOID lpOverlapped);
win32::BOOL    __attribute__((ms_abi)) k32_WriteConsoleA(win32::HANDLE hConsoleOutput, const void* lpBuffer, win32::DWORD nNumberOfCharsToWrite, win32::LPDWORD lpNumberOfCharsWritten, win32::LPVOID lpReserved);
void           __attribute__((ms_abi)) k32_ExitProcess(win32::UINT uExitCode);
const char*    __attribute__((ms_abi)) k32_GetCommandLineA(void);
win32::LPVOID  __attribute__((ms_abi)) k32_VirtualAlloc(win32::LPVOID lpAddress, win32::SIZE_T dwSize, win32::DWORD flAllocationType, win32::DWORD flProtect);
win32::BOOL    __attribute__((ms_abi)) k32_VirtualFree(win32::LPVOID lpAddress, win32::SIZE_T dwSize, win32::DWORD dwFreeType);
win32::DWORD   __attribute__((ms_abi)) k32_GetLastError(void);
void           __attribute__((ms_abi)) k32_SetLastError(win32::DWORD dwErrCode);
win32::HANDLE  __attribute__((ms_abi)) k32_GetModuleHandleA(const char* lpModuleName);
void*          __attribute__((ms_abi)) k32_GetProcAddress(win32::HANDLE hModule, const char* lpProcName);
void           __attribute__((ms_abi)) k32_Sleep(win32::DWORD dwMilliseconds);
uint64_t       __attribute__((ms_abi)) k32_GetTickCount64(void);
win32::DWORD   __attribute__((ms_abi)) k32_GetTickCount(void);
win32::HANDLE  __attribute__((ms_abi)) k32_GetProcessHeap(void);
win32::LPVOID  __attribute__((ms_abi)) k32_HeapAlloc(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::SIZE_T dwBytes);
win32::BOOL    __attribute__((ms_abi)) k32_HeapFree(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::LPVOID lpMem);

// Global context saving for ExitProcess context unwinding
extern uint64_t g_win32_saved_kernel_rsp;
extern uint64_t g_win32_exit_code;
extern void win32_exit_trampoline(void);

} // extern "C"
