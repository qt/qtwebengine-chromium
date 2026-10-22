#!/bin/bash
# Copyright 2026 The PDFium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# adb_push_test.sh: push files from a PDFium checkout to android emulator.
#
# PRECONDITIONS:
#   Android emulator already configured on host.
#   Android binaries build into out/Android.
#   Script is run from top-level pdfium dirctory.

LOCAL_BUILD_DIR=out/Android
REMOTE_EXE_DIR=/data/local/tmp
REMOTE_TEST_DIR=/data/local/chromium_tests_root

# `adb push` functions differently depending upon existence of remote
# directories. Ensure these exist, even on a newly-created emulator.
adb shell mkdir -p ${REMOTE_TEST_DIR}/testing
adb shell mkdir -p ${REMOTE_TEST_DIR}/third_party

# Push the entire build directory to pick up things like shared libraries
# and v8 snapshots. This could be made more specific. The `*` expansion
# avoid having /Android in the final path.
adb push --sync ${LOCAL_BUILD_DIR}/*  ${REMOTE_EXE_DIR}

# Copy test binaries upwards, replacing python invoker placeholders.
adb shell cp ${REMOTE_EXE_DIR}/pdfium_unittests__dist/pdfium_unittests \
    ${REMOTE_EXE_DIR}/pdfium_unittests

adb shell cp ${REMOTE_EXE_DIR}/pdfium_embeddertests__dist/pdfium_embeddertests \
    ${REMOTE_EXE_DIR}/pdfium_embeddertests

# Now push the test resources.
adb push --sync testing/resources ${REMOTE_TEST_DIR}/testing
adb push --sync third_party/NotoSansCJK ${REMOTE_TEST_DIR}/third_party
adb push --sync ${LOCAL_BUILD_DIR}/test_fonts ${REMOTE_EXE_DIR}

# Now run the tests
adb shell ${REMOTE_EXE_DIR}/pdfium_unittests
adb shell ${REMOTE_EXE_DIR}/pdfium_embeddertests
