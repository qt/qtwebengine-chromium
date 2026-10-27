# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions for Go script wrappers."""

from __future__ import annotations

import pathlib
import platform

_REPO_DIR = pathlib.Path(__file__).resolve().parents[1]


def _get_bin_path() -> pathlib.Path:
    machine = platform.machine().lower()
    if machine in ("arm64", "aarch64"):
        arch = "arm64"
    elif machine.startswith("arm"):
        arch = "arm"
    elif machine in ("x86_64", "amd64", "x64"):
        arch = "x64"
    else:
        arch = "x86"

    system = platform.system().lower()
    if system == "darwin":
        os_name = "mac"
    elif system == "windows":
        os_name = "win"
    else:
        os_name = "linux"

    return _REPO_DIR / "third_party" / "golang" / os_name / arch / "bin"


def _get_exec_extension() -> str:
    return ".exe" if platform.system().lower() == "windows" else ""


def get_go_compiler_path() -> pathlib.Path:
    path = _get_bin_path() / f"go{_get_exec_extension()}"
    if not path.exists():
        raise FileNotFoundError(
            f"Go compiler not found at {path}. Did you run `gclient sync`?")
    return path


def get_gofmt_path() -> pathlib.Path:
    path = _get_bin_path() / f"gofmt{_get_exec_extension()}"
    if not path.exists():
        raise FileNotFoundError(
            f"Go formatter not found at {path}. Did you run `gclient sync`?")
    return path
