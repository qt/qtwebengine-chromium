# AGENTS.md

Guidance for AI coding agents working in Eigen. Human contributors should start with
[`README.md`](README.md) and the project documentation it links to. Per-tool files such as `CLAUDE.md` should import
this file and contain only tool-specific additions.

## Scope and precedence

Follow the user's task, then the nearest applicable `AGENTS.md`, then repository documentation and established local
patterns. The checked-out source, tests, CMake files, and CI configuration are authoritative for current mechanics. If
this guide disagrees with the tree, follow the tree, report the discrepancy, and update the guidance when that is in
scope.

Read this file for every task. Then read every row below that matches the work; do not load unrelated guides by
default.

| Work area | Additional guidance |
|---|---|
| Tests and CMake test targets | [`.agents/testing.md`](.agents/testing.md) |
| Numerical kernels, decompositions, solvers, accuracy | [`.agents/numerics.md`](.agents/numerics.md) |
| Performance changes and benchmarks | [`.agents/benchmarking.md`](.agents/benchmarking.md) |
| Packet math, CUDA, HIP, SYCL, `unsupported/Eigen/GPU` | [`.agents/simd-gpu.md`](.agents/simd-gpu.md) |
| Tensor, ThreadPool, and multithreading | [`.agents/tensor-threadpool.md`](.agents/tensor-threadpool.md) |
| Formatting, lint, and GitLab CI | [`.agents/ci.md`](.agents/ci.md) |
| Expression templates or evaluator internals | [`doc/TopicLazyEvaluation.dox`](doc/TopicLazyEvaluation.dox), [`doc/NewExpressionType.dox`](doc/NewExpressionType.dox), and [`doc/ClassHierarchy.dox`](doc/ClassHierarchy.dox) |

## Non-negotiable rules

1. **Preserve existing work.** Start with `git status --short`. Never discard, overwrite, reformat, or stage unrelated
   user changes. Do not use destructive Git commands unless the user explicitly requests that operation. Stage named
   paths, never `git add .` or `git add -A`.
2. **Keep provenance clean.** Code must be original or derived from source material whose license is compatible with
   Eigen's MPL-2.0 distribution. Do not copy, paraphrase, or translate code from proprietary, NDA-covered, internal, or
   incompatibly licensed sources. Published papers, standards, textbooks, and algorithm descriptions may inform an
   independent implementation; cite them inline when they materially inform it. A citation does not make copied code
   permissible. Never invent an attribution for AI-generated code.
3. **Respect the header-only and C++14 contracts.** Supported headers must compile as C++14 unless a guarded backend has
   a documented newer requirement. User code, examples, and public-behavior tests include umbrella headers such as
   `Eigen/Core` or `Eigen/SVD`, not files below `Eigen/src/` or `unsupported/Eigen/src/`. Focused tests of private
   utilities may follow an established direct-include pattern, but those paths remain private even where a header is not
   mechanically guarded. Definitions in public headers must have valid header linkage and avoid ODR violations.
4. **Protect compatibility.** Treat supported public names, signatures, header paths, semantics, and ABI-affecting
   configuration as compatibility surfaces. Prefer additive changes and deprecation over removal. When moving private
   implementation headers, update the public umbrella and remove the old private file rather than adding a private-path
   forwarding shim. ABI-affecting Eigen macros must be consistent across translation units.
5. **Preserve Eigen annotations and style.** Do not drop `EIGEN_DEVICE_FUNC` from coefficient-level or device-callable
   functions. Do not replace `EIGEN_STRONG_INLINE` with `inline`, reorder includes, normalize Eigen macro layout, or
   apply broad `modernize-*` or `cppcoreguidelines-*` rewrites. The repository's conventions and `.clang-format` take
   precedence over generic C++ advice.
6. **Ship verification with behavior.** New functionality includes focused tests. Bug fixes include a regression test
   that fails without the fix when practical. Performance-sensitive changes include an appropriate benchmark. Scale
   broader coverage to the affected scalar types, storage orders, backends, and public contracts.
7. **Treat external writes as deliberate actions.** Unless the user already asked for them, pause after the local commit
   before pushing, opening or updating a merge request, commenting on an issue, or making another external-system write.

## Standard workflow

1. Inspect `git status --short`, the current branch, and the diff. Separate pre-existing work from the requested change.
2. As applicable, read the public header, implementation, nearby tests, registration in `CMakeLists.txt`, and relevant
   task guides before deciding on an implementation. Search with `rg` or `rg --files`.
3. Keep the patch within the owning module and established patterns. Avoid opportunistic refactors and generated or
   metadata churn.
4. Add or update applicable tests and benchmarks in the same patch. Test public behavior through its umbrella header so
   missing exports are caught; follow nearby patterns for focused private-internal tests.
5. Format only files changed by the task with `clang-format-17 -i <files>`. `scripts/format.sh` rewrites matching files
   across the tree; use it only when the worktree is clean and a whole-tree pass is intentional.
6. Build and run the narrowest relevant test first, then widen validation according to the change's risk. Use separate
   build directories for materially different CMake configurations.
7. Review `git diff --check`, `git diff`, and `git status --short`. Report the exact validation run and any unavailable
   compiler, ISA, GPU, dependency, or downstream coverage.

## Repository essentials

Eigen is a header-only expression-template library. Consumers include module headers under `Eigen/` or
`unsupported/Eigen/`. The top-level CMake project builds tests, documentation, demos, and BLAS/LAPACK shims rather than
a core Eigen library; benchmarks use separate CMake projects. `Eigen/Dense` aggregates the dense modules, while
`Eigen/Eigen` includes `Dense` and `Sparse`. External backend support modules and `Eigen/ThreadPool` remain separate
includes. The upstream project is on GitLab; its GitHub repository is a read-only mirror.

