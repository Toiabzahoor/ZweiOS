; ==============================================================================
; ZweiOS - Bare-Metal x86_64 Operating System
; Component: Low-Level Binary Execution Stub & Stack Switcher
; ==============================================================================

[bits 64]

section .text
global loader_invoke_entry
global win32_exit_trampoline
extern g_win32_saved_kernel_rsp
extern g_win32_exit_code

; int64_t loader_invoke_entry(uint64_t entry_point, uint64_t stack_top, bool is_windows)
; System V Calling Convention:
;   RDI = entry_point
;   RSI = stack_top
;   RDX = is_windows (1 if Windows x64 ABI, 0 if System V AMD64 ABI)

loader_invoke_entry:
    ; 1. Preserve host kernel callee-saved registers
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    push rdi        ; save entry_point
    push rdx        ; save is_windows

    ; 2. Switch from kernel stack to the binary's dedicated execution stack
    mov r12, rsp    ; r12 = saved kernel stack pointer
    mov rsp, rsi    ; switch rsp to binary stack
    and rsp, -16    ; enforce 16-byte stack alignment

    ; 3. Setup ABI environment and call the entry point
    test rdx, rdx
    jz .call_linux

.call_windows:
    ; Save host kernel stack pointer for k32_ExitProcess unwinding
    mov [rel g_win32_saved_kernel_rsp], r12

    ; Microsoft x64 ABI: allocate 32 bytes of shadow space on stack
    sub rsp, 32
    call rdi
    add rsp, 32
    jmp .done

.call_linux:
    ; System V AMD64 ABI: standard call
    call rdi

.done:
    ; 4. Restore host kernel stack pointer
    mov rsp, r12

    ; 5. Restore host kernel callee-saved registers
    pop rdx
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return value from the binary is in RAX
    ret

; ------------------------------------------------------------------------------
; win32_exit_trampoline: Unwinds context when k32_ExitProcess is invoked
; ------------------------------------------------------------------------------
win32_exit_trampoline:
    ; Restore host kernel stack pointer directly
    mov rsp, [rel g_win32_saved_kernel_rsp]

    ; Restore host kernel callee-saved registers (matches .done)
    pop rdx
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return exit code from ExitProcess in RAX
    mov rax, [rel g_win32_exit_code]
    ret

