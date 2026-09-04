; ==============================================================================
; ZweiOS Bare-Metal x86_64 Architecture Support
; Component: GDT & Task Register (TR) Hardware Reload Assembly Routine
; Target: NASM ELF64
; ==============================================================================

[bits 64]
section .text
global gdt_load

; ------------------------------------------------------------------------------
; void gdt_load(const gdtr_t* gdtr, uint16_t code_sel, uint16_t data_sel, uint16_t tss_sel);
;
; Parameters (System V AMD64 ABI):
;   RDI (1st) = const gdtr_t* gdtr   (Pointer to 10-byte GDTR descriptor struct)
;   RSI (2nd) = uint16_t code_sel     (64-bit Kernel Code Segment Selector, 0x08)
;   RDX (3rd) = uint16_t data_sel     (64-bit Kernel Data Segment Selector, 0x10)
;   RCX (4th) = uint16_t tss_sel      (64-bit TSS Segment Selector, 0x28)
; ------------------------------------------------------------------------------
gdt_load:
    ; 1. Load GDTR with new 64-bit GDT base and limit
    lgdt [rdi]

    ; 2. Reload Code Segment (CS) via Far Return (retfq)
    push rsi                        ; Push new Code Segment selector (0x08)
    lea rax, [rel .reload_cs]       ; Target RIP label
    push rax                        ; Push target RIP
    retfq                           ; Atomically load CS=0x08 and jump to .reload_cs

.reload_cs:
    ; 3. Reload Data Segment Registers (DS, ES, SS, FS, GS) with data_sel (0x10)
    mov ds, dx
    mov es, dx
    mov ss, dx
    mov fs, dx
    mov gs, dx

    ; 4. Load Task Register (TR) with tss_sel (0x28)
    ltr cx

    ; 5. Return to C++ caller
    ret
