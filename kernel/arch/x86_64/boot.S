; ==============================================================================
; ZweiOS Bare-Metal x86_64 Bootstrap & Early Entry
; Supports Multiboot 1, Multiboot 2, and Long-Mode 64-Bit Higher-Half Transition
; ==============================================================================

[bits 32]

; --- Multiboot 1 Header Constants ---
MB1_MAGIC    equ 0x1BADB002
MB1_FLAGS    equ 0x00000003   ; ALIGN (bit 0) + MEMINFO (bit 1)
MB1_CHECKSUM equ -(MB1_MAGIC + MB1_FLAGS)

; --- Multiboot 2 Header Constants ---
MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0            ; i386 32-bit protected mode

; --- Virtual Memory Offsets ---
KERNEL_VIRT_BASE equ 0xFFFFFFFF80000000

; --- Early Boot Header Section (Placed at physical load address 0x100000) ---
section .boot_header progbits alloc exec
align 8

; 1. Multiboot 1 Header (for maximum QEMU -kernel compatibility)
multiboot1_header:
    dd MB1_MAGIC
    dd MB1_FLAGS
    dd MB1_CHECKSUM

; 2. Multiboot 2 Header (8-byte aligned)
align 8
multiboot2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd multiboot2_header_end - multiboot2_header
    dd -(MB2_MAGIC + MB2_ARCH + (multiboot2_header_end - multiboot2_header))

    ; Tag 1: Information Request (Memory Map Tag 6)
    align 8
    dw 1    ; type
    dw 0    ; flags
    dd 12   ; size
    dd 6    ; request memory map tag

    ; Tag 5: Framebuffer Tag (Optional)
    align 8
    dw 5    ; type
    dw 1    ; flags (optional)
    dd 20   ; size
    dd 80   ; width
    dd 25   ; height
    dd 0    ; depth

    ; Tag 0: End Terminator Tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot2_header_end:

; --- Early Boot Code Section (32-bit Protected Mode) ---
section .boot_text progbits alloc exec
global _start
extern kmain

_start:
    ; 1. Disable hardware interrupts and clear direction flag
    cli
    cld

    ; 2. Preserve Bootloader Magic (EAX) and Boot Info Pointer (EBX)
    mov [boot_magic], eax
    mov [boot_info_ptr], ebx

    ; 3. Setup temporary early 32-bit stack
    mov esp, early_stack_top

    ; 4. Check for CPUID support (attempt to flip ID bit 21 in EFLAGS)
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

    ; 5. Check for 64-bit Long Mode (EFER.LME support via CPUID 0x80000001)
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29       ; Bit 29: Long Mode available
    jz .no_long_mode

    ; 6. Set up Early Page Tables for Higher-Half Transition
    ; Clear page tables (PML4, PDPT low, PDPT high, PD 1, PD 2)
    mov edi, pml4_table
    mov ecx, 4096 * 5 / 4
    xor eax, eax
    rep stosd

    ; Link PML4 Entry 0 (Identity mapping lower 1GB for User & Kernel) -> pdpt_low (Present | RW | User = 0x07)
    mov eax, pdpt_low
    or eax, 0x07
    mov [pml4_table], eax

    ; Link PML4 Entry 511 (Higher-half -2GB mapping for Kernel Only) -> pdpt_high (Present | RW = 0x03)
    mov eax, pdpt_high
    or eax, 0x03
    mov [pml4_table + 511 * 8], eax

    ; Link pdpt_low Entry 0 -> pd_table (Present | RW | User = 0x07)
    mov eax, pd_table
    or eax, 0x07
    mov [pdpt_low], eax

    ; Link pdpt_high Entry 510 (0xFFFFFFFF80000000) -> pd_table (Present | RW = 0x03)
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_high + 510 * 8], eax

    ; Link pdpt_high Entry 511 (0xFFFFFFFFC0000000) -> pd_table_2 (Present | RW = 0x03)
    mov eax, pd_table_2
    or eax, 0x03
    mov [pdpt_high + 511 * 8], eax

    ; Populate pd_table: Map 512 entries of 2MB Huge Pages (0 MB .. 1024 MB physical)
    ; Flags: 0x87 = Present | RW | User | PageSize 2MB
    mov edi, pd_table
    mov eax, 0x87           ; 0MB base, flags 0x87 (Present | RW | User | PageSize 2MB)
    mov ecx, 512
