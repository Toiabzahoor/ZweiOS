#include "win32/win32.hpp"
#include "arch/x86_64/io.hpp"
#include "mm/heap.hpp"
#include "mm/vmm.hpp"
#include "drivers/serial.hpp"
#include "drivers/vga.hpp"
#include "drivers/pit.hpp"
#include "drivers/rtc.hpp"
#include "fs/vfs.hpp"
#include "loader/loader.hpp"
#include "drivers/vbe.hpp"
#include "gui/wm.hpp"
#include "gui/gfx.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"
#include "net/socket.hpp"
#include "net/dns.hpp"
#include "net/ethernet.hpp"

uint64_t g_win32_saved_kernel_rsp = 0;
uint64_t g_win32_exit_code = 0;

namespace win32 {

static TEB* g_current_teb = nullptr;
static PEB* g_current_peb = nullptr;
static char g_command_line[256] = "hello_win.exe";

static struct {
    uint32_t text_color;
    uint32_t bk_color;
} g_gdi_state = {
    .text_color = 0xFFFFFFFF,
    .bk_color = 0xFF000000,
};

void win32_init_thread_environment(uint64_t image_base, uint64_t stack_base, uint64_t stack_limit) {
    if (!g_current_peb) {
        g_current_peb = reinterpret_cast<PEB*>(mm::kmalloc(sizeof(PEB)));
        lib::memset(g_current_peb, 0, sizeof(PEB));
        g_current_peb->ProcessHeap = mm::kmalloc(64 * 1024);
    }
    g_current_peb->ImageBaseAddress = reinterpret_cast<void*>(image_base);
    g_current_peb->ProcessParameters = g_command_line;

    if (!g_current_teb) {
        g_current_teb = reinterpret_cast<TEB*>(mm::kmalloc(sizeof(TEB)));
        lib::memset(g_current_teb, 0, sizeof(TEB));
    }
    g_current_teb->StackBase = reinterpret_cast<void*>(stack_base);
    g_current_teb->StackLimit = reinterpret_cast<void*>(stack_limit);
    g_current_teb->Self = g_current_teb;
    g_current_teb->ProcessEnvironmentBlock = g_current_peb;
    g_current_teb->LastErrorValue = 0;

    arch::wrmsr(arch::IA32_GS_BASE, reinterpret_cast<uint64_t>(g_current_teb));
}

void win32_set_command_line(const char* cmdline) {
    if (cmdline) {
        lib::strncpy(g_command_line, cmdline, sizeof(g_command_line));
    }
}

static bool str_equals_case_insensitive(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'a' && c1 <= 'z') c1 = static_cast<char>(c1 - ('a' - 'A'));
        if (c2 >= 'a' && c2 <= 'z') c2 = static_cast<char>(c2 - ('a' - 'A'));
        if (c1 != c2) return false;
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

uint64_t win32_resolve_symbol(const char* dll_name, const char* func_name, uint16_t ordinal) {
    (void)ordinal;
    if (!dll_name || !func_name) {
        return 0;
    }

    if (str_equals_case_insensitive(dll_name, "KERNEL32.DLL") ||
        str_equals_case_insensitive(dll_name, "KERNEL32") ||
        str_equals_case_insensitive(dll_name, "ntdll.dll") ||
        str_equals_case_insensitive(dll_name, "ntdll")) {

        if (lib::strcmp(func_name, "GetStdHandle") == 0)          return reinterpret_cast<uint64_t>(&k32_GetStdHandle);
        if (lib::strcmp(func_name, "WriteFile") == 0)             return reinterpret_cast<uint64_t>(&k32_WriteFile);
        if (lib::strcmp(func_name, "WriteConsoleA") == 0)         return reinterpret_cast<uint64_t>(&k32_WriteConsoleA);
        if (lib::strcmp(func_name, "ExitProcess") == 0)           return reinterpret_cast<uint64_t>(&k32_ExitProcess);
        if (lib::strcmp(func_name, "GetCommandLineA") == 0)       return reinterpret_cast<uint64_t>(&k32_GetCommandLineA);
        if (lib::strcmp(func_name, "VirtualAlloc") == 0)          return reinterpret_cast<uint64_t>(&k32_VirtualAlloc);
        if (lib::strcmp(func_name, "VirtualFree") == 0)           return reinterpret_cast<uint64_t>(&k32_VirtualFree);
        if (lib::strcmp(func_name, "GetLastError") == 0)          return reinterpret_cast<uint64_t>(&k32_GetLastError);
        if (lib::strcmp(func_name, "SetLastError") == 0)          return reinterpret_cast<uint64_t>(&k32_SetLastError);
        if (lib::strcmp(func_name, "GetModuleHandleA") == 0)      return reinterpret_cast<uint64_t>(&k32_GetModuleHandleA);
        if (lib::strcmp(func_name, "GetProcAddress") == 0)        return reinterpret_cast<uint64_t>(&k32_GetProcAddress);
        if (lib::strcmp(func_name, "Sleep") == 0)                 return reinterpret_cast<uint64_t>(&k32_Sleep);
        if (lib::strcmp(func_name, "GetTickCount64") == 0)        return reinterpret_cast<uint64_t>(&k32_GetTickCount64);
        if (lib::strcmp(func_name, "GetTickCount") == 0)          return reinterpret_cast<uint64_t>(&k32_GetTickCount);
        if (lib::strcmp(func_name, "GetProcessHeap") == 0)        return reinterpret_cast<uint64_t>(&k32_GetProcessHeap);
        if (lib::strcmp(func_name, "HeapAlloc") == 0)             return reinterpret_cast<uint64_t>(&k32_HeapAlloc);
        if (lib::strcmp(func_name, "HeapFree") == 0)              return reinterpret_cast<uint64_t>(&k32_HeapFree);
        if (lib::strcmp(func_name, "GetCurrentProcessId") == 0)   return reinterpret_cast<uint64_t>(&k32_GetCurrentProcessId);
        if (lib::strcmp(func_name, "CreateFileA") == 0)           return reinterpret_cast<uint64_t>(&k32_CreateFileA);
        if (lib::strcmp(func_name, "ReadFile") == 0)              return reinterpret_cast<uint64_t>(&k32_ReadFile);
        if (lib::strcmp(func_name, "CloseHandle") == 0)           return reinterpret_cast<uint64_t>(&k32_CloseHandle);
        if (lib::strcmp(func_name, "GetFileSize") == 0)           return reinterpret_cast<uint64_t>(&k32_GetFileSize);
        if (lib::strcmp(func_name, "CreateProcessA") == 0)        return reinterpret_cast<uint64_t>(&k32_CreateProcessA);
        if (lib::strcmp(func_name, "WaitForSingleObject") == 0)    return reinterpret_cast<uint64_t>(&k32_WaitForSingleObject);
        if (lib::strcmp(func_name, "GetExitCodeProcess") == 0)     return reinterpret_cast<uint64_t>(&k32_GetExitCodeProcess);
        if (lib::strcmp(func_name, "CreatePipe") == 0)             return reinterpret_cast<uint64_t>(&k32_CreatePipe);
        if (lib::strcmp(func_name, "DuplicateHandle") == 0)        return reinterpret_cast<uint64_t>(&k32_DuplicateHandle);
    }

    if (str_equals_case_insensitive(dll_name, "USER32.DLL") ||
        str_equals_case_insensitive(dll_name, "USER32")) {

        if (lib::strcmp(func_name, "CreateWindowExA") == 0)       return reinterpret_cast<uint64_t>(&u32_CreateWindowExA);
        if (lib::strcmp(func_name, "ShowWindow") == 0)            return reinterpret_cast<uint64_t>(&u32_ShowWindow);
        if (lib::strcmp(func_name, "UpdateWindow") == 0)          return reinterpret_cast<uint64_t>(&u32_UpdateWindow);
        if (lib::strcmp(func_name, "DefWindowProcA") == 0)        return reinterpret_cast<uint64_t>(&u32_DefWindowProcA);
        if (lib::strcmp(func_name, "GetMessageA") == 0)           return reinterpret_cast<uint64_t>(&u32_GetMessageA);
        if (lib::strcmp(func_name, "TranslateMessage") == 0)      return reinterpret_cast<uint64_t>(&u32_TranslateMessage);
        if (lib::strcmp(func_name, "DispatchMessageA") == 0)      return reinterpret_cast<uint64_t>(&u32_DispatchMessageA);
        if (lib::strcmp(func_name, "PostQuitMessage") == 0)       return reinterpret_cast<uint64_t>(&u32_PostQuitMessage);
        if (lib::strcmp(func_name, "BeginPaint") == 0)            return reinterpret_cast<uint64_t>(&u32_BeginPaint);
        if (lib::strcmp(func_name, "EndPaint") == 0)              return reinterpret_cast<uint64_t>(&u32_EndPaint);
        if (lib::strcmp(func_name, "MessageBoxA") == 0)           return reinterpret_cast<uint64_t>(&u32_MessageBoxA);
        if (lib::strcmp(func_name, "GetDC") == 0)                 return reinterpret_cast<uint64_t>(&u32_GetDC);
        if (lib::strcmp(func_name, "ReleaseDC") == 0)             return reinterpret_cast<uint64_t>(&u32_ReleaseDC);
    }

    if (str_equals_case_insensitive(dll_name, "GDI32.DLL") ||
        str_equals_case_insensitive(dll_name, "GDI32")) {

        if (lib::strcmp(func_name, "TextOutA") == 0)              return reinterpret_cast<uint64_t>(&gdi_TextOutA);
        if (lib::strcmp(func_name, "SetTextColor") == 0)          return reinterpret_cast<uint64_t>(&gdi_SetTextColor);
        if (lib::strcmp(func_name, "SetBkColor") == 0)            return reinterpret_cast<uint64_t>(&gdi_SetBkColor);
        if (lib::strcmp(func_name, "Rectangle") == 0)             return reinterpret_cast<uint64_t>(&gdi_Rectangle);
        if (lib::strcmp(func_name, "FillRect") == 0)              return reinterpret_cast<uint64_t>(&gdi_FillRect);
    }

    if (str_equals_case_insensitive(dll_name, "WS2_32.DLL") ||
        str_equals_case_insensitive(dll_name, "WS2_32") ||
        str_equals_case_insensitive(dll_name, "WINSOCK.DLL") ||
        str_equals_case_insensitive(dll_name, "WSOCK32.DLL")) {

        if (func_name) {
            if (lib::strcmp(func_name, "WSAStartup") == 0)            return reinterpret_cast<uint64_t>(&wsa_WSAStartup);
            if (lib::strcmp(func_name, "WSACleanup") == 0)            return reinterpret_cast<uint64_t>(&wsa_WSACleanup);
            if (lib::strcmp(func_name, "socket") == 0)                return reinterpret_cast<uint64_t>(&wsa_socket);
            if (lib::strcmp(func_name, "connect") == 0)               return reinterpret_cast<uint64_t>(&wsa_connect);
            if (lib::strcmp(func_name, "send") == 0)                  return reinterpret_cast<uint64_t>(&wsa_send);
            if (lib::strcmp(func_name, "recv") == 0)                  return reinterpret_cast<uint64_t>(&wsa_recv);
            if (lib::strcmp(func_name, "closesocket") == 0)           return reinterpret_cast<uint64_t>(&wsa_closesocket);
            if (lib::strcmp(func_name, "gethostbyname") == 0)         return reinterpret_cast<uint64_t>(&wsa_gethostbyname);
            if (lib::strcmp(func_name, "htons") == 0)                 return reinterpret_cast<uint64_t>(&wsa_htons);
            if (lib::strcmp(func_name, "htonl") == 0)                 return reinterpret_cast<uint64_t>(&wsa_htonl);
            if (lib::strcmp(func_name, "ntohs") == 0)                 return reinterpret_cast<uint64_t>(&wsa_ntohs);
            if (lib::strcmp(func_name, "ntohl") == 0)                 return reinterpret_cast<uint64_t>(&wsa_ntohl);
            if (lib::strcmp(func_name, "WSAGetLastError") == 0)       return reinterpret_cast<uint64_t>(&wsa_WSAGetLastError);
        }

        if (ordinal != 0) {
            if (ordinal == 3)   return reinterpret_cast<uint64_t>(&wsa_closesocket);
            if (ordinal == 4)   return reinterpret_cast<uint64_t>(&wsa_connect);
            if (ordinal == 9)   return reinterpret_cast<uint64_t>(&wsa_htons);
            if (ordinal == 14)  return reinterpret_cast<uint64_t>(&wsa_ntohl);
            if (ordinal == 15)  return reinterpret_cast<uint64_t>(&wsa_ntohs);
            if (ordinal == 16)  return reinterpret_cast<uint64_t>(&wsa_recv);
            if (ordinal == 19)  return reinterpret_cast<uint64_t>(&wsa_send);
            if (ordinal == 23)  return reinterpret_cast<uint64_t>(&wsa_socket);
            if (ordinal == 52)  return reinterpret_cast<uint64_t>(&wsa_gethostbyname);
            if (ordinal == 111) return reinterpret_cast<uint64_t>(&wsa_WSAGetLastError);
            if (ordinal == 115) return reinterpret_cast<uint64_t>(&wsa_WSAStartup);
            if (ordinal == 116) return reinterpret_cast<uint64_t>(&wsa_WSACleanup);
        }
    }

    return 0;
}

}

