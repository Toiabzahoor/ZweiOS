




[bits 64]

section .text
global loader_invoke_entry
global win32_exit_trampoline
extern g_win32_saved_kernel_rsp
extern g_win32_exit_code







loader_invoke_entry:

    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rdx


    mov r12, rsp
    mov rsp, rsi
    and rsp, -16


    test rdx, rdx
    jz .call_linux

.call_windows:

    mov [rel g_win32_saved_kernel_rsp], r12


    sub rsp, 32
    call rdi
    add rsp, 32
    jmp .done

.call_linux:

    call rdi

.done:

    mov rsp, r12


    pop rdx
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx


    ret




win32_exit_trampoline:

    mov rsp, [rel g_win32_saved_kernel_rsp]


    pop rdx
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx


    mov rax, [rel g_win32_exit_code]
    ret

