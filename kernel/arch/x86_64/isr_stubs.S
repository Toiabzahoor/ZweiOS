; ==============================================================================
; ZweiOS Bare-Metal x86_64 Architecture Support
; Component: Assembly Interrupt & Exception Stubs (256-entry IDT support)
; Target: NASM ELF64
; ==============================================================================

[bits 64]
section .text

extern isr_handler
global idt_load

; ------------------------------------------------------------------------------
; void idt_load(const idtr_t* idtr);
; RDI = Pointer to 10-byte IDTR descriptor
; ------------------------------------------------------------------------------
idt_load:
    lidt [rdi]
    ret

; ------------------------------------------------------------------------------
; Macros for ISR Stub Generation
; ------------------------------------------------------------------------------
%macro ISR_NOERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword 0                ; Dummy error code
    push qword %1               ; Interrupt vector number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push qword %1               ; Interrupt vector number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; ------------------------------------------------------------------------------
; CPU Exceptions (Vectors 0 - 31)
; ------------------------------------------------------------------------------
ISR_NOERRCODE 0                 ; #DE Divide-by-Zero
ISR_NOERRCODE 1                 ; #DB Debug
ISR_NOERRCODE 2                 ; NMI Non-Maskable Interrupt
ISR_NOERRCODE 3                 ; #BP Breakpoint
ISR_NOERRCODE 4                 ; #OF Overflow
ISR_NOERRCODE 5                 ; #BR BOUND Range Exceeded
ISR_NOERRCODE 6                 ; #UD Invalid Opcode
ISR_NOERRCODE 7                 ; #NM Device Not Available
ISR_ERRCODE   8                 ; #DF Double Fault (Error Code = 0)
ISR_NOERRCODE 9                 ; Coprocessor Segment Overrun
ISR_ERRCODE   10                ; #TS Invalid TSS
ISR_ERRCODE   11                ; #NP Segment Not Present
ISR_ERRCODE   12                ; #SS Stack-Segment Fault
ISR_ERRCODE   13                ; #GP General Protection Fault
ISR_ERRCODE   14                ; #PF Page Fault
ISR_NOERRCODE 15                ; Reserved / Spurious
ISR_NOERRCODE 16                ; #MF x87 FPU Error
ISR_ERRCODE   17                ; #AC Alignment Check
ISR_NOERRCODE 18                ; #MC Machine Check
ISR_NOERRCODE 19                ; #XM SIMD Exception
ISR_NOERRCODE 20                ; #VE Virtualization Exception
ISR_ERRCODE   21                ; #CP Control Protection Exception
ISR_NOERRCODE 22                ; Reserved
ISR_NOERRCODE 23                ; Reserved
ISR_NOERRCODE 24                ; Reserved
ISR_NOERRCODE 25                ; Reserved
ISR_NOERRCODE 26                ; Reserved
ISR_NOERRCODE 27                ; Reserved
ISR_NOERRCODE 28                ; #HV Hypervisor Injection
ISR_ERRCODE   29                ; #VC VMM Communication
ISR_ERRCODE   30                ; #SX Security Exception
ISR_NOERRCODE 31                ; Reserved

; ------------------------------------------------------------------------------
; Hardware IRQs 0 - 15 (Vectors 32 - 47)
; ------------------------------------------------------------------------------
%assign i 32
%rep 16
    ISR_NOERRCODE i
%assign i i+1
%endrep

; Vector 128 (0x80 System Call)
ISR_NOERRCODE 128

; Vector 255 (0xFF Spurious Interrupt)
ISR_NOERRCODE 255

; ------------------------------------------------------------------------------
; Unified ISR Common Stub
; Stack frame constructed here matches struct cpu_registers_t exactly.
; ------------------------------------------------------------------------------
isr_common_stub:
    ; 1. Push all General Purpose Registers (15 registers in reverse order)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Read and push CR2 (Page Fault Linear Address)
    mov rax, cr2
    push rax

    ; 3. System V AMD64 ABI: First argument (RDI) = pointer to cpu_registers_t
    mov rdi, rsp

    ; 4. Clear Direction Flag (DF) required by C ABI for string operations
    cld

    ; 5. Call C++ High-Level Interrupt Dispatcher
    call isr_handler

    ; 6. Restore CR2 scratch slot
    pop rax

    ; 7. Restore General Purpose Registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 8. Pop vector number and error code (16 bytes)
    add rsp, 16

    ; 9. Return from interrupt (Pops RIP, CS, RFLAGS, RSP, SS)
    iretq