extern "C" {

win32::HANDLE __attribute__((ms_abi)) k32_GetStdHandle(win32::DWORD nStdHandle) {
    switch (nStdHandle) {
        case win32::STD_INPUT_HANDLE_VAL:  return win32::PSEUDO_STDIN_HANDLE;
        case win32::STD_OUTPUT_HANDLE_VAL: return win32::PSEUDO_STDOUT_HANDLE;
        case win32::STD_ERROR_HANDLE_VAL:  return win32::PSEUDO_STDERR_HANDLE;
        default: return nullptr;
    }
}

win32::BOOL __attribute__((ms_abi)) k32_WriteFile(
    win32::HANDLE hFile,
    win32::LPCVOID lpBuffer,
    win32::DWORD nNumberOfBytesToWrite,
    win32::LPDWORD lpNumberOfBytesWritten,
    win32::LPVOID lpOverlapped
) {
    (void)lpOverlapped;
    if (!lpBuffer) return win32::FALSE_VAL;

    if (hFile == win32::PSEUDO_STDOUT_HANDLE || hFile == win32::PSEUDO_STDERR_HANDLE) {
        const char* str = reinterpret_cast<const char*>(lpBuffer);
        for (win32::DWORD i = 0; i < nNumberOfBytesToWrite; ++i) {
            lib::kprint_char(str[i]);
        }
        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        }
        return win32::TRUE_VAL;
    }

    int fd = static_cast<int>(reinterpret_cast<uint64_t>(hFile));
    int64_t written = fs::vfs_write(fd, lpBuffer, nNumberOfBytesToWrite);
    if (written < 0) return win32::FALSE_VAL;

    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = static_cast<win32::DWORD>(written);
    }
    return win32::TRUE_VAL;
}

