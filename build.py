#!/usr/bin/env python3
"""
ZweiOS Standalone Build Engine
Builds the x86_64 higher-half freestanding kernel using Clang++, NASM, and LLD.
"""

import sys
import os
import shutil
import subprocess
import argparse
from pathlib import Path

WORKSPACE = Path(__file__).parent.resolve()
BUILD_DIR = WORKSPACE / "build"
KERNEL_DIR = WORKSPACE / "kernel"
LINKER_SCRIPT = WORKSPACE / "linker.ld"
KERNEL_ELF = BUILD_DIR / "kernel.elf"
KERNEL32_ELF = BUILD_DIR / "kernel32.elf"
ISO_IMAGE = BUILD_DIR / "zweios.iso"

# Known toolchain search directories on Windows
EXTRA_TOOL_PATHS = [
    Path(r"C:\msys64\ucrt64\bin"),
    Path(r"C:\msys64\usr\bin"),
    Path(r"C:\msys64\mingw64\bin"),
    Path(r"C:\Program Files\LLVM\bin"),
    Path(r"C:\Program Files\CMake\bin"),
]

def find_tool(tool_name: str) -> str:
    """Finds an executable in standard PATH or fallback directories."""
    found = shutil.which(tool_name)
    if found:
        return found
    for p in EXTRA_TOOL_PATHS:
        candidate = p / (tool_name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return str(candidate)
    return tool_name

CLANGXX = find_tool("clang++")
NASM = find_tool("nasm")
LD_LLD = find_tool("ld.lld")
LLVM_OBJCOPY = find_tool("llvm-objcopy")
XORRISO = find_tool("xorriso")

CXX_FLAGS = [
    "-target", "x86_64-unknown-none-elf",
    "-std=c++20",
    "-ffreestanding",
    "-fno-builtin",
    "-nostdlib",
    "-nostdinc++",
    "-fno-exceptions",
    "-fno-rtti",
    "-mno-red-zone",
    "-mno-mmx",
    "-mno-sse",
    "-mno-sse2",
    "-mcmodel=kernel",
    "-fno-pie",
    "-fno-pic",
    "-fno-stack-protector",
    "-fno-omit-frame-pointer",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-O2",
    "-g",
    f"-I{KERNEL_DIR}"
]

NASM_FLAGS = ["-f", "elf64", "-g", "-F", "dwarf"]

LDFLAGS = [
    "-m", "elf_x86_64",
    "-T", str(LINKER_SCRIPT),
    "-nostdlib",
    "-static",
    "-z", "max-page-size=0x1000",
    "--gc-sections"
]

def check_tools():
    tools = [
        ("C++ Compiler (clang++)", CLANGXX),
        ("Assembler (nasm)", NASM),
        ("Linker (ld.lld)", LD_LLD),
        ("Object Copy (llvm-objcopy)", LLVM_OBJCOPY),
    ]
    missing = []
    for name, path in tools:
        if not shutil.which(path) and not Path(path).is_file():
            missing.append(f"  - {name}: '{path}' not found")
    if missing:
        print("[ERROR] Required build tools missing:", file=sys.stderr)
        for m in missing:
            print(m, file=sys.stderr)
        sys.exit(1)

def compile_asm(src_path: Path, obj_path: Path, verbose: bool = False):
    cmd = [NASM] + NASM_FLAGS + [str(src_path), "-o", str(obj_path)]
    if verbose:
        print("  [NASM] ", " ".join(cmd))
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Assembling {src_path}:\n{res.stderr}", file=sys.stderr)
        sys.exit(1)

def compile_cxx(src_path: Path, obj_path: Path, verbose: bool = False):
    cmd = [CLANGXX] + CXX_FLAGS + ["-c", str(src_path), "-o", str(obj_path)]
    if verbose:
        print("  [CXX]  ", " ".join(cmd))
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Compiling {src_path}:\n{res.stderr}", file=sys.stderr)
        sys.exit(1)

def build_kernel(verbose: bool = False) -> bool:
    check_tools()
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    obj_dir = BUILD_DIR / "obj"
    obj_dir.mkdir(parents=True, exist_ok=True)

    print("[ZweiOS Build] Scanning sources in kernel/...")
    asm_sources = sorted(list(KERNEL_DIR.rglob("*.S")) + list(KERNEL_DIR.rglob("*.asm")))
    cxx_sources = sorted(list(KERNEL_DIR.rglob("*.cpp")))

    objects = []

    # Compile Assembly files
    for src in asm_sources:
        obj = obj_dir / f"{src.stem}_asm.o"
        print(f"  [ASM] {src.relative_to(WORKSPACE)}")
        compile_asm(src, obj, verbose)
        objects.append(obj)

    # Compile C++ files
    for src in cxx_sources:
        obj = obj_dir / f"{src.stem}.o"
        print(f"  [CXX] {src.relative_to(WORKSPACE)}")
        compile_cxx(src, obj, verbose)
        objects.append(obj)

    # Link ELF64 Kernel
    print(f"  [LD]  Linking {KERNEL_ELF.relative_to(WORKSPACE)}...")
    link_cmd = [LD_LLD] + LDFLAGS + ["-o", str(KERNEL_ELF)] + [str(o) for o in objects]
    if verbose:
        print("  [LD]   ", " ".join(link_cmd))
    res = subprocess.run(link_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Linking kernel.elf:\n{res.stderr}", file=sys.stderr)
        sys.exit(1)

    # Generate 32-bit ELF container for QEMU multiboot direct loading
    print(f"  [OBJCOPY] Generating {KERNEL32_ELF.relative_to(WORKSPACE)}...")
    objcopy_cmd = [LLVM_OBJCOPY, "-O", "elf32-i386", str(KERNEL_ELF), str(KERNEL32_ELF)]
    res = subprocess.run(objcopy_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Objcopy generation:\n{res.stderr}", file=sys.stderr)
        sys.exit(1)

    print(f"[ZweiOS Build] Build complete: {KERNEL_ELF.name} ({KERNEL_ELF.stat().st_size} bytes), {KERNEL32_ELF.name} ({KERNEL32_ELF.stat().st_size} bytes)")
    return True

def clean():
    if BUILD_DIR.exists():
        print(f"[ZweiOS Build] Cleaning {BUILD_DIR}...")
        shutil.rmtree(BUILD_DIR)
        print("[ZweiOS Build] Clean complete.")

def main():
    parser = argparse.ArgumentParser(description="ZweiOS Standalone Build Engine")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--rebuild", action="store_true", help="Clean and rebuild kernel")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose compiler/linker output")
    args = parser.parse_args()

    if args.clean:
        clean()
        return

    if args.rebuild:
        clean()

    build_kernel(args.verbose)

if __name__ == "__main__":
    main()
