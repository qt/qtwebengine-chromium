#!/bin/bash -eu

check=false
if [[ "$#" -ge 1 && "$1" == "--diff" ]]; then
    check=true
fi

# Ensure we're always running in the correct directory.
cd "$(dirname $(dirname "${BASH_SOURCE[0]}"))"

# If NOBUILD is set, we skip building GN. This is useful for
# CI, where we've just built it, and ninja is not present in the path.
if [[ -z "${NOBUILD:-}" ]]; then
    echo Building gn...
    ninja -C out gn
fi
echo Generating new docs/reference.md...
content=$(out/gn help --markdown all)

if "${check}"; then
    diff -u docs/reference.md <(echo "$content")
else
    echo "$content" > docs/reference.md
fi
