#!/bin/bash
# SPDX-FileCopyrightText: The Eigen Authors
# SPDX-License-Identifier: MPL-2.0

set -x

rootdir=`pwd`
cd ${EIGEN_CI_BUILDDIR}

# Generate test results. xsltproc is installed by common.linux.before_script.sh.
xsltproc ${rootdir}/ci/CTest2JUnit.xsl Testing/`head -n 1 < Testing/TAG`/Test.xml > "JUnitTestResults_$CI_JOB_ID.xml"

cd ${rootdir}

set +x
