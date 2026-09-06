




[bits 64]
section .text

global proc_context_switch
extern gdt_set_kernel_stack








proc_context_switch:

    test rdi, rdi
    jz .skip_save

    mov [rdi + 0*8],  r15
    mov [rdi + 1*8],  r14
    mov [rdi + 2*8],  r13
    mov [rdi + 3*8],  r12
    mov [rdi + 4*8],  rbp
    mov [rdi + 5*8],  rbx


    mov rax, [rsp]
    mov [rdi + 15*8], rax


    lea rax, [rsp + 8]
    mov [rdi + 18*8], rax


    pushfq
    pop rax
    mov [rdi + 17*8], rax

.skip_save:

    test rdx, rdx
    jz .skip_cr3
    mov rax, cr3
    cmp rax, rdx
    je .skip_cr3
    mov cr3, rdx

.skip_cr3:

    test rcx, rcx
    jz .skip_rsp0
    push rsi
    push rdi
    push rdx
    mov rdi, rcx
    call gdt_set_kernel_stack
    pop rdx
    pop rdi
    pop rsi

.skip_rsp0:

    test rsi, rsi
    jz .done

    mov r15, [rsi + 0*8]
    mov r14, [rsi + 1*8]
    mov r13, [rsi + 2*8]
    mov r12, [rsi + 3*8]
    mov rbp, [rsi + 4*8]
    mov rbx, [rsi + 5*8]


    mov rsp, [rsi + 18*8]


    push qword [rsi + 17*8]
    popfq


    mov rax, [rsi + 15*8]
    push rax
    ret

.done:
    ret
