# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package declaration for the chromium project."""

pkg.declare(
    name = "@chromium-agents",
    lucicfg = "1.46.3",
)

pkg.options.lint_checks([
    "default",
    "-confusing-name",
    "-function-docstring",
    "-function-docstring-args",
    "-function-docstring-return",
    "-module-docstring",
])

pkg.entrypoint("main.star")

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "9bf1157838ed60790863091715e1fd98faa76054",
    ),
)
