#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""compiler_wrapper.py - Skill script to easily compile schemas."""

import argparse
from pathlib import Path
import subprocess
import sys


def find_src_root(start_dir):
    """Walk up parent directories to find the Chromium src root."""
    curr = Path(start_dir).resolve()
    for parent in [curr] + list(curr.parents):
        compiler_path = (
            parent / "tools" / "json_schema_compiler" / "compiler.py"
        )
        if compiler_path.exists():
            return parent
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Helper script to compile Chromium Extension API schemas.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-f",
        "--file",
        required=True,
        help="Path to the JSON or IDL schema file.",
    )
    parser.add_argument(
        "-g",
        "--generator",
        default="cpp",
        choices=[
            "cpp",
            "cpp-bundle-registration",
            "cpp-bundle-schema",
            "externs",
            "ts_definitions",
            "interface",
        ],
        help="The generator type to run.",
    )
    parser.add_argument(
        "-d",
        "--destdir",
        help="The destination directory to output the generated files.",
    )
    parser.add_argument(
        "-n",
        "--namespace",
        default="extensions",
        help="The C++ namespace for generated files.",
    )
    parser.add_argument(
        "-r",
        "--root",
        help="Manual override for the Chromium src root directory.",
    )

    args = parser.parse_args()

    # Find the target file absolute path.
    target_file = Path(args.file).resolve()
    if not target_file.exists():
        print(f"Error: Target file not found: {args.file}", file=sys.stderr)
        return 1

    # Find the src root.
    if args.root:
        src_root = Path(args.root).resolve()
    else:
        src_root = find_src_root(target_file.parent)
        if not src_root:
            src_root = find_src_root(Path.cwd())

    if not src_root:
        print(
            "Error: Could not find Chromium src root "
            "(tools/json_schema_compiler/compiler.py not found in parent "
            "directories).",
            file=sys.stderr,
        )
        return 1

    compiler_path = src_root / "tools" / "json_schema_compiler" / "compiler.py"
    if not compiler_path.exists():
        print(
            f"Error: Compiler script not found at {compiler_path}",
            file=sys.stderr,
        )
        return 1

    # Get file path relative to src root.
    try:
        relative_target = target_file.relative_to(src_root)
    except ValueError:
        relative_target = target_file

    cmd = [
        "vpython3",
        str(compiler_path),
        "--root",
        str(src_root),
        "--namespace",
        args.namespace,
        "--generator",
        args.generator,
    ]

    if args.destdir:
        dest_dir = Path(args.destdir).resolve()
        cmd.extend(["--destdir", str(dest_dir)])

    cmd.append(str(relative_target))

    print(f"Running: {' '.join(cmd)}")
    try:
        # Run compiler.py. Allow output to stdout/stderr in real time.
        result = subprocess.run(
            cmd,
            cwd=src_root,
            check=False,
        )
        return result.returncode
    except Exception as e:
        print(f"Error executing compiler: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
