cmake_minimum_required(VERSION 3.20)
project(ZweiOS C CXX ASM)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Freestanding x86_64 target compilation flags
set(FREESTANDING_CXX_FLAGS
    -target x86_64-unknown-none-elf
    -std=c++20
    -ffreestanding
    -fno-builtin
    -nostdlib
    -nostdinc++
    -fno-exceptions
    -fno-rtti
    -mno-red-zone
    -mno-mmx
    -mno-sse
    -mno-sse2
    -mcmodel=kernel
    -fno-pie
    -fno-pic
    -fno-stack-protector
    -fno-omit-frame-pointer
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wpedantic
    -O2
    -g
)

add_compile_options(${FREESTANDING_CXX_FLAGS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel)

# Linker script and binary paths
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set(KERNEL_ELF "${CMAKE_CURRENT_BINARY_DIR}/kernel.elf")
set(KERNEL32_ELF "${CMAKE_CURRENT_BINARY_DIR}/kernel32.elf")
set(ISO_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/zweios.iso")

# Collect C++ kernel source files
file(GLOB_RECURSE KERNEL_CXX_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.cpp")
file(GLOB_RECURSE KERNEL_ASM_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.S" "${CMAKE_CURRENT_SOURCE_DIR}/kernel/*.asm")

# Assemble all assembly files using NASM
set(KERNEL_OBJS "")
foreach(ASM_SRC ${KERNEL_ASM_SRCS})
    get_filename_component(ASM_NAME ${ASM_SRC} NAME_WE)
    set(ASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${ASM_NAME}_asm.o")
    add_custom_command(
        OUTPUT "${ASM_OBJ}"
        COMMAND nasm -f elf64 -g -F dwarf "${ASM_SRC}" -o "${ASM_OBJ}"
        DEPENDS "${ASM_SRC}"
        COMMENT "Assembling ${ASM_SRC} with NASM"
    )
    list(APPEND KERNEL_OBJS "${ASM_OBJ}")
endforeach()

# Compile C++ objects
foreach(SRC ${KERNEL_CXX_SRCS})
    get_filename_component(BASENAME ${SRC} NAME_WE)
    set(OBJ "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.o")
    add_custom_command(
        OUTPUT "${OBJ}"
        COMMAND clang++ ${FREESTANDING_CXX_FLAGS} -I"${CMAKE_CURRENT_SOURCE_DIR}/kernel" -c "${SRC}" -o "${OBJ}"
        DEPENDS "${SRC}"
        COMMENT "Compiling ${SRC}"
    )
    list(APPEND KERNEL_OBJS "${OBJ}")
endforeach()

# Link kernel.elf with ld.lld
add_custom_command(
    OUTPUT "${KERNEL_ELF}"
    COMMAND ld.lld -m elf_x86_64 -T "${LINKER_SCRIPT}" -nostdlib -static -z max-page-size=0x1000 --gc-sections -o "${KERNEL_ELF}" ${KERNEL_OBJS}
    DEPENDS "${LINKER_SCRIPT}" ${KERNEL_OBJS}
    COMMENT "Linking ${KERNEL_ELF} with ld.lld"
)

# Convert to kernel32.elf for QEMU multiboot direct loading
add_custom_command(
    OUTPUT "${KERNEL32_ELF}"
    COMMAND llvm-objcopy -O elf32-i386 "${KERNEL_ELF}" "${KERNEL32_ELF}"
    DEPENDS "${KERNEL_ELF}"
    COMMENT "Creating 32-bit ELF container ${KERNEL32_ELF}"
)

add_custom_target(kernel ALL DEPENDS "${KERNEL32_ELF}" "${KERNEL_ELF}")

# Custom Target: run (GUI mode)
add_custom_target(run
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/zweirun.py" --gui
    DEPENDS "${KERNEL32_ELF}"
    COMMENT "Launching ZweiOS in QEMU GUI mode"
)

# Custom Target: test_headless (Headless COM1 verification)
add_custom_target(test_headless
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/zweirun.py" --test
    DEPENDS "${KERNEL32_ELF}"
    COMMENT "Running automated headless test suite"
)

# Custom Target: iso (Bootable ISO)
add_custom_target(iso
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/zweirun.py" --iso --build
    DEPENDS "${KERNEL_ELF}"
    COMMENT "Building bootable ZweiOS ISO"
)
