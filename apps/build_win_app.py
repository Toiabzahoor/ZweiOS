#!/usr/bin/env python3
"""
ZweiOS Windows PE App Compiler & Embedder
Compiles authentic Windows C programs into standard PE32+ (x86_64) executables
and updates the kernel embedded sample_pe_binary.
"""

import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLANG = Path(r"C:\msys64\ucrt64\bin\clang.EXE")
READELF = Path(r"C:\msys64\ucrt64\bin\llvm-readelf.EXE")
LIBKERNEL32 = Path(r"C:\msys64\ucrt64\lib\libkernel32.a")

SRC_C = ROOT / "apps" / "hello_win.c"
OUT_EXE = ROOT / "build" / "hello_win.exe"
LOADER_CPP = ROOT / "kernel" / "loader" / "loader.cpp"

def compile_pe():
    OUT_EXE.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(CLANG),
        "-target", "x86_64-windows-gnu",
        "-nostdlib", "-fno-builtin",
        "-Wl,-e,mainCRTStartup",
        "-Wl,--image-base,0x800000",
        str(SRC_C),
        str(LIBKERNEL32),
        "-o", str(OUT_EXE)
    ]
    print(f"[BUILD] Compiling {SRC_C.name} -> {OUT_EXE.name}...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print("[ERROR] Compilation failed:")
        print(res.stderr)
        return False

    print(f"[OK] Compiled successfully: {OUT_EXE.stat().st_size} bytes")
    return True

def inspect_pe():
    cmd = [str(READELF), "-h", "-l", "--coff-imports", str(OUT_EXE)]
    res = subprocess.run(cmd, capture_output=True, text=True)
    print("\n--- PE32+ Header & Import Directory ---")
    print(res.stdout)

def embed_into_loader():
    data = OUT_EXE.read_bytes()
    print(f"[EMBED] Updating kernel/loader/loader.cpp with {len(data)} bytes PE32+ binary...")

    lines = []
    lines.append("// Real 64-bit Windows PE32+ Executable:")
    lines.append("// Compiled from apps/hello_win.c with clang -target x86_64-windows-gnu")
    lines.append(f"alignas(16) static const uint8_t sample_pe_binary[] = {{")

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};\n")

    pe_array_code = "\n".join(lines)

    content = LOADER_CPP.read_text(encoding="utf-8")

    start_marker = "alignas(16) static const uint8_t sample_pe_binary[]"
    end_marker = "const uint8_t* loader_get_sample_elf"

    start_idx = content.find(start_marker)
    if start_idx == -1:
        print("[ERROR] Could not find sample_pe_binary marker in loader.cpp")
        return False

    end_idx = content.find(end_marker)
    if end_idx == -1:
        print("[ERROR] Could not find loader_get_sample_elf marker in loader.cpp")
        return False

    # Back up to start of comment above sample_pe_binary if present
    comment_idx = content.rfind("//", 0, start_idx)
    if comment_idx != -1 and (start_idx - comment_idx) < 100:
        start_idx = comment_idx

    new_content = content[:start_idx] + pe_array_code + "\n" + content[end_idx:]
    LOADER_CPP.write_text(new_content, encoding="utf-8")
    print("[OK] loader.cpp updated successfully with authentic PE32+ binary.")
    return True

if __name__ == "__main__":
    if compile_pe():
        inspect_pe()
        embed_into_loader()