win32::BOOL __attribute__((ms_abi)) k32_WriteConsoleA(
    win32::HANDLE hConsoleOutput,
    const void* lpBuffer,
    win32::DWORD nNumberOfCharsToWrite,
    win32::LPDWORD lpNumberOfCharsWritten,
    win32::LPVOID lpReserved
) {
    return k32_WriteFile(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);
}

void __attribute__((ms_abi)) k32_ExitProcess(win32::UINT uExitCode) {
    g_win32_exit_code = uExitCode;
    drivers::serial_puts("[WIN32] ExitProcess called with status: ");
    drivers::serial_put_dec(static_cast<int64_t>(uExitCode));
    drivers::serial_puts("\r\n");
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
    (void)lpAddress;
    (void)flAllocationType;
    (void)flProtect;
    return mm::kmalloc(dwSize);
}

win32::BOOL __attribute__((ms_abi)) k32_VirtualFree(
    win32::LPVOID lpAddress,
    win32::SIZE_T dwSize,
    win32::DWORD dwFreeType
) {
    (void)dwSize;
    (void)dwFreeType;
    if (lpAddress) {
        mm::kfree(lpAddress);
        return win32::TRUE_VAL;
    }
    return win32::FALSE_VAL;
}

win32::DWORD __attribute__((ms_abi)) k32_GetLastError(void) {
    if (win32::g_current_teb) {
        return win32::g_current_teb->LastErrorValue;
    }
    return 0;
}

