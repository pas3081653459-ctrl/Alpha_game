#!/usr/bin/env python3
"""Compile and execute the presentation calculations used by UE visual code.

Uses temporary outputs and preserves compilation/test failure exit codes.
This does not run Unreal or validate rendering.
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
        print("BLOCKED: g++ is required for portable presentation tests.", file=sys.stderr)
        return 127
    with tempfile.TemporaryDirectory(prefix="alpha-presentation-") as temporary:
        executable = Path(temporary) / "alpha_presentation_tests"
        command = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I", str(ROOT / "Source" / "AlphaGame"),
            str(ROOT / "Scripts" / "test_presentation.cpp"), "-o", str(executable),
        ]
        if args.sanitize:
            command[1:1] = ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
        print("Compiling portable C++ presentation tests" + (" with UBSan..." if args.sanitize else "..."), flush=True)
        compile_result = subprocess.run(command, cwd=ROOT, check=False)
        if compile_result.returncode != 0:
            return compile_result.returncode
        print("Running portable C++ presentation tests...", flush=True)
        return subprocess.run([str(executable)], cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