.map_pd_entry:
    mov [edi], eax          ; Lower 32 bits
    mov dword [edi + 4], 0  ; Upper 32 bits
    add eax, 0x200000       ; Add 2MB
    add edi, 8              ; Next 8-byte entry
    dec ecx
    jnz .map_pd_entry

    ; Populate pd_table_2: Map 512 entries of 2MB Huge Pages (1024 MB .. 2048 MB physical)
    mov edi, pd_table_2
    mov eax, 0x40000083     ; 1024MB base, flags 0x83 (Present | RW | PageSize 2MB)
    mov ecx, 512
.map_pd_entry_2:
    mov [edi], eax          ; Lower 32 bits
    mov dword [edi + 4], 0  ; Upper 32 bits
    add eax, 0x200000       ; Add 2MB
    add edi, 8              ; Next 8-byte entry
    dec ecx
    jnz .map_pd_entry_2

    ; 7. Load CR3 with Physical Address of PML4
    mov eax, pml4_table
    mov cr3, eax

    ; 8. Enable Physical Address Extension (CR4.PAE = bit 5), OSFXSR (bit 9), OSXMMEXCPT (bit 10)
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; 9. Enable Long Mode (LME) and No-Execute (NXE) in IA32_EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    ; 10. Enable Paging (CR0.PG = bit 31), Protected Mode (CR0.PE = bit 0), MP (bit 1)
    ; Clear EM (bit 2) and TS (bit 3) for SSE instructions
    mov eax, cr0
    and eax, ~((1 << 2) | (1 << 3))
    or eax, (1 << 31) | (1 << 0) | (1 << 1)
    mov cr0, eax

    ; 11. Load Early 64-bit GDT
    lgdt [early_gdt_descriptor]

    ; 12. Far Jump into 64-bit Long Mode Code Segment (0x08) within physical memory
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

; ==============================================================================
; 64-Bit Physical Entry Stub
; ==============================================================================
[bits 64]
long_mode_entry_phys:
    ; 1. Reload data segment registers with 64-bit data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; 2. Initialize 64-bit Higher-Half Stack Pointer
    mov rsp, kernel_stack_top

    ; 3. Jump to higher-half virtual address in .text section
    mov rax, higher_half_start
    jmp rax

; ==============================================================================
; Higher-Half 64-Bit Kernel Execution (Virtual Address Space)
; ==============================================================================
section .text
higher_half_start:
    ; 4. Pass Bootloader Arguments to kmain (System V AMD64 ABI):
    ; RDI (1st param) = uint64_t magic
    ; RSI (2nd param) = void* boot_info
    xor rdi, rdi
    mov edi, [rel boot_magic]
    xor rsi, rsi
    mov esi, [rel boot_info_ptr]

    ; Convert 32-bit physical boot_info pointer to higher-half virtual address if not null
    test rsi, rsi
    jz .call_kernel
    mov rax, KERNEL_VIRT_BASE
    add rsi, rax

.call_kernel:
    ; Clear RBP frame pointer for outer stack boundary
    xor rbp, rbp

    ; Ensure 16-byte stack alignment before calling C++ code
    and rsp, -16

    ; 5. Call C++ Kernel Entry Point
    call kmain

    ; 6. Halt loop if kmain returns
    cli
.halt_loop:
    hlt
    jmp .halt_loop

; ==============================================================================
; Early GDT Table (Physical Address Space)
; ==============================================================================
section .boot_data progbits alloc noexec
align 16
early_gdt:
    ; 0x00: Null Descriptor
    dq 0x0000000000000000
    ; 0x08: 64-bit Kernel Code Segment (Present, Ring 0, Exec/Read, Long Mode)
    dq 0x00209A0000000000
    ; 0x10: 64-bit Kernel Data Segment (Present, Ring 0, Read/Write)
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

; ==============================================================================
; Early Page Tables & Temporary 32-bit Stack (Physical BSS)
; ==============================================================================
section .boot_bss nobits alloc noexec
align 4096
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

; ==============================================================================
; 64-Bit Higher-Half Kernel Stack (Virtual BSS)
; ==============================================================================
section .bss nobits alloc noexec
align 16
global kernel_stack_bottom
global kernel_stack_top
global stack_bottom
global stack_top
stack_bottom:
kernel_stack_bottom:
    resb 65536 ; 64 KB kernel stack
stack_top:
kernel_stack_top:
