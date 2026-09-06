#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace win32 {

using HANDLE    = void*;
using HWND      = void*;
using HDC       = void*;
using HBRUSH    = void*;
using HMENU     = void*;
using HINSTANCE = void*;
using DWORD     = uint32_t;
using BOOL      = int32_t;
using LPCVOID   = const void*;
using LPVOID    = void*;
using SIZE_T    = uint64_t;
using LPDWORD   = uint32_t*;
using UINT      = uint32_t;
using COLORREF  = uint32_t;
using WPARAM    = uint64_t;
using LPARAM    = int64_t;
using LRESULT   = int64_t;

inline constexpr BOOL TRUE_VAL  = 1;
inline constexpr BOOL FALSE_VAL = 0;

inline constexpr DWORD STD_INPUT_HANDLE_VAL  = static_cast<DWORD>(-10);
inline constexpr DWORD STD_OUTPUT_HANDLE_VAL = static_cast<DWORD>(-11);
inline constexpr DWORD STD_ERROR_HANDLE_VAL  = static_cast<DWORD>(-12);

inline const HANDLE PSEUDO_STDIN_HANDLE  = reinterpret_cast<HANDLE>(0x10ULL);
inline const HANDLE PSEUDO_STDOUT_HANDLE = reinterpret_cast<HANDLE>(0x11ULL);
inline const HANDLE PSEUDO_STDERR_HANDLE = reinterpret_cast<HANDLE>(0x12ULL);

struct RECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

struct POINT {
    int32_t x;
    int32_t y;
};

struct MSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
};

struct PAINTSTRUCT {
    HDC     hdc;
    BOOL    fErase;
    RECT    rcPaint;
    BOOL    fRestore;
    BOOL    fIncUpdate;
    uint8_t rgbReserved[32];
};

struct STARTUPINFOA {
    DWORD    cb;
    char*    lpReserved;
    char*    lpDesktop;
    char*    lpTitle;
    DWORD    dwX;
    DWORD    dwY;
    DWORD    dwXSize;
    DWORD    dwYSize;
    DWORD    dwXCountChars;
    DWORD    dwYCountChars;
    DWORD    dwFillAttribute;
    DWORD    dwFlags;
    uint16_t wShowWindow;
    uint16_t cbReserved2;
    uint8_t* lpReserved2;
    HANDLE   hStdInput;
    HANDLE   hStdOutput;
    HANDLE   hStdError;
};

struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
};

struct PEB {
    uint8_t  InheritedAddressSpace;
    uint8_t  ReadImageFileExecOptions;
    uint8_t  BeingDebugged;
    uint8_t  SpareBool;
    uint32_t Reserved1;
    void*    Mutant;
    void*    ImageBaseAddress;
    void*    Ldr;
    void*    ProcessParameters;
    void*    SubSystemData;
    void*    ProcessHeap;
    uint8_t  Reserved2[456];
};

struct TEB {
    void*    ExceptionList;
    void*    StackBase;
    void*    StackLimit;
    void*    SubSystemTib;
    void*    FiberData;
    void*    ArbitraryUserPointer;
    TEB*     Self;
    void*    EnvironmentPointer;
    uint64_t UniqueProcess;
    uint64_t UniqueThread;
    void*    ActiveRpcHandle;
    void*    ThreadLocalStoragePointer;
    PEB*     ProcessEnvironmentBlock;
    uint32_t LastErrorValue;
    uint8_t  Reserved[4096 - 0x6C];
};

static_assert(offsetof(TEB, Self) == 0x30, "TEB.Self must be at offset 0x30 (gs:[0x30])");
static_assert(offsetof(TEB, ProcessEnvironmentBlock) == 0x60, "TEB.PEB must be at offset 0x60 (gs:[0x60])");

void win32_init_thread_environment(uint64_t image_base, uint64_t stack_base, uint64_t stack_limit);
void win32_set_command_line(const char* cmdline);
uint64_t win32_resolve_symbol(const char* dll_name, const char* func_name, uint16_t ordinal);

using SOCKET = uint64_t;
inline constexpr SOCKET INVALID_SOCKET_VAL = static_cast<SOCKET>(~0ULL);
inline constexpr int SOCKET_ERROR_VAL = -1;

struct WSADATA {
    uint16_t wVersion;
    uint16_t wHighVersion;
    char     szDescription[257];
    char     szSystemStatus[129];
    uint16_t iMaxSockets;
    uint16_t iMaxUdpDg;
    char*    lpVendorInfo;
};

struct hostent {
    char*    h_name;
    char**   h_aliases;
    int16_t  h_addrtype;
    int16_t  h_length;
    char**   h_addr_list;
};

}

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
win32::DWORD   __attribute__((ms_abi)) k32_GetCurrentProcessId(void);
win32::HANDLE  __attribute__((ms_abi)) k32_CreateFileA(const char* lpFileName, win32::DWORD dwDesiredAccess, win32::DWORD dwShareMode, win32::LPVOID lpSecurityAttributes, win32::DWORD dwCreationDisposition, win32::DWORD dwFlagsAndAttributes, win32::HANDLE hTemplateFile);
win32::BOOL    __attribute__((ms_abi)) k32_ReadFile(win32::HANDLE hFile, win32::LPVOID lpBuffer, win32::DWORD nNumberOfBytesToRead, win32::LPDWORD lpNumberOfBytesRead, win32::LPVOID lpOverlapped);
win32::BOOL    __attribute__((ms_abi)) k32_CloseHandle(win32::HANDLE hObject);
win32::DWORD   __attribute__((ms_abi)) k32_GetFileSize(win32::HANDLE hFile, win32::LPDWORD lpFileSizeHigh);

