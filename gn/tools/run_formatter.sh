#!/bin/bash -eu

cd $(dirname $(dirname $0))

if [ "${1:-}" = "--diff" ]; then
  opts="--dry-run -Werror"
else
  opts="-i"
fi

if [ -z "${CLANG_FORMAT:-}" ]; then
  ensure_file=$(mktemp)
  # https://chrome-infra-packages.appspot.com/p/fuchsia/third_party/clang
  echo 'fuchsia/third_party/clang/${platform} integration' > $ensure_file
  cipd ensure -ensure-file $ensure_file -root clang
  CLANG_FORMAT="./clang/bin/clang-format"
fi

git ls-files | egrep '\.(h|cc)$' | fgrep -v 'third_party' |\
    xargs "$CLANG_FORMAT" $opts
