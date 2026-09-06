




[bits 32]


MB1_MAGIC    equ 0x1BADB002
MB1_FLAGS    equ 0x00000003
MB1_CHECKSUM equ -(MB1_MAGIC + MB1_FLAGS)


MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0


KERNEL_VIRT_BASE equ 0xFFFFFFFF80000000


section .boot_header progbits alloc exec
align 8


multiboot1_header:
    dd MB1_MAGIC
    dd MB1_FLAGS
    dd MB1_CHECKSUM


align 8
multiboot2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd multiboot2_header_end - multiboot2_header
    dd -(MB2_MAGIC + MB2_ARCH + (multiboot2_header_end - multiboot2_header))


    align 8
    dw 1
    dw 0
    dd 12
    dd 6


    align 8
    dw 5
    dw 1
    dd 20
    dd 80
    dd 25
    dd 0


    align 8
    dw 0
    dw 0
    dd 8
multiboot2_header_end:


section .boot_text progbits alloc exec
global _start
extern kmain

_start:

    cli
    cld


    mov [boot_magic], eax
    mov [boot_info_ptr], ebx


    mov esp, early_stack_top


    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid


    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode



    mov edi, pml4_table
    mov ecx, 4096 * 5 / 4
    xor eax, eax
    rep stosd


    mov eax, pdpt_low
    or eax, 0x07
    mov [pml4_table], eax


    mov eax, pdpt_high
    or eax, 0x03
    mov [pml4_table + 511 * 8], eax


    mov eax, pd_table
    or eax, 0x07
    mov [pdpt_low], eax


    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_high + 510 * 8], eax


    mov eax, pd_table_2
    or eax, 0x03
    mov [pdpt_high + 511 * 8], eax



    mov edi, pd_table
    mov eax, 0x87
    mov ecx, 512
.map_pd_entry:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    dec ecx
    jnz .map_pd_entry


    mov edi, pd_table_2
    mov eax, 0x40000083
    mov ecx, 512
.map_pd_entry_2:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    dec ecx
    jnz .map_pd_entry_2


    mov eax, pml4_table
    mov cr3, eax


    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax


    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr



    mov eax, cr0
    and eax, ~((1 << 2) | (1 << 3))
    or eax, (1 << 31) | (1 << 0) | (1 << 1)
    mov cr0, eax


    lgdt [early_gdt_descriptor]


    jmp 0x08:long_mode_entry_phys

.no_cpuid:
    mov dx, 0x3F8
    mov al, 'C'
    out dx, al
    hlt
    jmp .no_cpuid

.no_long_mode:
    mov dx, 0x3F8
    mov al, 'L'
    out dx, al
    hlt
    jmp .no_long_mode




[bits 64]
long_mode_entry_phys:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax


    mov rsp, kernel_stack_top


    mov rax, higher_half_start
    jmp rax




section .text
higher_half_start:



    xor rdi, rdi
    mov edi, [rel boot_magic]
    xor rsi, rsi
    mov esi, [rel boot_info_ptr]


    test rsi, rsi
    jz .call_kernel
    mov rax, KERNEL_VIRT_BASE
    add rsi, rax

.call_kernel:

    xor rbp, rbp


    and rsp, -16


    call kmain


    cli
.halt_loop:
    hlt
    jmp .halt_loop




section .boot_data progbits alloc noexec
align 16
early_gdt:

    dq 0x0000000000000000

    dq 0x00209A0000000000

    dq 0x0000920000000000
early_gdt_end:

align 4
early_gdt_descriptor:
    dw early_gdt_end - early_gdt - 1
    dd early_gdt

boot_magic:
    dd 0
boot_info_ptr:
    dd 0




section .boot_bss nobits alloc noexec
align 4096
global pml4_table
global pdpt_high
global pd_table_2
pml4_table:
    resb 4096
pdpt_low:
    resb 4096
pdpt_high:
    resb 4096
pd_table:
    resb 4096
pd_table_2:
    resb 4096

align 16
early_stack_bottom:
    resb 4096
early_stack_top:




section .bss nobits alloc noexec
align 16
global kernel_stack_bottom
global kernel_stack_top
global stack_bottom
global stack_top
stack_bottom:
kernel_stack_bottom:
    resb 65536
stack_top:
kernel_stack_top:
