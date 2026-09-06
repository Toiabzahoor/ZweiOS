




[bits 64]

section .data
global user_mode_kernel_rsp
global user_mode_user_rsp
global syscall_exit_requested
global syscall_exit_code

user_mode_kernel_rsp:   dq 0
user_mode_user_rsp:     dq 0
syscall_exit_requested: dq 0
syscall_exit_code:      dq 0

section .bss
align 16
kernel_syscall_stack: resb 16384
kernel_syscall_stack_top:

section .text
global syscall_entry
global enter_user_mode
extern syscall_dispatch







enter_user_mode:

    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15


    mov [rel user_mode_kernel_rsp], rsp
    mov qword [rel syscall_exit_requested], 0
    mov qword [rel syscall_exit_code], 0


    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    push 0x1B
    push rsi
    push 0x202
    push 0x23
    push rdi


    iretq











syscall_entry:

    mov [rel user_mode_user_rsp], rsp


    lea rsp, [rel kernel_syscall_stack_top]


    push rcx
    push r11


    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15


    mov bx, 0x10
    mov ds, bx
    mov es, bx



















    push r9
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax

    call syscall_dispatch
    add rsp, 8




    cmp qword [rel syscall_exit_requested], 0
    jne .process_exited



    mov bx, 0x1B
    mov ds, bx
    mov es, bx

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11
    pop rcx


    mov rsp, [rel user_mode_user_rsp]


    o64 sysret

.process_exited:

    mov rax, [rel syscall_exit_code]


    mov rsp, [rel user_mode_kernel_rsp]


    mov bx, 0x10
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx


    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx


    sti


    ret
