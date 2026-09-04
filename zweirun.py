#!/usr/bin/env python3
"""
ZweiOS Custom Runner & Automated Test Tooling (zweirun.py)
Orchestrates QEMU interactive GUI, ISO packaging, GDB debugging, and headless automated testing.
"""

import sys
import os
import time
import shutil
import subprocess
import argparse
import json
from pathlib import Path

WORKSPACE = Path(__file__).parent.resolve()
BUILD_DIR = WORKSPACE / "build"
KERNEL_ELF = BUILD_DIR / "kernel.elf"
KERNEL32_ELF = BUILD_DIR / "kernel32.elf"
ISO_IMAGE = BUILD_DIR / "zweios.iso"
SERIAL_LOG = BUILD_DIR / "serial.log"
TEST_RESULTS_JSON = BUILD_DIR / "test_results.json"

EXTRA_PATHS = [
    Path(r"C:\msys64\ucrt64\bin"),
    Path(r"C:\msys64\usr\bin"),
    Path(r"C:\msys64\mingw64\bin"),
    Path(r"C:\Program Files\qemu"),
    Path(r"C:\Program Files\CMake\bin"),
]

def find_executable(name: str) -> str:
    """Locates an executable in PATH or standard system directories."""
    found = shutil.which(name)
    if found:
        return found
    for p in EXTRA_PATHS:
        candidate = p / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return str(candidate)
    return name

QEMU = find_executable("qemu-system-x86_64")
XORRISO = find_executable("xorriso")

def run_build(clean: bool = False, verbose: bool = False) -> bool:
    """Builds the kernel using the standalone build module."""
    import build
    if clean:
        build.clean()
    return build.build_kernel(verbose=verbose)

def build_iso() -> bool:
    """Builds a bootable ISO using xorriso."""
    run_build()
    iso_root = BUILD_DIR / "iso_root"
    boot_dir = iso_root / "boot"
    boot_dir.mkdir(parents=True, exist_ok=True)

    # Copy kernel binaries into ISO boot directory
    shutil.copy2(KERNEL_ELF, boot_dir / "kernel.elf")
    shutil.copy2(KERNEL32_ELF, boot_dir / "kernel32.elf")

    # Create limine / grub configuration if needed
    cfg_file = boot_dir / "limine.cfg"
    cfg_file.write_text(
        "TIMEOUT=0\n"
        ":ZweiOS\n"
        "PROTOCOL=multiboot2\n"
        "KERNEL_PATH=boot:///boot/kernel32.elf\n"
    )

    print(f"[ZweiOS Runner] Generating ISO {ISO_IMAGE.name} with xorriso...")
    cmd = [
        XORRISO,
        "-as", "mkisofs",
        "-b", "boot/kernel32.elf",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "-o", str(ISO_IMAGE),
        str(iso_root)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] ISO creation failed:\n{res.stderr}", file=sys.stderr)
        return False
    print(f"[ZweiOS Runner] ISO build complete: {ISO_IMAGE} ({ISO_IMAGE.stat().st_size} bytes)")
    return True

def run_gui(use_iso: bool = False, memory: str = "256M", smp: int = 1, verbose: bool = False):
    """Launches ZweiOS in QEMU GUI mode with serial mirrored to stdio."""
    if use_iso:
        build_iso()
    else:
        run_build(verbose=verbose)

    target_image = ISO_IMAGE if use_iso else KERNEL32_ELF
    cmd = [
        QEMU,
        "-m", memory,
        "-smp", str(smp),
        "-usb",
        "-device", "usb-tablet",
        "-display", "gtk,show-cursor=on",
        "-serial", "stdio",
        "-no-reboot",
    ]
    if use_iso:
        cmd += ["-cdrom", str(target_image)]
    else:
        cmd += ["-kernel", str(target_image)]

    print(f"[ZweiOS Runner] Launching GUI QEMU: {' '.join(cmd)}")
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print("\n[ZweiOS Runner] QEMU terminated by user.")

