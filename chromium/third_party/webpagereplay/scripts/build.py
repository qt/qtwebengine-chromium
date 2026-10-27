#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to build WebPageReplay Go binaries with the bundled go toolchain."""

from __future__ import annotations

import argparse
import logging
import os
import pathlib
import subprocess
import sys

import go_utils

_REPO_DIR = pathlib.Path(__file__).resolve().parents[1]
_SRC_DIR = _REPO_DIR / "src"


def _compute_go_arch(os_arch):
    # As given by the output of `go tool dist list`
    if os_arch == "x64":
        return "amd64"
    if os_arch == "x86":
        return "386"
    if os_arch == "arm64":
        return "arm64"
    if os_arch == "arm32":
        return "arm"
    raise ValueError(f"Invalid architecture {os_arch}")


def _compute_go_os(os_name):
    # As given by the output of `go tool dist list`
    if os_name == "win":
        return "windows"
    if os_name == "macos":
        return "darwin"
    if os_name in ("linux", "android", "chromeos"):
        # It's not possible to build with CGO_ENABLED=0 and GOOS="android".
        # Linux is good enough.
        return "linux"
    raise ValueError(f"Invalid OS {os_name}")


def _run(cmd, env=None, stdout=None, stderr=None):
    if env is None:
        env = {}
    env_str = " ".join([f"{k}={v}" for k, v in env.items()])
    cmd_str = " ".join(cmd)
    logging.info(f"{env_str} {cmd_str}")
    subprocess.check_call(cmd,
                          env=os.environ.copy() | env,
                          stdout=stdout,
                          stderr=stderr)


def build(os_name=None, arch=None, out_dir=None, binary=None):
    if out_dir is None:
        out_dir = "."
    if binary is None:
        binary = "wpr"
    out_dir = pathlib.Path(out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    env = {
        "CGO_ENABLED": "0",
    }
    if os_name is not None:
        env["GOOS"] = _compute_go_os(os_name)
    if arch is not None:
        env["GOARCH"] = _compute_go_arch(arch)
    _run(
        [
            str(go_utils.get_go_compiler_path()),
            "build",
            "-C",
            str(_SRC_DIR),
            # -trimpath and -buildvcs achieve deterministic builds.
            "-trimpath",
            "-buildvcs=false",
            "-o",
            str(out_dir / binary),
            f"{binary}.go",
        ],
        env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--os",
        help="Target OS (win, mac, linux, chromeos or android). Defaults to "
        "the host OS.")
    parser.add_argument(
        "--arch",
        help="Target arch (x64, x86, arm64 or arm32). Defaults to the host "
        "arch.")
    parser.add_argument("--out-dir",
                        help="Output directory for the binary. Defaults to "
                        "the current directory")
    parser.add_argument("--binary",
                        help="Binary to build: wpr (default) or httparchive.")
    parser.add_argument("--verbose",
                        action="store_true",
                        help="Enable verbose logging")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO if args.verbose else logging.ERROR)
    build(args.os, args.arch, args.out_dir, args.binary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