void __attribute__((ms_abi)) k32_SetLastError(win32::DWORD dwErrCode) {
    if (win32::g_current_teb) {
        win32::g_current_teb->LastErrorValue = dwErrCode;
    }
}

win32::HANDLE __attribute__((ms_abi)) k32_GetModuleHandleA(const char* lpModuleName) {
    (void)lpModuleName;
    if (win32::g_current_peb) {
        return win32::g_current_peb->ImageBaseAddress;
    }
    return reinterpret_cast<win32::HANDLE>(0x0000000000800000ULL);
}

void* __attribute__((ms_abi)) k32_GetProcAddress(win32::HANDLE hModule, const char* lpProcName) {
    (void)hModule;
    return reinterpret_cast<void*>(win32::win32_resolve_symbol("KERNEL32.DLL", lpProcName, 0));
}

void __attribute__((ms_abi)) k32_Sleep(win32::DWORD dwMilliseconds) {
    uint32_t ticks = dwMilliseconds / 10;
    if (ticks == 0) ticks = 1;
    drivers::timer_sleep_ticks(ticks);
}

uint64_t __attribute__((ms_abi)) k32_GetTickCount64(void) {
    return drivers::timer_get_ticks() * 10;
}

win32::DWORD __attribute__((ms_abi)) k32_GetTickCount(void) {
    return static_cast<win32::DWORD>(k32_GetTickCount64());
}

win32::HANDLE __attribute__((ms_abi)) k32_GetProcessHeap(void) {
    if (win32::g_current_peb) {
        return win32::g_current_peb->ProcessHeap;
    }
    return reinterpret_cast<win32::HANDLE>(0x1000);
}

win32::LPVOID __attribute__((ms_abi)) k32_HeapAlloc(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::SIZE_T dwBytes) {
    (void)hHeap;
    (void)dwFlags;
    return mm::kmalloc(dwBytes);
}

win32::BOOL __attribute__((ms_abi)) k32_HeapFree(win32::HANDLE hHeap, win32::DWORD dwFlags, win32::LPVOID lpMem) {
    (void)hHeap;
    (void)dwFlags;
    if (lpMem) {
        mm::kfree(lpMem);
        return win32::TRUE_VAL;
    }
    return win32::FALSE_VAL;
}

win32::DWORD __attribute__((ms_abi)) k32_GetCurrentProcessId(void) {
    return 1;
}