def run_headless_test(timeout_sec: float = 5.0, use_iso: bool = False, memory: str = "256M", verbose: bool = False) -> bool:
    """
    Executes automated headless test verification.
    Captures COM1 to build/serial.log and monitors isa-debug-exit port 0xF4.
    """
    if use_iso:
        build_iso()
    else:
        run_build(verbose=verbose)

    if SERIAL_LOG.exists():
        SERIAL_LOG.unlink()
    if TEST_RESULTS_JSON.exists():
        TEST_RESULTS_JSON.unlink()

    target_image = ISO_IMAGE if use_iso else KERNEL32_ELF
    cmd = [
        QEMU,
        "-m", memory,
        "-display", "none",
        "-serial", f"file:{SERIAL_LOG}",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        "-no-reboot",
    ]
    if use_iso:
        cmd += ["-cdrom", str(target_image)]
    else:
        cmd += ["-kernel", str(target_image)]

    print(f"[ZweiOS Runner] Executing Headless QEMU Test (timeout: {timeout_sec}s)...")
    start_time = time.time()
    proc = subprocess.Popen(cmd)

    exit_code = None
    try:
        proc.wait(timeout=timeout_sec)
        exit_code = proc.returncode
    except subprocess.TimeoutExpired:
        print(f"[ZweiOS Runner] WARNING: QEMU exceeded {timeout_sec}s timeout; terminating...")
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
        exit_code = -1

    elapsed = time.time() - start_time
    print(f"[ZweiOS Runner] QEMU finished with exit code {exit_code} in {elapsed:.2f}s")

    log_content = ""
    if SERIAL_LOG.exists():
        log_content = SERIAL_LOG.read_text(encoding="utf-8", errors="replace")

    print("\n-------------------- QEMU COM1 SERIAL OUTPUT --------------------")
    print(log_content.strip())
    print("-----------------------------------------------------------------")

    # Decode isa-debug-exit status: exit code 33 = (16 << 1) | 1 -> TEST_SUCCESS
    # exit code 35 = (17 << 1) | 1 -> TEST_FAILURE
    # exit code 37 = (18 << 1) | 1 -> KERNEL_PANIC
    guest_code = ((exit_code - 1) >> 1) if (exit_code is not None and exit_code > 0 and exit_code % 2 == 1) else exit_code

    required_sentinels = [
        "[ZweiOS]",
        "[GDT]",
        "[IDT]",
        "[PIC]",
        "[VGA]",
        "[COM1]",
        "[PS/2]",
        "[PMM]",
        "[VMM]",
        "[HEAP]",
        "[SHELL]",
        "zwei>",
    ]

    sentinel_checks = {s: (s in log_content) for s in required_sentinels}
    all_sentinels_ok = all(sentinel_checks.values())

    fatal_patterns = ["Triple Fault", "Unhandled CPU Exception", "Page Fault at 0x0"]
    fatals_found = [p for p in fatal_patterns if p in log_content]

    test_passed = (all_sentinels_ok and not fatals_found)

    print("\n" + "=" * 62)
    print("                ZweiOS Subsystem & Shell Test Report")
    print("=" * 62)
    print(f" Verdict:        [{'PASSED' if test_passed else 'FAILED'}]")
    print(f" QEMU Exit Code: {exit_code} (Guest status: {guest_code})")
    print(f" Execution Time: {elapsed:.2f}s")
    for s, ok in sentinel_checks.items():
        print(f"   {'[OK]' if ok else '[FAIL]'} Sentinel: \"{s}\"")
    if fatals_found:
        print(f"   [FAIL] Fatal strings detected: {fatals_found}")
    print("=" * 62 + "\n")

    result_data = {
        "verdict": "PASSED" if test_passed else "FAILED",
        "is_success": test_passed,
        "raw_exit_code": exit_code,
        "guest_status_code": guest_code,
        "elapsed_seconds": elapsed,
        "sentinel_checks": sentinel_checks,
        "fatal_errors": fatals_found,
    }
    TEST_RESULTS_JSON.write_text(json.dumps(result_data, indent=2), encoding="utf-8")

    return test_passed

def run_debug(memory: str = "256M", smp: int = 1):
    """Launches QEMU in GDB debug mode waiting on localhost:1234."""
    run_build()
    cmd = [
        QEMU,
        "-kernel", str(KERNEL32_ELF),
        "-m", memory,
        "-smp", str(smp),
        "-s", "-S",
        "-serial", "stdio"
    ]
    print("[ZweiOS Runner] GDB server active on localhost:1234. Connect with: gdb -ex 'target remote :1234' build/kernel.elf")
    subprocess.run(cmd)

def main():
    parser = argparse.ArgumentParser(description="ZweiOS Runner & Test Tool")
    parser.add_argument("action", nargs="?", default=None, choices=["build", "run", "test", "clean", "debug", "iso"],
                        help="Action to perform (build, run, test, clean, debug, iso)")
    parser.add_argument("--build", action="store_true", help="Compile kernel binary")
    parser.add_argument("--clean", action="store_true", help="Clean build artifacts")
    parser.add_argument("--iso", action="store_true", help="Use bootable ISO rather than direct ELF")
    parser.add_argument("--gui", action="store_true", help="Launch interactive QEMU GUI window")
    parser.add_argument("--test", action="store_true", help="Run automated headless test verification")
    parser.add_argument("--debug", action="store_true", help="Launch QEMU with GDB stub waiting on port 1234")
    parser.add_argument("-m", "--memory", default="256M", help="RAM allocated to QEMU VM (default: 256M)")
    parser.add_argument("-s", "--smp", type=int, default=1, help="CPU core count (default: 1)")
    parser.add_argument("-t", "--timeout", type=float, default=5.0, help="Timeout in seconds for headless test (default: 5.0)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose compiler and runner output")

    args = parser.parse_args()

    # Determine requested operation
    if args.action == "clean" or args.clean:
        import build
        build.clean()
    elif args.action == "debug" or args.debug:
        run_debug(memory=args.memory, smp=args.smp)
    elif args.action == "test" or args.test:
        success = run_headless_test(timeout_sec=args.timeout, use_iso=(args.action == "iso" or args.iso), memory=args.memory, verbose=args.verbose)
        sys.exit(0 if success else 1)
    elif args.action == "build" or args.build:
        if args.iso or args.action == "iso":
            success = build_iso()
        else:
            success = run_build(clean=args.clean, verbose=args.verbose)
        sys.exit(0 if success else 1)
    elif args.action == "iso":
        build_iso()
    elif args.action == "run" or args.gui or args.action is None:
        run_gui(use_iso=args.iso, memory=args.memory, smp=args.smp, verbose=args.verbose)

if __name__ == "__main__":
    main()
