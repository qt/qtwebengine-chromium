#!/usr/bin/env python3
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

"""Benchmark regression detection using Welch's t-test.

Compares the current benchmark run against historical data stored on
the perf-data git branch.  A regression is flagged when:

  1. Welch's t-test p-value < significance threshold (default 0.01)
  2. The relative change exceeds a minimum percentage (default 5%)
  3. The direction is a slowdown (higher real_time)

Exit codes:
  0  no regressions
  1  regressions detected
  2  error
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict, namedtuple

# scipy is the only external dependency (pip-installed in the CI job).
from scipy.stats import ttest_ind

Regression = namedtuple(
    "Regression",
    ["target", "key", "current_mean", "historical_mean", "change_pct", "p_value"],
)


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--results-dir",
        required=True,
        help="Directory containing current run JSON files.",
    )
    p.add_argument(
        "--perf-branch",
        default="perf-data",
        help="Git branch storing historical benchmark data.",
    )
    p.add_argument(
        "--history-count",
        type=int,
        default=30,
        help="Number of past runs to compare against.",
    )
    p.add_argument(
        "--significance",
        type=float,
        default=0.01,
        help="P-value threshold for Welch's t-test.",
    )
    p.add_argument(
        "--min-change-pct",
        type=float,
        default=5.0,
        help="Minimum percentage change to flag.",
    )
    p.add_argument(
        "--output-report",
        default="regression_report.txt",
        help="Path for text report.",
    )
    return p.parse_args()


def clone_perf_branch(branch, clone_dir):
    """Shallow-clone the perf-data branch.  Returns True on success."""
    # Construct clone URL from CI environment or fall back to current remote.
    url = os.environ.get("CI_REPOSITORY_URL", "")
    if not url:
        try:
            url = subprocess.check_output(
                ["git", "remote", "get-url", "origin"], text=True
            ).strip()
        except Exception:
            return False

    try:
        subprocess.check_call(
            [
                "git",
                "clone",
                "--depth=1",
                "--single-branch",
                "--branch",
                branch,
                url,
                clone_dir,
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return True
    except subprocess.CalledProcessError:
        return False


def _history_sort_key(fpath):
    """Sort key for historical result files.

    Prefer the recorded UTC timestamp in the JSON metadata. Fall back to the
    filename so older date-only files still participate in the history window.
    """
    try:
        with open(fpath) as f:
            metadata = json.load(f).get("metadata", {})
    except Exception:
        metadata = {}
    return metadata.get("timestamp") or metadata.get("date") or os.path.basename(fpath)


def load_historical_data(perf_dir, target, history_count):
    """Load per-repetition real_time values from the last *history_count* runs.

    Returns dict: benchmark_key -> list of raw real_time values (multiple per run).

    We load the same non-aggregate rows that load_current_results uses so both
    sides of the t-test contain the same kind of measurement (individual
    repetitions), avoiding a unit mismatch between per-rep and per-run means.
    """
    target_dir = os.path.join(perf_dir, target)
    if not os.path.isdir(target_dir):
        return {}

    files = sorted(
        glob.glob(os.path.join(target_dir, "*.json")),
        key=_history_sort_key,
        reverse=True,
    )
    files = files[:history_count]

    history = defaultdict(list)
    for fpath in files:
        with open(fpath) as f:
            data = json.load(f)
        for exe_name, exe_data in data.get("files", {}).items():
            for bm in exe_data.get("benchmarks", []):
                run_type = bm.get("run_type", "")
                if run_type == "aggregate":
                    continue
                name = bm.get("name", "")
                key = f"{exe_name}/{name}"
                rt = bm.get("real_time")
                if rt is not None:
                    history[key].append(rt)
    return history


def load_current_results(results_dir):
    """Load current run results, keyed by target.

    Returns dict: target -> dict(benchmark_key -> list of per-repetition real_time).
    """
    data = defaultdict(lambda: defaultdict(list))

    for jf in sorted(glob.glob(os.path.join(results_dir, "*_*_*.json"))):
        with open(jf) as f:
            run = json.load(f)
        meta = run.get("metadata", {})
        target = meta.get("target", "unknown")

        for exe_name, exe_data in run.get("files", {}).items():
            for bm in exe_data.get("benchmarks", []):
                name = bm.get("name", "")
                run_type = bm.get("run_type", "")
                # Use individual iteration rows (not aggregates) for the
                # current run so we have per-repetition samples.
                if run_type == "aggregate":
                    continue
                key = f"{exe_name}/{name}"
                rt = bm.get("real_time")
                if rt is not None:
                    data[target][key].append(rt)

    return data


def find_regressions(current, historical, significance, min_change_pct):
    """Compare current vs historical using Welch's t-test.

    Returns (regressions, improvements, skipped_count).
    """
    regressions = []
    improvements = []
    skipped = 0

    for key, current_values in sorted(current.items()):
        hist_values = historical.get(key)
        if not hist_values or len(hist_values) < 5:
            skipped += 1
            continue
        if len(current_values) < 3:
            skipped += 1
            continue

        cur_mean = sum(current_values) / len(current_values)
        hist_mean = sum(hist_values) / len(hist_values)

        if hist_mean == 0:
            skipped += 1
            continue

        change_pct = (cur_mean - hist_mean) / hist_mean * 100.0

        _, p_value = ttest_ind(current_values, hist_values, equal_var=False)

        entry = Regression(
            target="",  # filled in by caller
            key=key,
            current_mean=cur_mean,
            historical_mean=hist_mean,
            change_pct=change_pct,
            p_value=p_value,
        )

        if p_value < significance and abs(change_pct) > min_change_pct:
            if change_pct > 0:
                # Higher real_time = slower = regression.
                regressions.append(entry)
            else:
                improvements.append(entry)

    return regressions, improvements, skipped


def _qualified_key(r):
    """Target-qualified display key, e.g. '[x86-64-avx2] bench_gemm/BM_Gemm/256'."""
    return f"[{r.target}] {r.key}"


def write_text_report(regressions, improvements, skipped, total, path):
    """Write a human-readable summary."""
    with open(path, "w") as f:
        f.write("# Benchmark Regression Report\n\n")

        if regressions:
            f.write(f"## Regressions ({len(regressions)})\n\n")
            f.write(
                f"{'Benchmark':<70s} {'Historical':>12s} {'Current':>12s} "
                f"{'Change':>8s} {'p-value':>8s}\n"
            )
            f.write("-" * 114 + "\n")
            for r in sorted(regressions, key=lambda x: -x.change_pct):
                f.write(
                    f"{_qualified_key(r):<70s} {r.historical_mean:>12.1f} {r.current_mean:>12.1f} "
                    f"{r.change_pct:>+7.1f}% {r.p_value:>8.4f}\n"
                )
            f.write("\n")

        if improvements:
            f.write(f"## Improvements ({len(improvements)})\n\n")
            f.write(
                f"{'Benchmark':<70s} {'Historical':>12s} {'Current':>12s} "
                f"{'Change':>8s} {'p-value':>8s}\n"
            )
            f.write("-" * 114 + "\n")
            for r in sorted(improvements, key=lambda x: x.change_pct):
                f.write(
                    f"{_qualified_key(r):<70s} {r.historical_mean:>12.1f} {r.current_mean:>12.1f} "
                    f"{r.change_pct:>+7.1f}% {r.p_value:>8.4f}\n"
                )
            f.write("\n")

        f.write(f"## Summary\n\n")
        f.write(f"- Benchmarks analyzed: {total}\n")
        f.write(f"- Regressions: {len(regressions)}\n")
        f.write(f"- Improvements: {len(improvements)}\n")
        f.write(f"- Skipped (insufficient data): {skipped}\n")


def write_junit_report(regressions, analyzed_keys, path):
    """Write JUnit XML so GitLab displays results in the test report tab.

    Keys in *analyzed_keys* and regression entries are target-qualified
    (e.g. "[x86-64-avx2] bench_gemm/BM_Gemm/256") so the same benchmark
    on different ISA targets appears as separate test cases.
    """
    suite = ET.Element(
        "testsuite",
        name="benchmark-regressions",
        tests=str(len(analyzed_keys)),
        failures=str(len(regressions)),
    )

    regression_by_qkey = {_qualified_key(r): r for r in regressions}
    for key in sorted(analyzed_keys):
        tc = ET.SubElement(suite, "testcase", name=key, classname="benchmark")
        r = regression_by_qkey.get(key)
        if r is not None:
            ET.SubElement(
                tc,
                "failure",
                message=f"{r.change_pct:+.1f}% regression (p={r.p_value:.4f})",
            ).text = (
                f"historical_mean={r.historical_mean:.1f} "
                f"current_mean={r.current_mean:.1f} "
                f"change={r.change_pct:+.1f}% p={r.p_value:.6f}"
            )

    tree = ET.ElementTree(suite)
    ET.indent(tree)
    tree.write(path, xml_declaration=True, encoding="utf-8")


def main():
    args = parse_args()
    results_dir = args.results_dir

    # Load current results (keyed by target).
    current_by_target = load_current_results(results_dir)
    if not current_by_target:
        print("No current benchmark results found.")
        sys.exit(2)

    total_benchmarks = sum(len(v) for v in current_by_target.values())
    print(f"Loaded {total_benchmarks} benchmarks from current run.")
    print(f"Targets: {', '.join(sorted(current_by_target.keys()))}")

    # Clone historical data.
    perf_dir = "/tmp/perf-data-history"
    has_history = clone_perf_branch(args.perf_branch, perf_dir)

    if not has_history:
        print("No historical data found (perf-data branch missing).")
        print("This is expected on the first run. Storing baseline only.")
        sys.exit(0)

    # Run analysis per target.
    all_regressions = []
    all_improvements = []
    total_analyzed = 0
    total_skipped = 0
    all_keys = set()

    for target in sorted(current_by_target.keys()):
        target_current = current_by_target[target]
        historical = load_historical_data(perf_dir, target, args.history_count)
        if not historical:
            print(f"  {target}: no historical data, skipping analysis.")
            continue

        regs, imps, skipped = find_regressions(
            target_current, historical, args.significance, args.min_change_pct
        )

        # Tag regressions with the target.
        regs = [r._replace(target=target) for r in regs]
        imps = [r._replace(target=target) for r in imps]

        all_regressions.extend(regs)
        all_improvements.extend(imps)
        total_analyzed += len(target_current) - skipped
        total_skipped += skipped
        # Use target-qualified keys so the same benchmark on different ISAs
        # shows up as separate entries in reports.
        all_keys.update(f"[{target}] {k}" for k in target_current)

        print(
            f"  {target}: {len(regs)} regressions, "
            f"{len(imps)} improvements, {skipped} skipped"
        )

    # Write reports.
    report_path = args.output_report
    write_text_report(
        all_regressions, all_improvements, total_skipped, total_analyzed, report_path
    )
    print(f"\nText report: {report_path}")

    junit_path = report_path.replace(".txt", ".xml")
    write_junit_report(all_regressions, all_keys, junit_path)
    print(f"JUnit report: {junit_path}")

    # Print summary and exit.
    if all_regressions:
        print(f"\nREGRESSIONS DETECTED: {len(all_regressions)} benchmark(s)")
        for r in all_regressions:
            print(f"  [{r.target}] {r.key}: {r.change_pct:+.1f}% (p={r.p_value:.4f})")
        sys.exit(1)
    else:
        n_imp = len(all_improvements)
        print(f"\nNo regressions detected. {n_imp} improvement(s) found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