win32::HANDLE __attribute__((ms_abi)) k32_CreateFileA(
    const char* lpFileName,
    win32::DWORD dwDesiredAccess,
    win32::DWORD dwShareMode,
    win32::LPVOID lpSecurityAttributes,
    win32::DWORD dwCreationDisposition,
    win32::DWORD dwFlagsAndAttributes,
    win32::HANDLE hTemplateFile
) {
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;

    int flags = 0;
    if ((dwDesiredAccess & 0xC0000000) == 0xC0000000) {
        flags |= fs::O_RDWR;
    } else if (dwDesiredAccess & 0x40000000) {
        flags |= fs::O_WRONLY;
    } else {
        flags |= fs::O_RDONLY;
    }

    if (dwCreationDisposition == 2) {
        flags |= (fs::O_CREAT | fs::O_TRUNC);
    } else if (dwCreationDisposition == 4) {
        flags |= fs::O_CREAT;
    }

    int fd = fs::vfs_open(lpFileName, flags);
    if (fd < 0) {
        return reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(-1));
    }
    return reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(fd));
}

win32::BOOL __attribute__((ms_abi)) k32_ReadFile(
    win32::HANDLE hFile,
    win32::LPVOID lpBuffer,
    win32::DWORD nNumberOfBytesToRead,
    win32::LPDWORD lpNumberOfBytesRead,
    win32::LPVOID lpOverlapped
) {
    (void)lpOverlapped;
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(hFile));
    int64_t bytes = fs::vfs_read(fd, lpBuffer, nNumberOfBytesToRead);
    if (bytes < 0) return win32::FALSE_VAL;

    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = static_cast<win32::DWORD>(bytes);
    }
    return win32::TRUE_VAL;
}

win32::BOOL __attribute__((ms_abi)) k32_CloseHandle(win32::HANDLE hObject) {
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(hObject));
    if (fd >= 0) {
        fs::vfs_close(fd);
        return win32::TRUE_VAL;
    }
    return win32::FALSE_VAL;
}

win32::DWORD __attribute__((ms_abi)) k32_GetFileSize(win32::HANDLE hFile, win32::LPDWORD lpFileSizeHigh) {
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(hFile));
    int64_t cur = fs::vfs_lseek(fd, 0, fs::SEEK_CUR);
    int64_t end = fs::vfs_lseek(fd, 0, fs::SEEK_END);
    fs::vfs_lseek(fd, cur, fs::SEEK_SET);

    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return static_cast<win32::DWORD>(end);
}

win32::BOOL __attribute__((ms_abi)) k32_CreateProcessA(
    const char* lpApplicationName,
    char* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    win32::BOOL bInheritHandles,
    win32::DWORD dwCreationFlags,
    void* lpEnvironment,
    const char* lpCurrentDirectory,
    win32::STARTUPINFOA* lpStartupInfo,
    win32::PROCESS_INFORMATION* lpProcessInformation
) {
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    (void)lpCurrentDirectory;
    (void)lpStartupInfo;

    const char* target = lpApplicationName;
    if (!target && lpCommandLine) {
        target = lpCommandLine;
    }
    if (!target) return win32::FALSE_VAL;

    const char* argv[] = { target, nullptr };
    int64_t code = loader::loader_execute_path(target, 1, argv);

    if (lpProcessInformation) {
        lpProcessInformation->hProcess = reinterpret_cast<win32::HANDLE>(static_cast<uint64_t>(1));
        lpProcessInformation->hThread = reinterpret_cast<win32::HANDLE>(static_cast<uint64_t>(1));
        lpProcessInformation->dwProcessId = 1;
        lpProcessInformation->dwThreadId = 1;
    }
    return (code >= 0) ? win32::TRUE_VAL : win32::FALSE_VAL;
}

win32::DWORD __attribute__((ms_abi)) k32_WaitForSingleObject(win32::HANDLE hHandle, win32::DWORD dwMilliseconds) {
    (void)hHandle;
    (void)dwMilliseconds;
    return 0;
}

win32::BOOL __attribute__((ms_abi)) k32_GetExitCodeProcess(win32::HANDLE hProcess, win32::LPDWORD lpExitCode) {
    (void)hProcess;
    if (lpExitCode) {
        *lpExitCode = static_cast<win32::DWORD>(g_win32_exit_code);
    }
    return win32::TRUE_VAL;
}

win32::BOOL __attribute__((ms_abi)) k32_CreatePipe(win32::HANDLE* hReadPipe, win32::HANDLE* hWritePipe, void* lpPipeAttributes, win32::DWORD nSize) {
    (void)lpPipeAttributes;
    (void)nSize;
    if (!hReadPipe || !hWritePipe) return win32::FALSE_VAL;
    int pipefd[2];
    if (fs::vfs_pipe(pipefd, 0) != 0) return win32::FALSE_VAL;
    *hReadPipe = reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(pipefd[0]));
    *hWritePipe = reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(pipefd[1]));
    return win32::TRUE_VAL;
}

