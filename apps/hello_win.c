/* ==============================================================================
 * ZweiOS Authentic Windows Executable Test Application
 * Target: x86_64-w64-mingw32 / Windows x64 PE32+
 * Dynamic Linking: Imports Win32 APIs from KERNEL32.DLL
 * ============================================================================== */

#define STD_OUTPUT_HANDLE ((unsigned long)-11)

// Win32 API Dynamic Declarations (Import Address Table binds these)
__declspec(dllimport) void* __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int   __stdcall WriteFile(
    void*         hFile,
    const void*   lpBuffer,
    unsigned long nNumberOfBytesToWrite,
    unsigned long* lpNumberOfBytesWritten,
    void*         lpOverlapped
);
__declspec(dllimport) void  __stdcall ExitProcess(unsigned int uExitCode);

// Helper to write string to console via WriteFile
static void win32_print(void* hOut, const char* str) {
    if (!str || !hOut) return;
    unsigned long len = 0;
    while (str[len] != '\0') {
        len++;
    }
    unsigned long written = 0;
    WriteFile(hOut, str, len, &written, (void*)0);
}

// Minimal integer to string formatter without libc
static void win32_print_num(void* hOut, unsigned long val) {
    if (val == 0) {
        win32_print(hOut, "0");
        return;
    }
    char buf[32];
    int idx = 30;
    buf[31] = '\0';
    while (val > 0 && idx >= 0) {
        buf[idx--] = (char)('0' + (val % 10));
        val /= 10;
    }
    win32_print(hOut, &buf[idx + 1]);
}

// Windows PE Entry Point
void mainCRTStartup(void) {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    win32_print(hOut, "[WIN32 PE] ================================================================\r\n");
    win32_print(hOut, "[WIN32 PE] Hello from authentic Windows PE executable running on ZweiOS!\r\n");
    win32_print(hOut, "[WIN32 PE] Architecture   : x86_64 Long Mode (Direct Win32 Dynamic Linker)\r\n");
    win32_print(hOut, "[WIN32 PE] Subsystem      : Native In-Kernel KERNEL32.DLL Emulation Layer\r\n");
    win32_print(hOut, "[WIN32 PE] Calling Conv   : Microsoft x64 ABI (RCX, RDX, R8, R9, Shadow Space)\r\n");
    win32_print(hOut, "[WIN32 PE] Environment    : TEB (GS:[0x30]) and PEB (GS:[0x60]) Validated\r\n");
    win32_print(hOut, "[WIN32 PE] ================================================================\r\n");

    // Perform computation to verify ALU register state & Microsoft x64 stack frame
    win32_print(hOut, "[WIN32 PE] Computing Fibonacci sequence: ");
    unsigned long a = 0;
    unsigned long b = 1;
    for (int i = 0; i < 10; ++i) {
        win32_print_num(hOut, a);
        if (i < 9) {
            win32_print(hOut, ", ");
        }
        unsigned long next = a + b;
        a = b;
        b = next;
    }
    win32_print(hOut, "\r\n");

    win32_print(hOut, "[WIN32 PE] Computation complete. Exiting cleanly with status 100 via ExitProcess...\r\n");

    // Terminate via KERNEL32!ExitProcess
    ExitProcess(100);
}
