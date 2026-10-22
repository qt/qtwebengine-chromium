#!/bin/sh
# Push benchmark results to the perf-data orphan branch.
# POSIX sh compatible (runs under Alpine's busybox ash).
#
# Expected environment variables:
#   EIGEN_CI_BUILDDIR       - build directory containing results/
#   EIGEN_CI_GIT_PUSH_URL   - authenticated git push URL
#   CI_COMMIT_SHORT_SHA     - short commit hash

set -ex

results_dir="$(pwd)/${EIGEN_CI_BUILDDIR:-.bench-build}/results"
perf_branch="perf-data"
clone_dir="/tmp/perf-data-push"
push_url="${EIGEN_CI_GIT_PUSH_URL:?EIGEN_CI_GIT_PUSH_URL must be set}"

rm -rf "${clone_dir}"

# Clone perf-data branch, or create orphan if it doesn't exist.
if git clone --depth=1 --single-branch --branch "${perf_branch}" \
     "${push_url}" "${clone_dir}" 2>/dev/null; then
  echo "Cloned existing ${perf_branch} branch."
else
  echo "${perf_branch} branch does not exist, creating orphan..."
  mkdir -p "${clone_dir}"
  cd "${clone_dir}"
  git init
  git checkout --orphan "${perf_branch}"
  cat > README.md <<'EOF'
# Benchmark Performance Data

This branch stores nightly/weekly benchmark results as JSON files.
It is maintained automatically by the CI benchmark pipeline.

## Structure

    <target>/
      <date>_<commit>_<target>.json

## Analysis

See `ci/scripts/detect_regressions.py` on the main branch for the
regression detection script that consumes this data.
EOF
  git add README.md
  git -c user.name="CI Bot" -c user.email="ci@eigen.tuxfamily.org" \
    commit -m "Initialize perf-data branch"
  git remote add origin "${push_url}"
  cd -
fi

cd "${clone_dir}"

# Copy combined result files into target subdirectories.
# Only match canonical combined formats:
#   YYYY-MM-DDTHH-MM-SSZ_<hex>_<target>.json
#   YYYY-MM-DD_<hex>_<target>.json
# This avoids picking up raw per-benchmark files like bench_gemm_double.json.
copied=0
for combined_json in "${results_dir}"/*.json; do
  [ -f "${combined_json}" ] || continue
  filename=$(basename "${combined_json}")
  # Must start with a UTC timestamp or date, followed by a hex commit hash.
  case "${filename}" in
    [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]-[0-9][0-9]-[0-9][0-9]Z_[0-9a-f]*_*.json) ;;
    [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9a-f]*_*.json) ;;
    *) continue ;;
  esac
  # Extract target: strip timestamp/date + commit prefix and .json suffix.
  target=$(echo "${filename}" | sed 's/^[^_]*_[a-f0-9]*_//' | sed 's/\.json$//')
  mkdir -p "${target}"
  cp "${combined_json}" "${target}/${filename}"
  copied=$((copied + 1))
done

if [ "${copied}" -eq 0 ]; then
  echo "No result files to store."
  exit 0
fi

# Prune data older than 90 days to keep the branch manageable.
# We parse the date from the filename since clone mtime is always "now".
cutoff=$(date -u -d "@$(($(date +%s) - 90*86400))" +%Y-%m-%d)
find . -name '*.json' -path './*/*.json' | while IFS= read -r f; do
  file_date=$(basename "$f" | grep -oE '^[0-9]{4}-[0-9]{2}-[0-9]{2}')
  if [ -n "$file_date" ] && [ "$file_date" \< "$cutoff" ]; then
    rm -f "$f"
  fi
done

# Commit and push.
git add -A
git -c user.name="CI Bot" -c user.email="ci@eigen.tuxfamily.org" \
  commit -m "Add benchmark results for $(date -u +%Y-%m-%d) (${CI_COMMIT_SHORT_SHA:-unknown})" || {
  echo "No changes to commit."
  exit 0
}
git push origin "${perf_branch}"

echo "Pushed ${copied} result file(s) to ${perf_branch} branch."
