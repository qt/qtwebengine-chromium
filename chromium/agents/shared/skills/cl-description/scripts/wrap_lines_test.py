#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=line-too-long
"""Tests for wrap_lines.py"""

import textwrap
import unittest
from wrap_lines import wrap_text


class WrapLinesTest(unittest.TestCase):
    """Tests for wrap_text function."""

    def test_formatting(self):
        """Test various CL description formatting draft & final cases."""
        test_cases = [
            {
                "name": "empty_text",
                "draft": "   ",
                "final": "",
            },
            {
                "name": "single_line_subject",
                "draft": "[Component] Simple subject line",
                "final": "[Component] Simple subject line",
            },
            {
                "name":
                "short_subject_and_body",
                "draft":
                """
                    [Component] Subject
                    Short body line.
                """,
                "final":
                """
                    [Component] Subject

                    Short body line.
                """,
            },
            {
                "name":
                "long_body_lines_wrapping",
                "width":
                72,
                "draft":
                """
                    [Component] Subject
                    This is a very long line that should be wrapped to seventy-two characters by the script because it exceeds the length limit.
                """,
                "final":
                """
                    [Component] Subject

                    This is a very long line that should be wrapped to seventy-two
                    characters by the script because it exceeds the length limit.
                """,
            },
            {
                "name":
                "multiple_paragraphs",
                "draft":
                """
                    [Comp] Subject
                    Paragraph one with some text.

                    Paragraph two with some more text.
                """,
                "final":
                """
                    [Comp] Subject

                    Paragraph one with some text.

                    Paragraph two with some more text.
                """,
            },
            {
                "name":
                "subject_paragraph_and_bullets",
                "width":
                72,
                "draft":
                """
                    [Comp] Subject
                    This is an introductory paragraph explaining the overall change.

                    - First bullet point with details that are quite long and need to wrap properly.
                    - Second bullet point
                """,
                "final":
                """
                    [Comp] Subject

                    This is an introductory paragraph explaining the overall change.

                    - First bullet point with details that are quite long and need to wrap
                      properly.
                    - Second bullet point
                """,
            },
            {
                "name":
                "bullet_points_wrapping",
                "width":
                72,
                "draft":
                """
                    [Comp] Subject
                    - First bullet point that is quite long and exceeds the seventy-two character limit so it needs to wrap properly.
                    - Second short bullet.
                """,
                "final":
                """
                    [Comp] Subject

                    - First bullet point that is quite long and exceeds the seventy-two
                      character limit so it needs to wrap properly.
                    - Second short bullet.
                """,
            },
            {
                "name":
                "bullet_custom_indent",
                "width":
                40,
                "draft":
                """
                    [Comp] Subject
                    *   Or other indentation sizes
                         are OK - the formatter should
                         be flexible with whatever the
                         author indented as
                """,
                "final":
                """
                    [Comp] Subject

                    *   Or other indentation sizes are OK -
                        the formatter should be flexible
                        with whatever the author indented as
                """,
            },
            {
                "name":
                "various_bullet_types",
                "width":
                50,
                "draft":
                """
                    [Comp] Subject
                    * Asterisk bullet point that is long enough to require wrapping onto line two.
                    + Plus bullet point that is long enough to require wrapping onto line two.
                    1. Numbered list item that is long enough to require wrapping onto line two.
                    2) Paren numbered item that is long enough to require wrapping onto line two.
                """,
                "final":
                """
                    [Comp] Subject

                    * Asterisk bullet point that is long enough to
                      require wrapping onto line two.
                    + Plus bullet point that is long enough to require
                      wrapping onto line two.
                    1. Numbered list item that is long enough to
                       require wrapping onto line two.
                    2) Paren numbered item that is long enough to
                       require wrapping onto line two.
                """,
            },
            {
                "name":
                "footers_preserved",
                "draft":
                """
                    [Comp] Subject
                    Some description text.

                    Bug: 123456
                    Test: manual verification
                    Change-Id: I123456
                """,
                "final":
                """
                    [Comp] Subject

                    Some description text.

                    Bug: 123456
                    Test: manual verification
                    Change-Id: I123456
                """,
            },
            {
                "name":
                "obsolete_histogram_tag_single_line",
                "draft":
                """
                    [Comp] Subject
                    Some description text explaining the rationale for the changes.

                    OBSOLETE_HISTOGRAM[Tab.VeryLongHistogramName]=Replaced by Tab.AnotherLongHistogramName2 because the original metric is no longer needed.
                    Bug: 123456
                    Test: manual verification
                """,
                "final":
                """
                    [Comp] Subject

                    Some description text explaining the rationale for the changes.

                    OBSOLETE_HISTOGRAM[Tab.VeryLongHistogramName]=Replaced by Tab.AnotherLongHistogramName2 because the original metric is no longer needed.
                    Bug: 123456
                    Test: manual verification
                """,
            },
        ]

        for tc in test_cases:
            with self.subTest(tc["name"]):
                width = tc.get("width", 72)
                draft = textwrap.dedent(tc["draft"]).strip()
                final = textwrap.dedent(tc["final"]).strip()
                self.assertEqual(wrap_text(draft, width=width), final)


if __name__ == '__main__':
    unittest.main()