win32::BOOL __attribute__((ms_abi)) k32_DuplicateHandle(win32::HANDLE hSourceProcessHandle, win32::HANDLE hSourceHandle, win32::HANDLE hTargetProcessHandle, win32::HANDLE* lpTargetHandle, win32::DWORD dwDesiredAccess, win32::BOOL bInheritHandle, win32::DWORD dwOptions) {
    (void)hSourceProcessHandle;
    (void)hTargetProcessHandle;
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    (void)dwOptions;
    if (!lpTargetHandle) return win32::FALSE_VAL;
    int oldfd = static_cast<int>(reinterpret_cast<intptr_t>(hSourceHandle));
    int newfd = fs::vfs_dup(oldfd);
    if (newfd < 0) return win32::FALSE_VAL;
    *lpTargetHandle = reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(newfd));
    return win32::TRUE_VAL;
}

win32::HWND __attribute__((ms_abi)) u32_CreateWindowExA(
    win32::DWORD dwExStyle,
    const char* lpClassName,
    const char* lpWindowName,
    win32::DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    win32::HWND hWndParent,
    win32::HMENU hMenu,
    win32::HINSTANCE hInstance,
    void* lpParam
) {
    (void)dwExStyle;
    (void)lpClassName;
    (void)dwStyle;
    (void)hWndParent;
    (void)hMenu;
    (void)hInstance;
    (void)lpParam;

    int wx = (X >= 0) ? X : 120;
    int wy = (Y >= 0) ? Y : 100;
    int ww = (nWidth > 0) ? nWidth : 440;
    int wh = (nHeight > 0) ? nHeight : 300;

    auto* win = gui::wm_create_window(lpWindowName ? lpWindowName : "Win32 Window", wx, wy, ww, wh);
    return reinterpret_cast<win32::HWND>(win);
}

win32::BOOL __attribute__((ms_abi)) u32_ShowWindow(win32::HWND hWnd, int nCmdShow) {
    (void)nCmdShow;
    if (hWnd) {
        auto* win = reinterpret_cast<gui::Window*>(hWnd);
        win->is_minimized = false;
        gui::wm_focus_window(win->id);
        return win32::TRUE_VAL;
    }
    return win32::FALSE_VAL;
}

win32::BOOL __attribute__((ms_abi)) u32_UpdateWindow(win32::HWND hWnd) {
    if (hWnd) {
        gui::wm_render();
        return win32::TRUE_VAL;
    }
    return win32::FALSE_VAL;
}

win32::LRESULT __attribute__((ms_abi)) u32_DefWindowProcA(win32::HWND hWnd, win32::UINT Msg, win32::WPARAM wParam, win32::LPARAM lParam) {
    (void)hWnd;
    (void)Msg;
    (void)wParam;
    (void)lParam;
    return 0;
}

win32::BOOL __attribute__((ms_abi)) u32_GetMessageA(win32::MSG* lpMsg, win32::HWND hWnd, win32::UINT wMsgFilterMin, win32::UINT wMsgFilterMax) {
    (void)hWnd;
    (void)wMsgFilterMin;
    (void)wMsgFilterMax;
    if (!lpMsg) return win32::FALSE_VAL;

    gui::wm_update();
    lpMsg->hwnd = hWnd;
    lpMsg->message = 0x000F;
    lpMsg->wParam = 0;
    lpMsg->lParam = 0;
    lpMsg->time = drivers::timer_get_ticks() * 10;
    return win32::FALSE_VAL;
}

win32::BOOL __attribute__((ms_abi)) u32_TranslateMessage(const win32::MSG* lpMsg) {
    (void)lpMsg;
    return win32::TRUE_VAL;
}

win32::LRESULT __attribute__((ms_abi)) u32_DispatchMessageA(const win32::MSG* lpMsg) {
    (void)lpMsg;
    return 0;
}

void __attribute__((ms_abi)) u32_PostQuitMessage(int nExitCode) {
    k32_ExitProcess(static_cast<win32::UINT>(nExitCode));
}

win32::HDC __attribute__((ms_abi)) u32_BeginPaint(win32::HWND hWnd, win32::PAINTSTRUCT* lpPaint) {
    if (!hWnd) return nullptr;
    auto* win = reinterpret_cast<gui::Window*>(hWnd);
    if (lpPaint) {
        lpPaint->hdc = reinterpret_cast<win32::HDC>(win);
        lpPaint->rcPaint.left = 0;
        lpPaint->rcPaint.top = 0;
        lpPaint->rcPaint.right = win->width;
        lpPaint->rcPaint.bottom = win->height;
    }
    return reinterpret_cast<win32::HDC>(win);
}

win32::BOOL __attribute__((ms_abi)) u32_EndPaint(win32::HWND hWnd, const win32::PAINTSTRUCT* lpPaint) {
    (void)hWnd;
    (void)lpPaint;
    gui::wm_render();
    return win32::TRUE_VAL;
}

