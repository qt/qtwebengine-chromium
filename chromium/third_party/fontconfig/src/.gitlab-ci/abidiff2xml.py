#! /usr/bin/env python3
# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND

import argparse
import re
import sys
from enum import Enum
from junit_xml import TestSuite, TestCase


class ResultContext(Enum):
    Unknown = 0
    Summary = 1
    Added = 2
    Removed = 3


def is_error(ctx):
    return ctx != ResultContext.Summary


def output(head, lines, ctx):
    t = TestCase(
        head, "fontconfig", 0, "\n".join(lines), allow_multiple_subelements=True
    )
    if is_error(ctx):
        for line in lines:
            t.add_failure_info(head, line, ctx)

    return [t]


def abidiff2junit(result):
    results = []

    context = ResultContext.Unknown
    head = ""
    stdout = []
    unknown = []
    for line in result.splitlines():
        if m := re.match(r"(.*) changes summary: (.*)", line):
            ctx = ResultContext.Summary
            h = m.group(1) + " changes summary"
            if context != ResultContext.Unknown and (ctx != context or h != head):
                results += output(head, stdout, context)
            context = ctx
            head = m.group(1) + " changes summary"
            stdout = [m.group(2)]
        elif m := re.match(r"(\d+) (Removed|Added)\s(.*):$", line):
            ctx = (
                ResultContext.Added if m.group(2) == "Added" else ResultContext.Removed
            )
            if context != ResultContext.Unknown and ctx != context:
                results += output(head, stdout, context)
            context = ctx
            head = m.group(2) + " " + m.group(3)
            stdout = []
        elif not line.strip():
            continue
        elif context == ResultContext.Added or context == ResultContext.Removed:
            stdout += [line]
        else:
            unknown += [line]
    if context != ResultContext.Unknown:
        results += output(head, stdout, context)
    results += output("Unknown response", unknown, ResultContext.Unknown)
    return TestSuite("The result of abidiff", results)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-o",
        "--output",
        type=argparse.FileType("w"),
        default=sys.stdout,
        help="Outout xml file",
    )
    parser.add_argument(
        "RESULT",
        type=argparse.FileType("r"),
        default=sys.stdin,
        help="Result from abidiff",
    )
    args = parser.parse_args(args=sys.argv[1:])
    res = abidiff2junit(args.RESULT.read())
    args.output.write(TestSuite.to_xml_string([res]))
    args.output.close()