The supported implementation is under `Eigen/src/`; tests are under `test/`. Modules with looser API-stability
guarantees are under `unsupported/Eigen/`, with tests under `unsupported/test/`. "Unsupported" does not imply low
impact: Tensor is a foundational TensorFlow dependency. Public umbrella headers are the source of truth for a module's
exported internals.

Every new source file needs accurate REUSE metadata. Original Eigen code normally uses MPL-2.0; prefer the collective
form when an agent cannot truthfully attribute an individual author:

```cpp
// SPDX-FileCopyrightText: The Eigen Authors
// SPDX-License-Identifier: MPL-2.0
```

Use the language's comment syntax. Documentation or assets that should not carry inline tags must be covered precisely
in `REUSE.toml`; do not add a broad annotation that hides unrelated files. Compatible adapted material may require a
different license expression and attribution, which must be preserved rather than relabeled as MPL-2.0.

## Essential Eigen hazards

### Expressions, lifetimes, and aliasing

Eigen expressions are lazy and frequently retain references. Consumption can occur through assignment, construction,
coefficient access, reductions, or `.eval()`.

- `auto x = A + B;` stores a lazy expression whose references may dangle. Materialize with `(A + B).eval()` or use an
  appropriate plain-object type when ownership is required.
- `.noalias()` is a promise, not a runtime check. Use it only when the destination cannot appear in the right-hand side.
  `mat = mat * mat` is protected by product evaluation; `mat.noalias() = mat * mat` is wrong.
- Prefer Eigen expressions when they express the operation clearly and avoid repeated evaluation. Keep a scalar loop
  when it represents control flow better, avoids an unnecessary temporary, or has measured performance benefits.
- Prefer block and view expressions when a uniform operation or existing Eigen method applies to a submatrix; for
  example, scale a 2-by-2 block or call its `determinant()` instead of spelling out its coefficients. Preserve known
  extents with fixed-size accessors such as `block<Rows, Cols>(i, j)`; in dependent template code, write
  `m.template block<Rows, Cols>(i, j)`. Use runtime extents only when they are genuinely dynamic, and use individual
  coefficient access when entries require different operations. Blocks remain lazy, non-owning views, so the lifetime
  and overlap rules above still apply.
- The two arms of `?:` must have a common C++ type; distinct Eigen expression types often do not. Use `if`/`else` when
  necessary.

### Scalar, index, and storage genericity

Use `Eigen::Index` for dimensions and counts, but remember that its underlying type is configurable. Use `NumTraits` for
scalar properties and Eigen's `numext` helpers when custom-scalar or device support matters. Do not store sizes or loop
counts in `Scalar`, hard-code `float`/`double` without an API reason, or narrow to a vendor API's `int` without checking
the range. Test real, complex, integer, and narrow/custom scalar types according to the operation's documented domain.

Propagate storage-order and expression flags deliberately. `RowMajorBit`, fixed versus dynamic dimensions, alignment,
and vectorization eligibility affect evaluators and fast paths. Eigen alignment depends on configuration and
architecture; do not encode a presumed byte value. Include configuration-sensitive behavior in tests when it changes
semantics or ABI.

### Public APIs and diagnostics

For generic APIs, accept the least restrictive established Eigen base (`EigenBase`, `DenseBase`, `MatrixBase`,
`ArrayBase`, or a suitable `Ref`) that preserves the intended semantics. Follow nearby established patterns for writable
expression arguments; do not cast away constness from genuinely const storage. Public-header additions with non-template
definitions or objects deserve a multiple-translation-unit link test when an ODR regression is plausible.

The supported C++14 configurations cannot rely on C++17 over-aligned value passing. Pass fixed-size vectorizable Eigen
objects by reference rather than by value; see [`doc/PassingByValue.dox`](doc/PassingByValue.dox).

Use `eigen_assert` for runtime preconditions that belong to Eigen's public debug behavior and `eigen_internal_assert`
for internal invariants gated by `EIGEN_INTERNAL_DEBUGGING`. Use the local compile-time assertion style that gives the
clearest diagnostic. Comments should explain non-obvious mathematics, invariants, compatibility constraints, or
provenance rather than narrating the code. Keep comments concise and proportional to the code's complexity. Avoid
tutorial-style prose, section-by-section narration, and comments that restate identifiers or control flow. Longer
comments are justified only when that rationale cannot be expressed clearly in code.

## Quick build and test

By default, tests are not part of the `all` target, although that target may build configured auxiliary libraries. A
typical focused workflow is:

```bash
cmake -G Ninja -S . -B build
cmake --build build --target <test-name>
ctest --test-dir build -R '^<test-name>$' --output-on-failure
```

For a split test such as `foo_3`, build that exact target and match it exactly with CTest. The generated
`buildtests.sh` and `check.sh` wrappers accept source/test-name regexes and are useful for building all matching parts.
Use `buildtests`, `BuildOfficial`, `BuildUnsupported`, `buildsmoketests`, or `check` only when the requested validation
warrants that scope. See [`.agents/testing.md`](.agents/testing.md) for the current test framework, split rules,
configuration variants, and failure-test workflow.

## Completion checklist

Before declaring the task complete:

- The diff contains only intentional changes and preserves pre-existing work.
- New public implementation is reachable through the intended umbrella header.
- New files have correct REUSE metadata and no generated or local-tool files are staged.
- Changed source files pass `clang-format-17`; `git diff --check` is clean.
- Focused regression tests pass, with broader tests or benchmarks run when the risk warrants them.
- Numerical, aliasing, scalar, storage-order, device, threading, and ABI implications have been considered where
  relevant.
- The final report names validation performed, residual risk, and anything that could not be tested locally.

Commit subjects normally use `Category: Short description`, for example
`Core: Fix alias handling in product assignment`.
