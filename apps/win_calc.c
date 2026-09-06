

#define STD_OUTPUT_HANDLE ((unsigned long)-11)
#define HEAP_ZERO_MEMORY  0x00000008

__declspec(dllimport) void*         __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int           __stdcall WriteFile(void* hFile, const void* lpBuffer, unsigned long nNumberOfBytesToWrite, unsigned long* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) void          __stdcall ExitProcess(unsigned int uExitCode);
__declspec(dllimport) void*         __stdcall GetProcessHeap(void);
__declspec(dllimport) void*         __stdcall HeapAlloc(void* hHeap, unsigned long dwFlags, unsigned long long dwBytes);
__declspec(dllimport) int           __stdcall HeapFree(void* hHeap, unsigned long dwFlags, void* lpMem);
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);

static void print_str(void* hOut, const char* str) {
    if (!str || !hOut) return;
    unsigned long len = 0;
    while (str[len] != '\0') len++;
    unsigned long written = 0;
    WriteFile(hOut, str, len, &written, (void*)0);
}

static void print_u64(void* hOut, unsigned long long val) {
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


static void factorize(void* hOut, unsigned long long n) {
    print_str(hOut, "    Factors of ");
    print_u64(hOut, n);
    print_str(hOut, ": ");

    unsigned long long temp = n;
    int first = 1;


    while ((temp % 2) == 0) {
        if (!first) print_str(hOut, " * ");
        print_u64(hOut, 2);
        first = 0;
        temp /= 2;
    }


    for (unsigned long long d = 3; d * d <= temp; d += 2) {
        while ((temp % d) == 0) {
            if (!first) print_str(hOut, " * ");
            print_u64(hOut, d);
            first = 0;
            temp /= d;
        }
    }

    if (temp > 1) {
        if (!first) print_str(hOut, " * ");
        print_u64(hOut, temp);
    }
    print_str(hOut, "\r\n");
}


static void collatz_analysis(void* hOut, unsigned long long start_val) {
    print_str(hOut, "    Collatz Sequence for n = ");
    print_u64(hOut, start_val);
    print_str(hOut, ":\r\n");

    unsigned long long n = start_val;
    unsigned long long steps = 0;
    unsigned long long peak = start_val;

    while (n != 1 && steps < 10000) {
        if (n & 1) {
            n = 3 * n + 1;
        } else {
            n = n / 2;
        }
        if (n > peak) {
            peak = n;
        }
        steps++;
    }

    print_str(hOut, "      Total Steps to Reach 1 : ");
    print_u64(hOut, steps);
    print_str(hOut, "\r\n");
    print_str(hOut, "      Maximum Peak Value     : ");
    print_u64(hOut, peak);
    print_str(hOut, "\r\n");
}

void mainCRTStartup(void) {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    void* hHeap = GetProcessHeap();

    unsigned long long t_start = GetTickCount64();

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "  [WIN32 CALC] High-Performance Number Theory & Math Engine     \r\n");
    print_str(hOut, "================================================================\r\n");


    print_str(hOut, "[1] Prime Factorization Engine:\r\n");
    factorize(hOut, 1024);
    factorize(hOut, 65535);
    factorize(hOut, 104729);
    factorize(hOut, 1234567890ULL);


    print_str(hOut, "\r\n[2] Collatz 3n+1 Trajectory Analysis:\r\n");
    collatz_analysis(hOut, 27);
    collatz_analysis(hOut, 97);
    collatz_analysis(hOut, 871);


    print_str(hOut, "\r\n[3] Dynamic Heap Allocation via HeapAlloc (512 QWORDs):\r\n");
    unsigned long long* buffer = (unsigned long long*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 512 * sizeof(unsigned long long));
    if (buffer) {

        buffer[0] = 0;
        buffer[1] = 1;
        for (int i = 2; i < 50; ++i) {
            buffer[i] = buffer[i - 1] + buffer[i - 2];
        }
        print_str(hOut, "    Fibonacci(45) computed in dynamic heap: ");
        print_u64(hOut, buffer[45]);
        print_str(hOut, "\r\n");
        print_str(hOut, "    Fibonacci(49) computed in dynamic heap: ");
        print_u64(hOut, buffer[49]);
        print_str(hOut, "\r\n");

        HeapFree(hHeap, 0, buffer);
        print_str(hOut, "    Heap buffer successfully released via HeapFree.\r\n");
    } else {
        print_str(hOut, "    Error: HeapAlloc failed!\r\n");
    }

    unsigned long long t_end = GetTickCount64();
    print_str(hOut, "\r\n[4] Total Benchmark Compute Time: ");
    print_u64(hOut, (t_end >= t_start) ? (t_end - t_start) : 0);
    print_str(hOut, " ms\r\n");

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "[WIN32 CALC] Math Benchmarks Completed. Exiting with code 0...\r\n");
    print_str(hOut, "================================================================\r\n");

    ExitProcess(0);
}
