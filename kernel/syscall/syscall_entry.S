; ==============================================================================
; ZweiOS - Bare-Metal x86_64 Operating System
; Component: Fast System Call Low-Level Assembly Entry (IA32_LSTAR / sysret)
; ==============================================================================

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

; ------------------------------------------------------------------------------
; int64_t enter_user_mode(uint64_t user_rip, uint64_t user_rsp)
; System V ABI:
;   RDI = user_rip
;   RSI = user_rsp
; ------------------------------------------------------------------------------
enter_user_mode:
    ; 1. Preserve host kernel callee-saved registers
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; 2. Save kernel stack pointer for return upon sys_exit
    mov [rel user_mode_kernel_rsp], rsp
    mov qword [rel syscall_exit_requested], 0
    mov qword [rel syscall_exit_code], 0

    ; 3. Load user data segment (0x1B = GDT Index 3, RPL 3) into DS, ES, FS, GS
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 4. Prepare iretq stack frame (SS, RSP, RFLAGS, CS, RIP)
    push 0x1B           ; SS = USER_DS (0x18 | 3)
    push rsi            ; RSP = user_rsp
    push 0x202          ; RFLAGS = IF (bit 9) | Reserved (bit 1)
    push 0x23           ; CS = USER_CS (0x20 | 3)
    push rdi            ; RIP = user_rip

    ; 5. Transition to Ring 3 User Mode
    iretq

; ------------------------------------------------------------------------------
; syscall_entry: Target of IA32_LSTAR MSR
; Invoked when user code in Ring 3 executes the 'syscall' instruction.
; Hardware automatically:
;   RCX = user RIP
;   R11 = user RFLAGS
;   CS = STAR[47..32] (0x08)
;   SS = STAR[47..32] + 8 (0x10)
;   RFLAGS &= ~IA32_FMASK (IF cleared)
; ------------------------------------------------------------------------------
syscall_entry:
    ; 1. Save user stack pointer
    mov [rel user_mode_user_rsp], rsp

    ; 2. Switch to trusted kernel syscall stack
    lea rsp, [rel kernel_syscall_stack_top]

    ; 3. Save user return context (RIP & RFLAGS)
    push rcx
    push r11

    ; 4. Save callee-saved registers
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; 5. Set kernel data segments (use bx to preserve syscall number in rax)
    mov bx, 0x10
    mov ds, bx
    mov es, bx

    ; 6. Route Linux syscall arguments to C++ syscall_dispatch(num, a1..a6)
    ; Linux System V syscall registers:
    ;   RAX = syscall number
    ;   RDI = arg 1
    ;   RSI = arg 2
    ;   RDX = arg 3
    ;   R10 = arg 4 (Note: syscall uses R10 instead of RCX)
    ;   R8  = arg 5
    ;   R9  = arg 6
    ;
    ; C++ System V calling convention for syscall_dispatch:
    ;   RDI = num (from RAX)
    ;   RSI = a1  (from RDI)
    ;   RDX = a2  (from RSI)
    ;   RCX = a3  (from RDX)
    ;   R8  = a4  (from R10)
    ;   R9  = a5  (from R8)
    ;   [RSP] = a6 (from R9 pushed on stack)
    push r9             ; 7th argument (a6)
    mov r9, r8          ; 6th argument (a5)
    mov r8, r10         ; 5th argument (a4)
    mov rcx, rdx        ; 4th argument (a3)
    mov rdx, rsi        ; 3rd argument (a2)
    mov rsi, rdi        ; 2nd argument (a1)
    mov rdi, rax        ; 1st argument (num)

    call syscall_dispatch
    add rsp, 8          ; clean up 7th argument

    ; Return value from syscall_dispatch is in RAX

    ; 7. Check if process exit was requested (sys_exit / sys_exit_group)
    cmp qword [rel syscall_exit_requested], 0
    jne .process_exited

    ; 8. Process continuation: restore user state and return via sysretq
    ; Restore user data segment
    mov bx, 0x1B
    mov ds, bx
    mov es, bx

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11             ; restore user RFLAGS
    pop rcx             ; restore user RIP

    ; Restore user stack pointer
    mov rsp, [rel user_mode_user_rsp]

    ; Return to Ring 3 (64-bit operand SYSRET)
    o64 sysret

.process_exited:
    ; Process called sys_exit: return directly to the kernel caller of enter_user_mode
    mov rax, [rel syscall_exit_code]

    ; Restore kernel stack pointer
    mov rsp, [rel user_mode_kernel_rsp]

    ; Restore kernel data segments (DS, ES, FS, GS)
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    ; Restore host kernel callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Re-enable interrupts before returning to kernel shell
    sti

    ; Return back to the shell / caller of enter_user_mode with exit code in RAX!
    ret
