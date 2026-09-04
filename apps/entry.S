/* ==============================================================================
 * ZweiOS - Standard System V AMD64 CRT0 Startup Routine
 * Target: x86_64-unknown-linux-gnu
 * ============================================================================== */

.global _start
.extern app_main

.section .text
_start:
    /* Clear frame pointer to mark outermost stack frame */
    xor %rbp, %rbp

    /* Pass initial System V stack pointer (pointing to argc) to app_main in %rdi */
    mov %rsp, %rdi

    /* Align stack to 16-byte boundary before calling C function */
    and $-16, %rsp

    /* Call high-level C program entry */
    call app_main

    /* Process exit code returned in %rax -> pass to sys_exit in %rdi */
    mov %rax, %rdi
    mov $60, %rax          /* __NR_exit = 60 */
    syscall

    /* Trap if kernel fails to terminate */
1:  hlt
    jmp 1b
