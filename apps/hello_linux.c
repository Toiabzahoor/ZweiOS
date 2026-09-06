

typedef unsigned long  size_t;
typedef long           int64_t;
typedef unsigned long  uint64_t;


static inline int64_t sys_write(int fd, const void* buf, size_t count) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static void print(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

static void print_num(uint64_t n) {
    char buf[32];
    if (n == 0) {
        print("0");
        return;
    }
    int idx = 30;
    buf[31] = '\0';
    while (n > 0 && idx >= 0) {
        buf[idx--] = '0' + (n % 10);
        n /= 10;
    }
    print(&buf[idx + 1]);
}

int app_main(const uint64_t* sp) {
    uint64_t argc = sp[0];
    const char* argv0 = (const char*)sp[1];

    print("\r\n==============================================================\r\n");
    print("[LINUX ELF64] Hello from real Linux ELF binary running in Ring 3!\r\n");
    print("[LINUX ELF64] Operating System: ZweiOS (Bare-Metal POSIX ABI)\r\n");

    if (argv0) {
        print("[LINUX ELF64] argv[0] confirmed: ");
        print(argv0);
        print("\r\n");
    }

    print("[LINUX ELF64] Stack argc: ");
    print_num(argc);
    print("\r\n");


    print("[LINUX ELF64] Calculating Fibonacci sequence in Ring 3: ");
    uint64_t a = 0, b = 1;
    for (int i = 0; i < 10; i++) {
        print_num(a);
        print(", ");
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    print_num(a);
    print("\r\n");

    print("[LINUX ELF64] Computation & System V stack verified!\r\n");
    print("[LINUX ELF64] Exiting cleanly with status 42 via sys_exit...\r\n");
    print("==============================================================\r\n");

    return 42;
}
