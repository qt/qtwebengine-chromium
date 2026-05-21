#!/usr/bin/env sh
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script finds all .dot files in the current directory and generates
# PNG images from them using the Graphviz 'dot' tool. Graphviz is generally
# considered in Chromium to be the most commonly used diagram generation tool.
# Other tools, such as Mermaid, could be added here for things like sequence
# diagram creation (which is an absolutely dreadful experience with Graphviz).
#
# Unfortunately, Chromium's (and thus Open Screen's) Gitiles renderer does not
# currently handle rendering markdown-based diagrams natively in documentation.
# See crbug.com/383566360. If this actually changes later, our documentation
# should likely take advantage of it.

set -e

DOCS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
OUTPUT_DIR="${DOCS_DIR}/images"

echo "Generating diagrams..."
echo "Source directory: ${DOCS_DIR}"
echo "Output directory: ${OUTPUT_DIR}"

# Create the output directory if it doesn't exist.
if [ ! -d "${OUTPUT_DIR}" ]; then
    echo "Creating output directory: ${OUTPUT_DIR}"
    mkdir -p "${OUTPUT_DIR}"
fi

# Check if Graphviz is installed.
if ! command -v dot &> /dev/null
then
    echo "Graphviz 'dot' command could not be found. Please install Graphviz."
    exit 1
fi

# Find all .dot files and generate PNGs.
for dot_file in "${DOCS_DIR}"/*.dot; do
  # Check if the file exists to avoid errors when no .dot files are found.
  if [ -f "$dot_file" ]; then
    base_name=$(basename "$dot_file" .dot)
    output_file="${OUTPUT_DIR}/${base_name}.png"
    echo " • Processing ${dot_file} -> ${output_file}"
    dot -Tpng -Gdpi=300 -o "$output_file" "$dot_file"
  fi
done

echo "Diagram generation complete."
