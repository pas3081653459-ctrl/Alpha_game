#!/usr/bin/env python3
"""Compile and execute the portable rules used by UE components.

This verifies shared rule calculations, not Unreal/UHT, rendering or replication.
Build products are temporary and the compiler/test process exit code is retained.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true', help='Enable UndefinedBehaviorSanitizer')
    args = parser.parse_args()
    compiler = shutil.which("g++")
    if compiler is None:
        print("BLOCKED: g++ is required for portable rule tests.", file=sys.stderr)
        return 127
    with tempfile.TemporaryDirectory(prefix="alpha-rules-") as temporary:
        executable = Path(temporary) / "alpha_rules_tests"
        command = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I", str(ROOT / "Source" / "AlphaGame"),
            str(ROOT / "Scripts" / "test_rules.cpp"), "-o", str(executable),
        ]
        if args.sanitize:
            command[1:1] = ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
        print("Compiling portable C++ rule tests...", flush=True)
        compile_result = subprocess.run(command, cwd=ROOT, check=False)
        if compile_result.returncode != 0:
            return compile_result.returncode
        print("Running portable C++ rule tests...", flush=True)
        return subprocess.run([str(executable)], cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
