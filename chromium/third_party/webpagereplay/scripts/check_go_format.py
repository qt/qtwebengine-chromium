#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys


def check_gofmt():
    """
    Checks for unformatted Go files using `gofmt -l`. If any are found, prints
    them and exits with an error.
    """
    try:
        result = subprocess.run(['gofmt', '-l', '.'],
                                capture_output=True,
                                text=True,
                                check=False)
        unformatted_files = result.stdout.strip().splitlines()
        if unformatted_files:
            print("The following files are not formatted. Run gofmt to fix.")
            for f in unformatted_files:
                print(f"    {f}")
            sys.exit(1)
        else:
            print("All Go files are correctly formatted.")
            sys.exit(0)
    except FileNotFoundError:
        print("Error: The 'gofmt' command was not found. Please ensure Go is "
              "installed and in your system's PATH.")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        sys.exit(1)


if __name__ == "__main__":
    check_gofmt()
