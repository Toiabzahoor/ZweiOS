#!/usr/bin/env python3
"""
ZweiOS Windows PE Multi-App Compiler, Verifier & Embedder
Compiles authentic Windows C programs into standard PE32+ (x86_64) executables,
runs and verifies them natively on Microsoft Windows,
and embeds their byte arrays for ZweiOS kernel execution.
"""

import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLANG = Path(r"C:\msys64\ucrt64\bin\clang.EXE")
READELF = Path(r"C:\msys64\ucrt64\bin\llvm-readelf.EXE")
LIBKERNEL32 = Path(r"C:\msys64\ucrt64\lib\libkernel32.a")
BUILD_DIR = ROOT / "build"

APPS = [
    {
        "name": "win_sysinfo",
        "src": ROOT / "apps" / "win_sysinfo.c",
        "exe": BUILD_DIR / "win_sysinfo.exe",
        "expected_code": 42
    },
    {
        "name": "win_calc",
        "src": ROOT / "apps" / "win_calc.c",
        "exe": BUILD_DIR / "win_calc.exe",
        "expected_code": 0
    },
    {
        "name": "win_life",
        "src": ROOT / "apps" / "win_life.c",
        "exe": BUILD_DIR / "win_life.exe",
        "expected_code": 7
    }
]

def compile_app(app):
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(CLANG),
        "-target", "x86_64-windows-gnu",
        "-nostdlib", "-fno-builtin",
        "-Wl,-e,mainCRTStartup",
        "-Wl,--image-base,0x800000",
        str(app["src"]),
        str(LIBKERNEL32),
        "-o", str(app["exe"])
    ]
    print(f"[BUILD] Compiling {app['src'].name} -> {app['exe'].name}...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Compilation of {app['name']} failed:")
        print(res.stderr)
        return False
    size = app["exe"].stat().st_size
    print(f"[OK] {app['exe'].name} compiled successfully ({size} bytes).")
    return True

def run_native_windows(app):
    print(f"\n[RUN NATIVE] Executing {app['exe'].name} directly on host Microsoft Windows NT...")
    res = subprocess.run([str(app["exe"])], capture_output=True, text=True)
    print("--- Output Start ---")
    print(res.stdout, end="")
    print("--- Output End ---")
    print(f"Exit Code: {res.returncode} (Expected: {app['expected_code']})")
    if res.returncode == app["expected_code"]:
        print(f"[PASS] {app['name']} verified on native Windows!")
        return True
    else:
        print(f"[FAIL] Exit code mismatch! Expected {app['expected_code']}, got {res.returncode}")
        return False

def generate_sample_win_apps_cpp():
    target = ROOT / "kernel" / "loader" / "sample_win_apps.cpp"
    print(f"\n[EMBED] Generating {target.relative_to(ROOT)}...")

    lines = [
        "/* ==============================================================================",
        " * ZweiOS - Embedded Complex Windows Applications",
        " * Generated automatically by apps/build_all_win_apps.py",
        " * Contains authentic PE32+ binaries compiled from: apps/win_sysinfo.c, apps/win_calc.c, apps/win_life.c",
        " * ============================================================================== */\n",
        "#include \"loader/loader.hpp\"\n",
        "namespace loader {\n"
    ]

    for app in APPS:
        data = app["exe"].read_bytes()
        lines.append(f"// Real 64-bit Windows PE32+ Executable: {app['name']}.exe ({len(data)} bytes)")
        lines.append(f"alignas(16) static const uint8_t sample_{app['name']}_binary[] = {{")
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
            lines.append(f"    {hex_vals},")
        lines.append("};\n")

        lines.append(f"const uint8_t* loader_get_sample_{app['name']}(size_t* out_size) {{")
        lines.append("    if (out_size) {")
        lines.append(f"        *out_size = sizeof(sample_{app['name']}_binary);")
        lines.append("    }")
        lines.append(f"    return sample_{app['name']}_binary;")
        lines.append("}\n")

    lines.append("} // namespace loader\n")

    target.write_text("\n".join(lines), encoding="utf-8")
    print(f"[OK] {target.name} generated successfully ({target.stat().st_size} bytes).")

if __name__ == "__main__":
    all_ok = True
    for app in APPS:
        if not compile_app(app):
            all_ok = False
            break
        if not run_native_windows(app):
            all_ok = False
            break

    if all_ok:
        generate_sample_win_apps_cpp()
        print("\n========================================================")
        print("ALL 3 WINDOWS APPLICATIONS COMPILED AND PASSED ON WINDOWS!")
        print("========================================================")
    else:
        print("\n[ERROR] One or more applications failed verification.")
        exit(1)