int __attribute__((ms_abi)) u32_MessageBoxA(win32::HWND hWnd, const char* lpText, const char* lpCaption, win32::UINT uType) {
    (void)hWnd;
    (void)uType;

    lib::kprint_str("\r\n================================================================\r\n");
    lib::kprintf("  [WIN32 MESSAGEBOX] %s\r\n", lpCaption ? lpCaption : "Message");
    lib::kprint_str("================================================================\r\n");
    lib::kprintf("  %s\r\n", lpText ? lpText : "");
    lib::kprint_str("================================================================\r\n\r\n");

    if (drivers::vbe_is_available()) {
        auto* popup = gui::wm_create_window(lpCaption ? lpCaption : "MessageBox", 320, 240, 380, 160, gui::WINDOW_FLAG_POPUP | gui::WINDOW_FLAG_HAS_TITLE);
        if (popup && popup->buffer) {
            gui::gfx_draw_rect(0, 0, popup->width, popup->height, gui::COLOR_WINDOW_BG, popup->buffer, popup->width, popup->height);
            gui::gfx_draw_text(20, 24, lpText ? lpText : "", gui::COLOR_WHITE, 0, true, popup->buffer, popup->width, popup->height);
            gui::gfx_draw_rect(popup->width / 2 - 40, popup->height - 40, 80, 24, gui::COLOR_TITLE_ACTIVE, popup->buffer, popup->width, popup->height);
            gui::gfx_draw_text(popup->width / 2 - 8, popup->height - 36, "OK", gui::COLOR_WHITE, 0, true, popup->buffer, popup->width, popup->height);
        }
        gui::wm_render();
    }

    return 1;
}

win32::HDC __attribute__((ms_abi)) u32_GetDC(win32::HWND hWnd) {
    return reinterpret_cast<win32::HDC>(hWnd);
}

int __attribute__((ms_abi)) u32_ReleaseDC(win32::HWND hWnd, win32::HDC hDC) {
    (void)hWnd;
    (void)hDC;
    return 1;
}

win32::BOOL __attribute__((ms_abi)) gdi_TextOutA(win32::HDC hdc, int x, int y, const char* lpString, int c) {
    if (!hdc || !lpString) return win32::FALSE_VAL;
    auto* win = reinterpret_cast<gui::Window*>(hdc);
    if (!win->buffer) return win32::FALSE_VAL;

    char tmp[128];
    size_t len = (c > 0 && c < 127) ? static_cast<size_t>(c) : lib::strlen(lpString);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    lib::memcpy(tmp, lpString, len);
    tmp[len] = '\0';

    gui::gfx_draw_text(x, y, tmp, win32::g_gdi_state.text_color, win32::g_gdi_state.bk_color, true, win->buffer, win->width, win->height);
    return win32::TRUE_VAL;
}

win32::COLORREF __attribute__((ms_abi)) gdi_SetTextColor(win32::HDC hdc, win32::COLORREF color) {
    (void)hdc;
    win32::COLORREF old = win32::g_gdi_state.text_color;
    win32::g_gdi_state.text_color = 0xFF000000 | (color & 0x00FFFFFF);
    return old;
}

win32::COLORREF __attribute__((ms_abi)) gdi_SetBkColor(win32::HDC hdc, win32::COLORREF color) {
    (void)hdc;
    win32::COLORREF old = win32::g_gdi_state.bk_color;
    win32::g_gdi_state.bk_color = 0xFF000000 | (color & 0x00FFFFFF);
    return old;
}

win32::BOOL __attribute__((ms_abi)) gdi_Rectangle(win32::HDC hdc, int left, int top, int right, int bottom) {
    if (!hdc) return win32::FALSE_VAL;
    auto* win = reinterpret_cast<gui::Window*>(hdc);
    if (!win->buffer) return win32::FALSE_VAL;

    int w = right - left;
    int h = bottom - top;
    gui::gfx_draw_rect_outline(left, top, w, h, win32::g_gdi_state.text_color, 1, win->buffer, win->width, win->height);
    return win32::TRUE_VAL;
}

int __attribute__((ms_abi)) gdi_FillRect(win32::HDC hdc, const win32::RECT* lprc, win32::HBRUSH hbr) {
    (void)hbr;
    if (!hdc || !lprc) return 0;
    auto* win = reinterpret_cast<gui::Window*>(hdc);
    if (!win->buffer) return 0;

    int w = lprc->right - lprc->left;
    int h = lprc->bottom - lprc->top;
    gui::gfx_draw_rect(lprc->left, lprc->top, w, h, win32::g_gdi_state.bk_color, win->buffer, win->width, win->height);
    return 1;
}

