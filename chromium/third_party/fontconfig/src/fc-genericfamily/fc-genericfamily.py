#!/usr/bin/env python3
# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

import sys
import os
import argparse
from pathlib import Path


# Map from input files to FC_FAMILY_* bit positions
# Each family can have multiple classifications, represented as bit fields
FAMILY_FILE_MAP = {
    "serif.txt": "FC_FAMILY_SERIF",
    "sans-serif.txt": "FC_FAMILY_SANS",
    "monospace.txt": "FC_FAMILY_MONO",
    "cursive.txt": "FC_FAMILY_CURSIVE",
    "fantasy.txt": "FC_FAMILY_FANTASY",
    "system-ui.txt": "FC_FAMILY_SYSTEM_UI",
    "ui-serif.txt": "FC_FAMILY_UI_SERIF",
    "ui-sans-serif.txt": "FC_FAMILY_UI_SANS",
    "ui-monospace.txt": "FC_FAMILY_UI_MONO",
    "ui-rounded.txt": "FC_FAMILY_UI_ROUNDED",
    "emoji.txt": "FC_FAMILY_EMOJI",
    "math.txt": "FC_FAMILY_MATH",
    "fangsong.txt": "FC_FAMILY_FANGSONG",
}

# FC_FAMILY_* enumeration values (as bit positions)
FAMILY_ENUM_BITS = {
    "FC_FAMILY_SERIF": 0,
    "FC_FAMILY_SANS": 1,
    "FC_FAMILY_MONO": 2,
    "FC_FAMILY_CURSIVE": 3,
    "FC_FAMILY_FANTASY": 4,
    "FC_FAMILY_SYSTEM_UI": 5,
    "FC_FAMILY_UI_SERIF": 6,
    "FC_FAMILY_UI_SANS": 7,
    "FC_FAMILY_UI_MONO": 8,
    "FC_FAMILY_UI_ROUNDED": 9,
    "FC_FAMILY_EMOJI": 10,
    "FC_FAMILY_MATH": 11,
    "FC_FAMILY_FANGSONG": 12,
}


def gen_header():
    return """/* Copyright (C) 2026 fontconfig Authors */
/* SPDX-License-Identifier: HPND */
/* This file is auto-generated. Do not edit. */

"""


def read_family_file(filepath: Path):
    """Read family names from a file, one per line, ignoring comments and empty lines."""
    families = []
    if filepath.exists():
        for line in filepath.read_text(encoding="utf-8").splitlines():
            # Remove inline comments
            if "#" in line:
                line = line.split("#", 1)[0]

            line = line.strip().rstrip()

            # Skip empty lines
            if line:
                families.append(line.lower())

    return families


def collect_family_data(base_dir: Path = Path(".")):
    """
    Collect family names from all configured files and build a mapping
    from family name to classification bit field.
    """
    family_map = {}

    for filename, family_enum in FAMILY_FILE_MAP.items():
        filepath = base_dir / filename
        families = read_family_file(filepath)

        if not families:
            continue

        bit_pos = FAMILY_ENUM_BITS.get(family_enum, 0)
        bit_value = 1 << bit_pos

        for family in families:
            if family in family_map:
                # Family can belong to multiple categories (OR the bit fields)
                family_map[family] |= bit_value
            else:
                family_map[family] = bit_value

    return family_map


def gen_gperf_code(family_map):
    """Generate gperf input code for family name classification lookup."""
    lines = []

    # gperf header
    lines.append("%{")
    lines.append("#include <string.h>")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("struct FcGenericFamilyEntry {")
    lines.append("    int      name;")
    lines.append("    uint32_t classification;  /* Bit field of FC_FAMILY_* values */")
    lines.append("};")
    lines.append("%}")
    lines.append("")

    # gperf directives
    lines.append("%struct-type")
    lines.append("%ignore-case")
    lines.append("%language=ANSI-C")
    lines.append("%define slot-name name")
    lines.append("%define lookup-function-name fc_generic_family_lookup")
    lines.append("%define hash-function-name fc_generic_family_hash")
    lines.append("%define string-pool-name FcConstFamilyNamePool")
    lines.append("%readonly-tables")
    lines.append("%pic")
    lines.append("%includes")
    lines.append("")
    lines.append("struct FcGenericFamilyEntry;")
    lines.append("%%")

    # Sort family names for consistent output
    for family in sorted(family_map.keys()):
        classification = family_map[family]
        # Escape special characters in family name if needed
        escaped_family = family.replace('"', '\\"')
        lines.append(f'"{escaped_family}", 0x{classification:08x}')

    lines.append("%%")
    lines.append("")

    return "\n".join(lines)


def main():
    # Ensure stdout uses UTF-8 encoding
    if sys.stdout.encoding != 'utf-8':
        sys.stdout.reconfigure(encoding='utf-8')

    parser = argparse.ArgumentParser(
        description="Generate gperf code for generic font family classification",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-o",
        "--output",
        type=argparse.FileType("w", encoding="utf-8"),
        default=sys.stdout,
        help="Output file (default: stdout)",
    )
    parser.add_argument(
        "-d",
        "--directory",
        type=Path,
        default=".",
        help="Base directory containing family name files",
    )

    args = parser.parse_args()

    # Collect family data from files
    family_map = collect_family_data(args.directory)

    if not family_map:
        print(
            "Error: No family data found. Make sure family list files exist.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Generate output
    with args.output:
        args.output.write(gen_header())
        args.output.write(gen_gperf_code(family_map))

    return 0


if __name__ == "__main__":
    sys.exit(main())
