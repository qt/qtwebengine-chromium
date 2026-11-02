#! /usr/bin/env python3
# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND

import argparse
import re
import subprocess
import sys
from junit_xml import TestSuite, TestCase


def pickup(fname):
    with open(fname, "r", encoding="utf-8") as f:
        rettype = None
        for l in f.readlines():
            l = l.rstrip()
            m = re.match(r"^([\w][\w \*].*[^;,{}]$)", l)
            if m and m.group(1):
                rettype = m.group(1)
                continue
            m = re.match(r"^(Fc[^ ]*)[\s\w]*\(.*", l)
            if m and m.group(1) not in ["FcCacheDir", "FcCacheSubdir"]:
                if not rettype:
                    raise RuntimeError(f"No return type detected for {m.group(1)}")
                yield (rettype, m.group(1))
            rettype = None


if __name__ == "__main__":
    results = []
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-i",
        "--ignore-implicit-visibility",
        action="store_true",
        help="Ignore implicit symbol visibility",
    )
    parser.add_argument(
        "-f",
        "--ignore-symbol-file",
        type=open,
        help="A file that contains symbol list to ignore implicit visibility",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=argparse.FileType("w"),
        default=sys.stdout,
        help="Output xml file",
    )
    parser.add_argument("library", help="A path to library")
    parser.add_argument("headers", nargs="+", help="A path to header file")

    args = parser.parse_args()
    symbollist = []

    if args.ignore_symbol_file:
        symbollist = [line.rstrip() for line in args.ignore_symbol_file.readlines()]
        symbollist = [item for item in symbollist if not re.match(r"^#.*", item)]

    knownapi = {}
    for fname in args.headers:
        ts = TestCase(
            f"Checking header files {fname}",
            "fontconfig",
            0,
            open(fname).read(),
            allow_multiple_subelements=True,
        )
        for rettype, func in pickup(fname):
            if not re.search(r"FcPrivate|FcPublic", rettype):
                if not args.ignore_implicit_visibility and func not in symbollist:
                    ts.add_failure_info(
                        f"{func} does not have FcPublic nor FcPrivate in {fname}",
                        "",
                        "Missing",
                    )

            knownapi[func] = rettype
        results += [ts]

    res = subprocess.run(
        ["nm", args.library], capture_output=True, text=True, check=True
    )
    if res.returncode != 0:
        raise RuntimeError(f"Unable to decode {args.library}: {res.stderr}")
    ts = TestCase(
        "Checking symbols", "fontconfig", 0, res.stdout, allow_multiple_subelements=True
    )
    for line in res.stdout.splitlines():
        m = re.search(r"\sT\s(.*)$", line)
        if m and m.group(1):
            if m.group(1) not in knownapi and m.group(1) not in symbollist:
                ts.add_failure_info(
                    f"{m.group(1)} does not have FcPublic nor FcPrivate", "", "Missing"
                )
    results += [ts]
    args.output.write(TestSuite.to_xml_string([TestSuite("API checking", results)]))
    args.output.close()