int __attribute__((ms_abi)) wsa_WSAStartup(uint16_t wVersionRequested, win32::WSADATA* lpWSAData) {
    if (lpWSAData) {
        lpWSAData->wVersion = wVersionRequested;
        lpWSAData->wHighVersion = 0x0202;
        lib::strncpy(lpWSAData->szDescription, "ZweiOS WinSock 2.2", sizeof(lpWSAData->szDescription));
        lib::strncpy(lpWSAData->szSystemStatus, "Running", sizeof(lpWSAData->szSystemStatus));
        lpWSAData->iMaxSockets = 32;
        lpWSAData->iMaxUdpDg = 1460;
        lpWSAData->lpVendorInfo = nullptr;
    }
    return 0;
}

int __attribute__((ms_abi)) wsa_WSACleanup(void) {
    return 0;
}

win32::SOCKET __attribute__((ms_abi)) wsa_socket(int af, int type, int protocol) {
    int fd = net::sock_create(af, type, protocol);
    if (fd < 0) {
        k32_SetLastError(10022);
        return win32::INVALID_SOCKET_VAL;
    }
    return static_cast<win32::SOCKET>(fd);
}

int __attribute__((ms_abi)) wsa_connect(win32::SOCKET s, const void* name, int namelen) {
    if (!name || namelen < static_cast<int>(sizeof(net::sockaddr_in))) {
        k32_SetLastError(10014);
        return win32::SOCKET_ERROR_VAL;
    }
    int res = net::sock_connect(static_cast<int>(s), reinterpret_cast<const net::sockaddr_in*>(name));
    if (res < 0) {
        k32_SetLastError(10060);
        return win32::SOCKET_ERROR_VAL;
    }
    return 0;
}

int __attribute__((ms_abi)) wsa_send(win32::SOCKET s, const char* buf, int len, int flags) {
    if (!buf || len < 0) {
        k32_SetLastError(10014);
        return win32::SOCKET_ERROR_VAL;
    }
    int64_t res = net::sock_send(static_cast<int>(s), buf, static_cast<size_t>(len), flags);
    if (res < 0) {
        k32_SetLastError(10054);
        return win32::SOCKET_ERROR_VAL;
    }
    return static_cast<int>(res);
}

int __attribute__((ms_abi)) wsa_recv(win32::SOCKET s, char* buf, int len, int flags) {
    if (!buf || len < 0) {
        k32_SetLastError(10014);
        return win32::SOCKET_ERROR_VAL;
    }
    int64_t res = net::sock_recv(static_cast<int>(s), buf, static_cast<size_t>(len), flags);
    if (res < 0) {
        k32_SetLastError(10054);
        return win32::SOCKET_ERROR_VAL;
    }
    return static_cast<int>(res);
}

int __attribute__((ms_abi)) wsa_closesocket(win32::SOCKET s) {
    int res = net::sock_close(static_cast<int>(s));
    if (res < 0) {
        k32_SetLastError(10038);
        return win32::SOCKET_ERROR_VAL;
    }
    return 0;
}

static win32::hostent s_wsa_hostent;
static char s_wsa_host_name[128];
static uint32_t s_wsa_host_addr;
static char* s_wsa_addr_list[2];

win32::hostent* __attribute__((ms_abi)) wsa_gethostbyname(const char* name) {
    if (!name) {
        k32_SetLastError(10014);
        return nullptr;
    }
    uint32_t resolved_ip = 0;
    if (!net::dns_resolve(name, &resolved_ip, 3000)) {
        k32_SetLastError(11001);
        return nullptr;
    }
    lib::strncpy(s_wsa_host_name, name, sizeof(s_wsa_host_name));
    s_wsa_host_name[sizeof(s_wsa_host_name) - 1] = '\0';
    s_wsa_host_addr = net::htonl(resolved_ip);
    s_wsa_addr_list[0] = reinterpret_cast<char*>(&s_wsa_host_addr);
    s_wsa_addr_list[1] = nullptr;

    s_wsa_hostent.h_name = s_wsa_host_name;
    s_wsa_hostent.h_aliases = nullptr;
    s_wsa_hostent.h_addrtype = 2;
    s_wsa_hostent.h_length = 4;
    s_wsa_hostent.h_addr_list = s_wsa_addr_list;
    return &s_wsa_hostent;
}

uint16_t __attribute__((ms_abi)) wsa_htons(uint16_t hostshort) {
    return net::htons(hostshort);
}

uint32_t __attribute__((ms_abi)) wsa_htonl(uint32_t hostlong) {
    return net::htonl(hostlong);
}

uint16_t __attribute__((ms_abi)) wsa_ntohs(uint16_t netshort) {
    return net::ntohs(netshort);
}

uint32_t __attribute__((ms_abi)) wsa_ntohl(uint32_t netlong) {
    return net::ntohl(netlong);
}

int __attribute__((ms_abi)) wsa_WSAGetLastError(void) {
    return static_cast<int>(k32_GetLastError());
}

}
