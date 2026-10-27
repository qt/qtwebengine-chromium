#!/usr/bin/env python3
#
# Check that the EI_EVENT_FOO and EIS_EVENT_FOO enum values match (for those events that
# exist in both libraries).

from typing import Iterator
from pathlib import Path
import re
import subprocess
import tempfile

template = """
#include <libei.h>
#include <libeis.h>

@@@

 int main(void) {
         return 0;
}
"""
assert_template = '_Static_assert(EIS_EVENT_{event} == EI_EVENT_{event}, "Mismatching event types for {event}");'


def extract(header: Path, prefix: str) -> Iterator[str]:
    with open(header) as fd:
        for line in fd:
            match = re.match(rf"^\t{prefix}(\w+)", line)
            if match:
                yield match[1]


if __name__ == "__main__":
    ei_events = extract(Path("src/libei.h"), prefix="EI_EVENT_")
    eis_events = extract(Path("src/libeis.h"), prefix="EIS_EVENT_")

    common = set(ei_events) & set(eis_events)

    print("Shared events that need identical values:")
    for e in common:
        print(f"  EI_EVENT_{e}")

    asserts = (assert_template.format(event=e) for e in common)

    with tempfile.NamedTemporaryFile(suffix=".c", delete=False) as fd:
        fd.write(template.replace("@@@", "\n".join(asserts)).encode("utf-8"))
        fd.flush()
        pkgconfig = subprocess.run(
            ["pkg-config", "--cflags", "--libs", "libei-1.0", "libeis-1.0"],
            check=True,
            capture_output=True,
        )
        pkgconfig_args = pkgconfig.stdout.decode("utf-8").strip().split(" ")
        try:
            subprocess.run(
                ["gcc", "-o", "event-type-check", *pkgconfig_args, fd.name],
                check=True,
                capture_output=True,
            )
            print("Success. Event types are identical")
        except subprocess.CalledProcessError as e:
            print("Mismatching event types")
            print(e.stdout.decode("utf-8"))
            print(e.stderr.decode("utf-8"))
            raise SystemExit(1)
