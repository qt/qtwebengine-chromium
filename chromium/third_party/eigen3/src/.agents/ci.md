# Formatting And CI

Use the checked-out configuration as the source of truth. [`.gitlab-ci.yml`](../.gitlab-ci.yml) defines stages and
includes; [`ci/*.gitlab-ci.yml`](../ci) and [`ci/scripts/`](../ci/scripts) define the actual jobs. Default MR pipelines
run a limited smoke matrix; labels such as `all-tests` and `gpu-tests`, plus scheduled or manually started pipelines,
enable broader jobs. A green default MR pipeline is not proof that every supported configuration was exercised.

Build jobs publish the configured build directory as an artifact. Their paired test jobs consume that artifact and
run CTest without rebuilding. When changing either side, keep the test job's `needs`, CTest label or filter, and the
corresponding build target consistent; otherwise CTest can discover tests whose executables are absent.

## Worktree-Safe Formatting

Inspect `git status --short` before formatting and preserve unrelated changes. Eigen requires `clang-format-17`
exactly. Format only files owned by the task:

```bash
clang-format-17 -i path/to/file.cpp path/to/header.h
clang-format-17 --dry-run --Werror path/to/file.cpp path/to/header.h
git clang-format --binary clang-format-17 --diff <base-sha>
```

`.clang-format` intentionally disables include sorting and registers Eigen-specific macros and attributes. Do not
reorder includes or restyle those macros manually.

[`scripts/format.sh`](../scripts/format.sh) rewrites every matching file in the tree in parallel. Run it only when the
worktree is clean or every affected change is owned by the task. Review `git diff` afterward in either case.

## Local Checks

Run checks relevant to the changed files and report unavailable tools:

```bash
codespell --config setup.cfg path/to/changed-file
reuse lint
```

The whole-tree codespell invocation used by CI can expose pre-existing findings. Do not modify unrelated files merely
to make a local broad scan clean. In the current CI configuration, clang-format, codespell, and clang-tidy jobs are
`allow_failure`; treat their diagnostics as review findings anyway. The REUSE job is blocking.

Source-like files normally carry an inline SPDX copyright and license header using the file type's comment syntax.
Files that should not carry inline comments need coverage in [`REUSE.toml`](../REUSE.toml). To process selected new
source files with the repository helper, pass them explicitly because its default scan considers tracked files:

```bash
python3 scripts/add_spdx_headers.py --paths path/to/new-file.cpp
```

## Clang-Tidy

Use the CI driver rather than invoking clang-tidy directly on an implementation header; the driver routes such a
header through its public umbrella include.

```bash
cmake -G Ninja -S . -B .tidy-build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DEIGEN_BUILD_TESTING=ON
ci/scripts/run-clang-tidy.sh <base-sha> .tidy-build
```

The driver examines files committed between `<base-sha>` and `HEAD`; uncommitted-only edits are not included. Eigen's
`.clang-tidy` policy is authoritative. Do not apply generic `modernize-*` or `cppcoreguidelines-*` campaigns.

## Before Review

1. Inspect `git diff` and `git diff --check`.
2. Format the exact changed source files with clang-format-17.
3. Run the focused builds and tests documented in [`testing.md`](testing.md).
4. Run applicable spelling, REUSE, and clang-tidy checks.
5. State what ran, what did not run, and why. Do not claim coverage from jobs or hardware that were unavailable.