win32::BOOL    __attribute__((ms_abi)) k32_CreateProcessA(const char* lpApplicationName, char* lpCommandLine, void* lpProcessAttributes, void* lpThreadAttributes, win32::BOOL bInheritHandles, win32::DWORD dwCreationFlags, void* lpEnvironment, const char* lpCurrentDirectory, win32::STARTUPINFOA* lpStartupInfo, win32::PROCESS_INFORMATION* lpProcessInformation);
win32::DWORD   __attribute__((ms_abi)) k32_WaitForSingleObject(win32::HANDLE hHandle, win32::DWORD dwMilliseconds);
win32::BOOL    __attribute__((ms_abi)) k32_GetExitCodeProcess(win32::HANDLE hProcess, win32::LPDWORD lpExitCode);
win32::BOOL    __attribute__((ms_abi)) k32_CreatePipe(win32::HANDLE* hReadPipe, win32::HANDLE* hWritePipe, void* lpPipeAttributes, win32::DWORD nSize);
win32::BOOL    __attribute__((ms_abi)) k32_DuplicateHandle(win32::HANDLE hSourceProcessHandle, win32::HANDLE hSourceHandle, win32::HANDLE hTargetProcessHandle, win32::HANDLE* lpTargetHandle, win32::DWORD dwDesiredAccess, win32::BOOL bInheritHandle, win32::DWORD dwOptions);

win32::HWND    __attribute__((ms_abi)) u32_CreateWindowExA(win32::DWORD dwExStyle, const char* lpClassName, const char* lpWindowName, win32::DWORD dwStyle, int X, int Y, int nWidth, int nHeight, win32::HWND hWndParent, win32::HMENU hMenu, win32::HINSTANCE hInstance, void* lpParam);
win32::BOOL    __attribute__((ms_abi)) u32_ShowWindow(win32::HWND hWnd, int nCmdShow);
win32::BOOL    __attribute__((ms_abi)) u32_UpdateWindow(win32::HWND hWnd);
win32::LRESULT __attribute__((ms_abi)) u32_DefWindowProcA(win32::HWND hWnd, win32::UINT Msg, win32::WPARAM wParam, win32::LPARAM lParam);
win32::BOOL    __attribute__((ms_abi)) u32_GetMessageA(win32::MSG* lpMsg, win32::HWND hWnd, win32::UINT wMsgFilterMin, win32::UINT wMsgFilterMax);
win32::BOOL    __attribute__((ms_abi)) u32_TranslateMessage(const win32::MSG* lpMsg);
win32::LRESULT __attribute__((ms_abi)) u32_DispatchMessageA(const win32::MSG* lpMsg);
void           __attribute__((ms_abi)) u32_PostQuitMessage(int nExitCode);
win32::HDC     __attribute__((ms_abi)) u32_BeginPaint(win32::HWND hWnd, win32::PAINTSTRUCT* lpPaint);
win32::BOOL    __attribute__((ms_abi)) u32_EndPaint(win32::HWND hWnd, const win32::PAINTSTRUCT* lpPaint);
int            __attribute__((ms_abi)) u32_MessageBoxA(win32::HWND hWnd, const char* lpText, const char* lpCaption, win32::UINT uType);
win32::HDC     __attribute__((ms_abi)) u32_GetDC(win32::HWND hWnd);
int            __attribute__((ms_abi)) u32_ReleaseDC(win32::HWND hWnd, win32::HDC hDC);

win32::BOOL    __attribute__((ms_abi)) gdi_TextOutA(win32::HDC hdc, int x, int y, const char* lpString, int c);
win32::COLORREF __attribute__((ms_abi)) gdi_SetTextColor(win32::HDC hdc, win32::COLORREF color);
win32::COLORREF __attribute__((ms_abi)) gdi_SetBkColor(win32::HDC hdc, win32::COLORREF color);
win32::BOOL    __attribute__((ms_abi)) gdi_Rectangle(win32::HDC hdc, int left, int top, int right, int bottom);
int            __attribute__((ms_abi)) gdi_FillRect(win32::HDC hdc, const win32::RECT* lprc, win32::HBRUSH hbr);

int          __attribute__((ms_abi)) wsa_WSAStartup(uint16_t wVersionRequested, win32::WSADATA* lpWSAData);
int          __attribute__((ms_abi)) wsa_WSACleanup(void);
win32::SOCKET __attribute__((ms_abi)) wsa_socket(int af, int type, int protocol);
int          __attribute__((ms_abi)) wsa_connect(win32::SOCKET s, const void* name, int namelen);
int          __attribute__((ms_abi)) wsa_send(win32::SOCKET s, const char* buf, int len, int flags);
int          __attribute__((ms_abi)) wsa_recv(win32::SOCKET s, char* buf, int len, int flags);
int          __attribute__((ms_abi)) wsa_closesocket(win32::SOCKET s);
win32::hostent* __attribute__((ms_abi)) wsa_gethostbyname(const char* name);
uint16_t     __attribute__((ms_abi)) wsa_htons(uint16_t hostshort);
uint32_t     __attribute__((ms_abi)) wsa_htonl(uint32_t hostlong);
uint16_t     __attribute__((ms_abi)) wsa_ntohs(uint16_t netshort);
uint32_t     __attribute__((ms_abi)) wsa_ntohl(uint32_t netlong);
int          __attribute__((ms_abi)) wsa_WSAGetLastError(void);

extern uint64_t g_win32_saved_kernel_rsp;
extern uint64_t g_win32_exit_code;
extern void win32_exit_trampoline(void);

}
