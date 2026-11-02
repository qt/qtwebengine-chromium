---
name: chromium_memory_tools
description: >-
  Helps developers debug memory issues, find memory leaks, and analyze
  memory usage in Chromium using tools from tools/memory/, including
  ASan, TSan, and PartitionAlloc utilities.
---

# Chromium Memory Tools Skill

This skill assists with debugging memory issues and analyzing memory usage in
Chromium using the suite of tools available in `tools/memory/`.

## Scope of Skill

Trigger this skill when the user asks to:

1.  "Debug a crash" or "investigate memory corruption."
2.  "Check for memory leaks."
3.  "Find use-after-free errors" or "detect buffer overflows."
4.  "Analyze memory usage."
5.  "Debug threading issues" or "find data races."
6.  Investigate issues related to PartitionAlloc.

## Underlying Tools

### 1. AddressSanitizer (ASan)

*   **Purpose:** Detecting memory errors like use-after-free, buffer overflows,
    etc.
*   **Build Flags:** Use a dedicated output directory (e.g., `out/ASan`) to
    avoid rebuilding and accidentally running with ASan always on. Add the
    following to `out/ASan/args.gn`:

    ```gn
    is_asan = true
    is_debug = false
    ```

*   **Building:**

    ```bash
    autoninja -C out/ASan chrome
    ```

*   **Running Tests/Chrome:**

    ```bash
    # Example running a test
    out/ASan/browser_tests --gtest_filter=YourTest.Name
    # Example running Chrome
    out/ASan/chrome
    ```

*   **Interpreting Output:** ASan reports are typically printed to stderr,
    detailing the type of error, stack traces for allocation, deallocation, and
    the error point.

### 2. ThreadSanitizer (TSan)

*   **Purpose:** Detecting data races and other thread-related memory errors.
*   **Build Flags:** Use a dedicated output directory (e.g., `out/TSan`) to
    avoid rebuilding. Add the following to `out/TSan/args.gn`:

    ```gn
    is_tsan = true
    is_debug = false
    ```

*   **Building:**

    ```bash
    autoninja -C out/TSan chrome
    ```

*   **Running Tests/Chrome:**

    ```bash
    # Example running a test
    out/TSan/unit_tests --gtest_filter=YourTest.Name
    ```

*   **Interpreting Output:** TSan reports data races to stderr, showing the
    stack traces of the conflicting memory accesses from different threads.

*   **Suppressions:** Known false positives should be suppressed at runtime by
    adding them to `build/sanitizers/tsan_suppressions.cc` rather than using
    compile-time ignores.

### 3. PartitionAlloc Tools

*   **Purpose:** Tools for analyzing, debugging, and testing Chromium's custom
    allocator, PartitionAlloc.
*   **Compiled Inspection Tools:** These tools are C++ binaries that must be
    compiled and run from your build output directory (e.g., `out/Default`).
    Ensure both the tool and the running Chrome instance are built at the same
    revision.

    *   `pa_tcache_inspect`: Displays statistics on PartitionAlloc's thread
        caches for a running Chrome process.
    *   `pa_buckets_inspect`: Inspects PartitionAlloc buckets.
    *   `pa_dump_heap`: Dumps the PartitionAlloc heap.
    *   **Building:**

        ```bash
        autoninja -C out/Default pa_tcache_inspect pa_buckets_inspect pa_dump_heap
        ```

    *   **Example Command:**

        ```bash
        # Inspect thread cache of a running Chrome process.
        # On some Linux configurations, this requires enabling ptrace
        # permissions first:
        # sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope'
        out/Default/pa_tcache_inspect <pid>
        ```

*   **Python Scripts:** These scripts are located in the source tree at
    `tools/memory/partition_allocator/` and can be run directly using
    `vpython3`:

    *   Fragmentation: `compute_external_fragmentation.py` and
        `compute_internal_fragmentation.py`.
    *   Visualization: `pa_graph_buckets.py`, `plot_bucket_stats.py`, and
        `plot_superpages.py`.
    *   Profiling: `profile_allocations.py`.
    *   **Example Command:**

        ```bash
        vpython3 tools/memory/partition_allocator/pa_graph_buckets.py <arguments>
        ```

--------------------------------------------------------------------------------

## Errors and Troubleshooting

*   **Build Failures:** Double-check GN args for typos. Ensure component builds
    are not conflicting with sanitizer flags.
*   **Runtime Crashes:** Analyze the stack traces provided by the sanitizers.
    Look for patterns.
*   **False Positives:** While rare for ASan, TSan can have false positives.
    Consult `build/sanitizers/tsan_suppressions.cc` for TSan or consider if the
    code has benign races.
*   **Performance:** Sanitizer builds run significantly slower and use more
    memory. This is expected.
