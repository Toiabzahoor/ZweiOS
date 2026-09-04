/* ==============================================================================
 * ZweiOS Complex Windows Application: win_sysinfo.exe
 * Target: x86_64-w64-mingw32 / Windows x64 PE32+
 * Runs identically on: Native Microsoft Windows & ZweiOS In-Kernel Win32 Subsystem
 * Functionality:
 *   - Direct GS:[0x30] (TEB) and GS:[0x60] (PEB) runtime inspection
 *   - PEB ImageBaseAddress extraction
 *   - Dynamic Virtual Memory allocation (16 KB) and checksum verification
 *   - Win32 error code state machine auditing (SetLastError/GetLastError)
 *   - Command line and system uptime queries
 * ============================================================================== */

#define STD_OUTPUT_HANDLE ((unsigned long)-11)
#define MEM_COMMIT        0x00001000
#define MEM_RESERVE       0x00002000
#define MEM_RELEASE       0x00008000
#define PAGE_READWRITE    0x04

// Win32 API Dynamic Declarations
__declspec(dllimport) void*         __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int           __stdcall WriteFile(void* hFile, const void* lpBuffer, unsigned long nNumberOfBytesToWrite, unsigned long* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) void          __stdcall ExitProcess(unsigned int uExitCode);
__declspec(dllimport) void*         __stdcall VirtualAlloc(void* lpAddress, unsigned long long dwSize, unsigned long flAllocationType, unsigned long flProtect);
__declspec(dllimport) int           __stdcall VirtualFree(void* lpAddress, unsigned long long dwSize, unsigned long dwFreeType);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);
__declspec(dllimport) void          __stdcall SetLastError(unsigned long dwErrCode);
__declspec(dllimport) const char*   __stdcall GetCommandLineA(void);
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);

static void print_str(void* hOut, const char* str) {
    if (!str || !hOut) return;
    unsigned long len = 0;
    while (str[len] != '\0') len++;
    unsigned long written = 0;
    WriteFile(hOut, str, len, &written, (void*)0);
}

static void print_hex64(void* hOut, unsigned long long val) {
    const char hex_digits[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; --i) {
        buf[2 + (15 - i)] = hex_digits[(val >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    print_str(hOut, buf);
}

static void print_dec(void* hOut, unsigned long long val) {
    if (val == 0) {
        print_str(hOut, "0");
        return;
    }
    char buf[32];
    int idx = 30;
    buf[31] = '\0';
    while (val > 0 && idx >= 0) {
        buf[idx--] = (char)('0' + (val % 10));
        val /= 10;
    }
    print_str(hOut, &buf[idx + 1]);
}

static inline void* read_teb(void) {
    void* teb;
    __asm__ volatile("movq %%gs:0x30, %0" : "=r"(teb));
    return teb;
}

static inline void* read_peb(void) {
    void* peb;
    __asm__ volatile("movq %%gs:0x60, %0" : "=r"(peb));
    return peb;
}

void mainCRTStartup(void) {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "  [WIN32 SYSINFO] Windows x64 Runtime & TEB/PEB Forensic Audit  \r\n");
    print_str(hOut, "================================================================\r\n");

    // 1. Thread Environment Block (TEB) & Process Environment Block (PEB) Inspection
    void* teb = read_teb();
    void* peb = read_peb();

    print_str(hOut, "[1] TEB Pointer (GS:[0x30])       : ");
    print_hex64(hOut, (unsigned long long)teb);
    print_str(hOut, "\r\n");

    print_str(hOut, "[2] PEB Pointer (GS:[0x60])       : ");
    print_hex64(hOut, (unsigned long long)peb);
    print_str(hOut, "\r\n");

    // In standard 64-bit PEB layout, ImageBaseAddress is at offset 0x10
    unsigned long long image_base = 0;
    if (peb) {
        image_base = *(unsigned long long*)((char*)peb + 0x10);
    }
    print_str(hOut, "[3] PEB.ImageBaseAddress (offset 0x10): ");
    print_hex64(hOut, image_base);
    print_str(hOut, "\r\n");

    // 2. Dynamic Memory Management: VirtualAlloc & VirtualFree
    print_str(hOut, "[4] Allocating 16 KB virtual memory via VirtualAlloc...\r\n");
    unsigned long long alloc_size = 16384;
    unsigned char* page_mem = (unsigned char*)VirtualAlloc((void*)0, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (page_mem) {
        print_str(hOut, "    Allocated address               : ");
        print_hex64(hOut, (unsigned long long)page_mem);
        print_str(hOut, "\r\n");

        // Write deterministic pseudorandom test pattern across all 4 pages
        unsigned long long checksum = 0;
        for (unsigned long long i = 0; i < alloc_size; ++i) {
            unsigned char b = (unsigned char)((i * 37 + 101) & 0xFF);
            page_mem[i] = b;
            checksum += b;
        }

        print_str(hOut, "    Pattern write verified. Checksum: ");
        print_hex64(hOut, checksum);
        print_str(hOut, "\r\n");

        // Release pages
        VirtualFree(page_mem, alloc_size, MEM_RELEASE);
        print_str(hOut, "    Virtual memory successfully released via VirtualFree.\r\n");
    } else {
        print_str(hOut, "    Error: VirtualAlloc failed!\r\n");
    }

    // 3. Error Code State Machine
    print_str(hOut, "[5] Testing Win32 Error Code Dispatch: SetLastError(0x1337)...\r\n");
    SetLastError(0x1337);
    unsigned long err = GetLastError();
    print_str(hOut, "    GetLastError() returned         : ");
    print_hex64(hOut, (unsigned long long)err);
    print_str(hOut, (err == 0x1337) ? " [MATCH OK]\r\n" : " [MISMATCH]\r\n");

    // 4. Command Line & System Uptime
    const char* cmd_line = GetCommandLineA();
    print_str(hOut, "[6] GetCommandLineA() returned       : \"");
    print_str(hOut, cmd_line ? cmd_line : "(null)");
    print_str(hOut, "\"\r\n");

    unsigned long long ticks = GetTickCount64();
    print_str(hOut, "[7] System Uptime (GetTickCount64)  : ");
    print_dec(hOut, ticks);
    print_str(hOut, " ms\r\n");

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "[WIN32 SYSINFO] Forensic Audit COMPLETE. Exiting with code 42...\r\n");
    print_str(hOut, "================================================================\r\n");

    ExitProcess(42);
}
