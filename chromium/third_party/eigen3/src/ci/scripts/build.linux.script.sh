#!/bin/bash
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

set -x

rootdir=`pwd`
mkdir -p ${EIGEN_CI_BUILDDIR}
cd ${EIGEN_CI_BUILDDIR}

cmake -G Ninja                                                   \
  -DCMAKE_CXX_COMPILER=${EIGEN_CI_CXX_COMPILER}                  \
  -DCMAKE_C_COMPILER=${EIGEN_CI_C_COMPILER}                      \
  -DCMAKE_CXX_COMPILER_TARGET=${EIGEN_CI_CXX_COMPILER_TARGET}    \
  ${EIGEN_CI_ADDITIONAL_ARGS} ${rootdir}

target=""
if [[ ${EIGEN_CI_BUILD_TARGET} ]]; then
  target="--target ${EIGEN_CI_BUILD_TARGET}"
fi

# Builds (particularly gcc) sometimes get killed, potentially when running
# out of resources.  In that case, keep trying to build the remaining
# targets (k0), then retry with reduced parallelism to minimize resource use.
# EIGEN_CI_BUILD_JOBS can be set to limit parallelism for memory-hungry
# compilers (e.g. NVHPC).  When unset, leave -j off so ninja picks its
# own default (NPROC + 2).  njobs is still needed for batch-size sizing.
njobs=${EIGEN_CI_BUILD_JOBS:-${NPROC}}
jobs=""
if [[ -n "${EIGEN_CI_BUILD_JOBS}" ]]; then
  jobs="-j${EIGEN_CI_BUILD_JOBS}"
fi

# Fallback parallelism for retry builds after a failure (default: 2).
fallback_jobs="-j${EIGEN_CI_FALLBACK_JOBS:-2}"

# For phony meta-targets (e.g. buildtests), shuffle the dependency list and
# build in batches so that memory-hungry compilations (like bdcsvd with
# nvc++) are spread out instead of all running at once.  Ninja ignores the
# command-line target order and schedules by its dependency graph, so we
# must feed it small batches to actually influence scheduling.
#
# Batch size defaults to 2 * njobs (enough work to keep all cores busy
# without scheduling too many memory-hungry targets simultaneously).
# Override with EIGEN_CI_BUILD_BATCH_SIZE for fine-grained control.
default_batch=$((njobs * 2))
default_batch=$((default_batch > 48 ? default_batch : 48))
batch_size=${EIGEN_CI_BUILD_BATCH_SIZE:-${default_batch}}
shuffled=false
if [[ -n "${EIGEN_CI_BUILD_TARGET}" ]] && command -v ninja >/dev/null 2>&1; then
  # Suppress xtrace while extracting and shuffling the target list
  # to avoid dumping ~1200 lines to the CI log.
  { set +x; } 2>/dev/null
  deps=$(ninja -t query "${EIGEN_CI_BUILD_TARGET}" 2>/dev/null \
         | awk '/^  input:/{found=1; next} /^  outputs:/{found=0} found && /^    /{print $1}')
  # CMake custom targets like BuildOfficial have an intermediate phony
  # (e.g. test/BuildOfficial) that holds the real dependencies.  If we
  # got exactly one dep, resolve it one more level.
  if [[ $(echo "$deps" | wc -l) -eq 1 ]] && [[ -n "$deps" ]]; then
    inner=$(ninja -t query "$deps" 2>/dev/null \
            | awk '/^  input:/{found=1; next} /^  outputs:/{found=0} found && /^    /{print $1}')
    if [[ -n "$inner" ]]; then
      deps="$inner"
    fi
  fi
  # Deterministic shuffle: hash each target name and sort by hash.
  # Stable across runs (helps ninja's .ninja_log and build caches),
  # portable (no shuf dependency), and spreads same-family targets apart.
  # Uses Knuth's multiplicative hash (golden-ratio prime 2654435761) for
  # good avalanche — similar names like bdcsvd_1..bdcsvd_51 get widely
  # dispersed instead of clustering together.
  shuffled_deps=$(echo "$deps" | awk '
    BEGIN { for(i=0;i<128;i++) ord[sprintf("%c",i)]=i }
    { h=0
      for(i=1;i<=length($0);i++) h=(h+ord[substr($0,i,1)])*2654435761%2147483647
      printf "%010d %s\n",h,$0 }' | sort | sed 's/^[^ ]* //')
  if [[ -n "$shuffled_deps" ]]; then
    ndeps=$(echo "$shuffled_deps" | wc -l)
    echo "Building ${ndeps} targets in batches of ${batch_size} (njobs=${njobs})"
    shuffled=true
    # Build in batches: ninja parallelises within each batch, but batches
    # run sequentially so memory-hungry targets from different families
    # don't pile up simultaneously.  Track failures so we can report the
    # right exit code at the end.
    # Note: xtrace stays off to avoid dumping the full target list.
    # Use process substitution so the while loop runs in the current
    # shell and build_failed propagates.
    batch_num=0
    build_failed=false
    while IFS= read -r batch; do
      batch_num=$((batch_num + 1))
      echo "=== Batch ${batch_num} ==="
      ninja -k0 ${jobs} ${batch} || ninja -k0 ${fallback_jobs} ${batch} || build_failed=true
    done < <(echo "$shuffled_deps" | xargs -n "${batch_size}")
    if [[ "$build_failed" == "true" ]]; then
      echo "Some batches failed."
      exit 1
    fi
  fi
  set -x
fi

if [[ "$shuffled" != "true" ]]; then
  cmake --build . ${target} -- -k0 ${jobs} || cmake --build . ${target} -- -k0 ${fallback_jobs}
fi

cd ${rootdir}

set +x
