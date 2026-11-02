#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wraps text to 72 characters per line for CL descriptions."""
import fileinput
import re
import textwrap


def wrap_text(text, width=72):
    if not text.strip():
        return ""

    # Separate subject and body
    parts = text.split('\n', 1)
    subject = parts[0].strip()
    body = parts[1].strip() if len(parts) > 1 else ""

    if not body:
        return subject

    wrapped_blocks = []
    paragraphs = re.split(r'\n\n+', body)
    footer_re = r'^\w+[A-Za-z0-9_\-]*(?:\[[^\]]+\])?[:=]'
    for p in paragraphs:
        if not p.strip():
            continue

        items = []
        for line in p.splitlines():
            # Check if line starts a bullet point
            is_bullet = bool(re.match(r'^\s*(?:[-*+]|\d+[.)])\s+', line))
            # Check if line starts a footer or tag
            is_footer = bool(re.match(footer_re, line))

            if not items or is_bullet or is_footer:
                items.append([line])
            else:
                items[-1].append(line)

        wrapped_items = []
        for item_lines in items:
            item_str = '\n'.join(item_lines)
            bullet_match = re.match(r'^(\s*(?:[-*+]|\d+[.)])\s+)(.*)',
                                    item_str, re.DOTALL)
            if bullet_match:
                marker = bullet_match.group(1)
                content = bullet_match.group(2)
                subsequent_indent = ' ' * len(marker)

                cleaned_content = ' '.join(content.split())
                wrapper = textwrap.TextWrapper(
                    width=width,
                    initial_indent=marker,
                    subsequent_indent=subsequent_indent,
                    break_long_words=False,
                    break_on_hyphens=False)
                wrapped_items.append(wrapper.fill(cleaned_content))
            elif re.match(footer_re, item_str):
                # Footer tags must remain on a single line.
                wrapped_items.append(' '.join(item_str.split()))
            else:
                cleaned_content = ' '.join(item_str.split())
                wrapper = textwrap.TextWrapper(width=width,
                                               break_long_words=False,
                                               break_on_hyphens=False)
                wrapped_items.append(wrapper.fill(cleaned_content))

        wrapped_blocks.append('\n'.join(wrapped_items))

    formatted_body = '\n\n'.join(wrapped_blocks)
    return f"{subject}\n\n{formatted_body}"


if __name__ == "__main__":
    input_text = "".join(fileinput.input())
    print(wrap_text(input_text))
