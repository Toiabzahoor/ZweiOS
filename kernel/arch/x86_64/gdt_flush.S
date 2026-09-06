





[bits 64]
section .text
global gdt_load










gdt_load:

    lgdt [rdi]


    push rsi
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:

    mov ds, dx
    mov es, dx
    mov ss, dx
    mov fs, dx
    mov gs, dx


    ltr cx


    ret
