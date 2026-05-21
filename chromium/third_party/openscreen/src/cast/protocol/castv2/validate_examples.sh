#!/usr/bin/env bash

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DEFAULT_YAJSV_PATH=~/go/bin/yajsv

YAJSV_BIN=$(which yajsv)
if [ "$YAJSV_BIN" == "" ]; then
  if [[ -f "$DEFAULT_YAJSV_PATH" ]]; then
      echo "Found yajsv at the default path ($DEFAULT_YAJSV_PATH), using."
      YAJSV_BIN=$DEFAULT_YAJSV_PATH
  else
      echo "Could not find yajsv, see //cast/protocol/castv2/README.md"
      exit 1
  fi
fi

for filename in $SCRIPT_DIR/streaming_examples/*.json; do
  "$YAJSV_BIN" -s "$SCRIPT_DIR/streaming_schema.json" "$filename"
done

for filename in $SCRIPT_DIR/receiver_examples/*.json; do
  "$YAJSV_BIN" -s "$SCRIPT_DIR/receiver_schema.json" "$filename"
done
