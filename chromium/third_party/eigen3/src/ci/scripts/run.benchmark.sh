#!/bin/bash
# Run Eigen benchmarks and collect JSON results with metadata.
#
# Expected environment variables:
#   EIGEN_CI_BUILDDIR          - build directory containing benchmark executables
#   EIGEN_BENCH_TARGET         - ISA target name (e.g. "x86-64-avx2")
#   EIGEN_BENCH_SCOPE          - "nightly" (core subset) or "weekly" (all)
#   EIGEN_BENCH_REPETITIONS    - number of repetitions per benchmark (default: 5)
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

set -ex

rootdir=$(pwd)
builddir=${EIGEN_CI_BUILDDIR:-.bench-build}
results_dir="$(pwd)/${builddir}/results"
mkdir -p "${results_dir}"

target=${EIGEN_BENCH_TARGET:?EIGEN_BENCH_TARGET must be set}
scope=${EIGEN_BENCH_SCOPE:-nightly}
reps=${EIGEN_BENCH_REPETITIONS:-5}

# Auto-promote to weekly on Sundays (day 0) so the full suite runs once a
# week without requiring a separate GitLab schedule.
if [ "${scope}" = "nightly" ] && [ "$(date -u +%u)" = "7" ]; then
  echo "Sunday detected, promoting scope from nightly to weekly."
  scope="weekly"
fi

# Runtime ISA check: skip if the runner lacks the required instruction set.
if [[ "${target}" == *"avx512"* ]]; then
  if ! grep -q 'avx512dq' /proc/cpuinfo 2>/dev/null; then
    echo "WARNING: Runner does not support AVX-512 DQ. Skipping benchmarks."
    exit 0
  fi
fi
if [[ "${target}" == *"avx2"* ]]; then
  if ! grep -q 'avx2' /proc/cpuinfo 2>/dev/null; then
    echo "WARNING: Runner does not support AVX2. Skipping benchmarks."
    exit 0
  fi
fi

cd "${builddir}"

# Determine which benchmarks to run.
bench_list=()
if [[ "${scope}" == "weekly" ]]; then
  while IFS= read -r -d '' exe; do
    bench_list+=("${exe}")
  done < <(find . -maxdepth 1 -type f -executable -name 'bench_*' -print0 | sort -z)
else
  while IFS= read -r name; do
    [[ -z "$name" || "$name" == \#* ]] && continue
    name=$(echo "$name" | xargs)  # trim whitespace
    [[ -z "$name" ]] && continue
    if [[ -x "./${name}" ]]; then
      bench_list+=("./${name}")
    else
      echo "WARNING: ${name} not found, skipping."
    fi
  done < "${rootdir}/ci/scripts/benchmark_targets.txt"
fi

if [[ ${#bench_list[@]} -eq 0 ]]; then
  echo "ERROR: No benchmark executables found."
  exit 1
fi

# Collect system info.
cpu_model=$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || echo "unknown")
timestamp=$(date -u +%Y-%m-%dT%H-%M-%SZ)
commit=${CI_COMMIT_SHORT_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")}
successful_runs=0

# Run each benchmark executable.
for bench_exe in "${bench_list[@]}"; do
  bench_name=$(basename "${bench_exe}")
  outfile="${results_dir}/${bench_name}.json"

  echo "=== Running ${bench_name} (${reps} repetitions) ==="
  if ! "${bench_exe}" \
    --benchmark_format=json \
    --benchmark_out="${outfile}" \
    --benchmark_repetitions="${reps}" \
    --benchmark_report_aggregates_only=false \
    2>&1; then
    echo "WARNING: ${bench_name} failed (possibly SIGILL), skipping."
    rm -f "${outfile}"
    continue
  fi
  successful_runs=$((successful_runs + 1))
done

cd "${rootdir}"

if [[ ${successful_runs} -eq 0 ]]; then
  echo "ERROR: No benchmark executables completed successfully."
  exit 1
fi

# Wrap each result file with metadata and produce a combined output.
python3 - "${results_dir}" "${timestamp}" "${commit}" "${target}" "${cpu_model}" "${scope}" <<'PYEOF'
import json
import glob
import os
import sys

results_dir = sys.argv[1]
timestamp   = sys.argv[2]
commit      = sys.argv[3]
target      = sys.argv[4]
cpu_model   = sys.argv[5]
scope       = sys.argv[6]

metadata = {
    "timestamp": timestamp,
    "date": timestamp[:10],
    "commit": commit,
    "target": target,
    "cpu_model": cpu_model,
    "scope": scope,
    "ci_job_id": os.environ.get("CI_JOB_ID", ""),
    "ci_pipeline_id": os.environ.get("CI_PIPELINE_ID", ""),
    "runner_description": os.environ.get("CI_RUNNER_DESCRIPTION", ""),
}

combined = {"metadata": metadata, "files": {}}

for jf in sorted(glob.glob(os.path.join(results_dir, "bench_*.json"))):
    name = os.path.splitext(os.path.basename(jf))[0]
    with open(jf) as f:
        data = json.load(f)
    entry = {
        "context": data.get("context", {}),
        "benchmarks": data.get("benchmarks", []),
    }
    combined["files"][name] = entry

outpath = os.path.join(results_dir, f"{timestamp}_{commit}_{target}.json")
with open(outpath, "w") as f:
    json.dump(combined, f, indent=2)

print(f"Combined results written to {outpath}")
print(f"  {len(combined['files'])} benchmark files, target={target}")
PYEOF
