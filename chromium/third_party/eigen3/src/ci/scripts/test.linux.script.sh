#!/bin/bash
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

set -x

rootdir=`pwd`
cd ${EIGEN_CI_BUILDDIR}

target=""
if [[ ${EIGEN_CI_CTEST_REGEX} ]]; then
  target="-R ${EIGEN_CI_CTEST_REGEX}"
elif [[ ${EIGEN_CI_CTEST_LABEL} ]]; then
  target="-L ${EIGEN_CI_CTEST_LABEL}"
fi

exclude=""
if [[ -n "${EIGEN_CI_CTEST_EXCLUDE}" ]]; then
  exclude="-E ${EIGEN_CI_CTEST_EXCLUDE}"
fi

set +x

EIGEN_CI_CTEST_PARALLEL=${EIGEN_CI_CTEST_PARALLEL:-${NPROC}}
# Total attempts for flaky tests (passed to ctest --repeat until-pass:N).
EIGEN_CI_CTEST_REPEAT=${EIGEN_CI_CTEST_REPEAT:-3}
ctest_cmd="ctest ${EIGEN_CI_CTEST_ARGS} --parallel ${EIGEN_CI_CTEST_PARALLEL} --output-on-failure --no-compress-output --build-noclean ${target} ${exclude}"

echo "Running initial tests..."
if ${ctest_cmd} -T test; then
  echo "Tests passed on the first attempt."
  exit_code=0
else
  echo "Initial tests failed with exit code $?. Retrying up to ${EIGEN_CI_CTEST_REPEAT} times..."
  if ${ctest_cmd} --rerun-failed --repeat until-pass:${EIGEN_CI_CTEST_REPEAT}; then
    echo "Tests passed on retry."
    # 42 = passed-on-retry; .test:linux / .test:windows whitelist it via
    # allow_failure.exit_codes so the job is marked as a soft warning.
    exit_code=42
  else
    exit_code=$?
  fi
fi

set -x

cd ${rootdir}

set +x

exit $exit_code
