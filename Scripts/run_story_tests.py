#!/usr/bin/env python3
"""Compile and execute the shared story rules.

Temporary outputs, strict C++17 compilation and optional UBSan.
This verifies calculations and decisions, not Unreal input or replication.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sanitize", action="store_true",
        help="Compile with UndefinedBehaviorSanitizer and stop on its first error",
    )
    args = parser.parse_args()
    compiler = shutil.which("g++")
    if compiler is None:
        print("BLOCKED: g++ is required for story tests.", file=sys.stderr)
        return 127
    with tempfile.TemporaryDirectory(prefix="alpha-story-") as temporary:
        executable = Path(temporary) / "alpha_story_tests"
        command = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-I", str(ROOT / "Source" / "AlphaGame"),
            str(ROOT / "Scripts" / "test_story_rules.cpp"), "-o", str(executable),
        ]
        if args.sanitize:
            command[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=undefined"]
        print("Compiling portable C++ story tests" + (" with UBSan..." if args.sanitize else "..."), flush=True)
        compiled = subprocess.run(command, cwd=ROOT, check=False)
        if compiled.returncode != 0:
            return compiled.returncode
        print("Running portable C++ story tests...", flush=True)
        return subprocess.run([str(executable)], cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
