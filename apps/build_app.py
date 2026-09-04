#!/usr/bin/env python3
"""
ZweiOS Linux App Compiler & Embedder
Compiles standalone Linux C programs into standard ELF64 executables
and updates the kernel embedded sample.
"""

import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLANG = Path(r"C:\msys64\ucrt64\bin\clang.EXE")
READELF = Path(r"C:\msys64\ucrt64\bin\llvm-readelf.EXE")

SRC_C = ROOT / "apps" / "hello_linux.c"
SRC_ASM = ROOT / "apps" / "entry.S"
OUT_ELF = ROOT / "build" / "hello_linux.elf"
LOADER_CPP = ROOT / "kernel" / "loader" / "loader.cpp"

def compile_elf():
    OUT_ELF.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(CLANG),
        "-target", "x86_64-unknown-linux-gnu",
        "-nostdlib", "-static", "-fno-builtin",
        "-fuse-ld=lld",
        "-Wl,-Ttext=0x401000",
        "-Wl,--entry=_start",
        str(SRC_ASM),
        str(SRC_C),
        "-o", str(OUT_ELF)
    ]
    print(f"[BUILD] Compiling {SRC_ASM.name} + {SRC_C.name} -> {OUT_ELF.name}...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print("[ERROR] Compilation failed:")
        print(res.stderr)
        return False
    
    print(f"[OK] Compiled successfully: {OUT_ELF.stat().st_size} bytes")
    return True

def inspect_elf():
    cmd = [str(READELF), "-h", "-l", str(OUT_ELF)]
    res = subprocess.run(cmd, capture_output=True, text=True)
    print("\n--- ELF64 Header & Program Headers ---")
    print(res.stdout)

def embed_into_loader():
    data = OUT_ELF.read_bytes()
    print(f"[EMBED] Updating kernel/loader/loader.cpp with {len(data)} bytes ELF binary...")
    
    lines = []
    lines.append("// Real 64-bit Static Linux ELF Executable:")
    lines.append("// Compiled from apps/hello_linux.c with clang -target x86_64-unknown-linux-gnu")
    lines.append(f"alignas(16) static const uint8_t sample_elf_binary[] = {{")
    
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};\n")
    
    elf_array_code = "\n".join(lines)
    
    content = LOADER_CPP.read_text(encoding="utf-8")
    
    # Replace sample_elf_binary
    start_marker = "alignas(16) static const uint8_t sample_elf_binary[] = {"
    end_marker = "alignas(16) static const uint8_t sample_pe_binary[] = {"
    
    start_idx = content.find("alignas(16) static const uint8_t sample_elf_binary[]")
    if start_idx == -1:
        print("[ERROR] Could not find sample_elf_binary in loader.cpp")
        return False
    
    end_idx = content.find(end_marker)
    if end_idx == -1:
        print("[ERROR] Could not find sample_pe_binary marker in loader.cpp")
        return False
    
    new_content = content[:start_idx] + elf_array_code + "\n// Sample Windows PE32+ Executable (2560 bytes):\n" + content[end_idx:]
    LOADER_CPP.write_text(new_content, encoding="utf-8")
    print("[OK] loader.cpp updated successfully.")
    return True

if __name__ == "__main__":
    if compile_elf():
        inspect_elf()
        embed_into_loader()
