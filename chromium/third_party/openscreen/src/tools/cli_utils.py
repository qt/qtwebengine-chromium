#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Terminal styling and output utilities for CLI scripts."""

import sys


class TerminalStyle:
    """ANSI escape codes for terminal styling."""
    BOLD = '\033[1m'
    GREEN = '\033[92m'
    RED = '\033[91m'
    BLUE = '\033[94m'
    RESET = '\033[0m'


def _supports_color(stream) -> bool:
    """Checks if the output stream supports ANSI color codes."""
    return hasattr(stream, 'isatty') and stream.isatty()


def print_step(step: str, message: str) -> None:
    """Prints a major step header, bolded and colored if supported."""
    if _supports_color(sys.stdout):
        print(f'\n{TerminalStyle.BLUE}{TerminalStyle.BOLD}Step {step}: '
              f'{message}{TerminalStyle.RESET}')
    else:
        print(f'\nStep {step}: {message}')


def print_substep(message: str) -> None:
    """Prints a sub-step or detail message."""
    print(f'  -> {message}')


def print_error(message: str) -> None:
    """Prints an error message to stderr in red if supported."""
    if _supports_color(sys.stderr):
        print(
            f'{TerminalStyle.RED}{TerminalStyle.BOLD}ERROR:'
            f'{TerminalStyle.RESET} {message}',
            file=sys.stderr)
    else:
        print(f'ERROR: {message}', file=sys.stderr)


def print_success(message: str) -> None:
    """Prints a success message in green if supported."""
    if _supports_color(sys.stdout):
        print(f'\n{TerminalStyle.GREEN}{TerminalStyle.BOLD}{message}'
              f'{TerminalStyle.RESET}\n')
    else:
        print(f'\n{message}\n')
