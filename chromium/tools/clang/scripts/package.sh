#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang, and then package the results up
# to a tgz file.

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BOOTSTRAP_DIR="${THIS_DIR}/../../../third_party/llvm-bootstrap"
LLVM_BUILD_DIR="${THIS_DIR}/../../../third_party/llvm-build"
LLVM_BIN_DIR="${LLVM_BUILD_DIR}/Release+Asserts/bin"
LLVM_LIB_DIR="${LLVM_BUILD_DIR}/Release+Asserts/lib"

echo "Diff in llvm:" | tee buildlog.txt
svn stat "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/tools/clang:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/tools/clang" 2>&1 | tee -a buildlog.txt
echo "Diff in llvm/projects/compiler-rt:" | tee -a buildlog.txt
svn stat "${LLVM_DIR}/projects/compiler-rt" 2>&1 | tee -a buildlog.txt
svn diff "${LLVM_DIR}/projects/compiler-rt" 2>&1 | tee -a buildlog.txt

echo "Starting build" | tee -a buildlog.txt

set -ex

# Do a clobber build.
rm -rf "${LLVM_BOOTSTRAP_DIR}"
rm -rf "${LLVM_BUILD_DIR}"
"${THIS_DIR}"/update.sh --run-tests --bootstrap --force-local-build 2>&1 | \
    tee -a buildlog.txt

R=$("${LLVM_BIN_DIR}/clang" --version | \
     sed -ne 's/clang version .*(trunk \([0-9]*\))/\1/p')

PDIR=clang-$R
rm -rf $PDIR
mkdir $PDIR
mkdir $PDIR/bin
mkdir $PDIR/lib

if [ "$(uname -s)" = "Darwin" ]; then
  SO_EXT="dylib"
else
  SO_EXT="so"
fi

# Copy buildlog over.
cp buildlog.txt $PDIR/

# Copy clang into pdir, symlink clang++ to it.
cp "${LLVM_BIN_DIR}/clang" $PDIR/bin/
(cd $PDIR/bin && ln -sf clang clang++ && cd -)
cp "${LLVM_BIN_DIR}/llvm-symbolizer" $PDIR/bin/

# Copy plugins. Some of the dylibs are pretty big, so copy only the ones we
# care about.
cp "${LLVM_LIB_DIR}/libFindBadConstructs.${SO_EXT}" $PDIR/lib

# Copy built-in headers (lib/clang/3.2/include).
# libcompiler-rt puts all kinds of libraries there too, but we want only some.
if [ "$(uname -s)" = "Darwin" ]; then
  # Keep only
  # Release+Asserts/lib/clang/*/lib/darwin/libclang_rt.{asan,profile}_osx*
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/darwin*' \
       ! -name '*asan_osx*' ! -name '*profile_osx*' | xargs rm
  # Fix LC_ID_DYLIB for the ASan dynamic library to be relative to
  # @executable_path.
  # TODO(glider): this is transitional. We'll need to fix the dylib name
  # either in our build system, or in Clang. See also http://crbug.com/170629.
  ASAN_DYLIB_NAME=libclang_rt.asan_osx_dynamic.dylib
  ASAN_DYLIB=$(find "${LLVM_LIB_DIR}/clang" -type f -path "*${ASAN_DYLIB_NAME}")
  install_name_tool -id @executable_path/${ASAN_DYLIB_NAME} "${ASAN_DYLIB}"
else
  # Keep only
  # Release+Asserts/lib/clang/*/lib/linux/libclang_rt.{[atm]san,profile,ubsan}-*.a
  find "${LLVM_LIB_DIR}/clang" -type f -path '*lib/linux*' \
       ! -name '*[atm]san*' ! -name '*profile*' ! -name '*ubsan*' | xargs rm
fi

cp -R "${LLVM_LIB_DIR}/clang" $PDIR/lib

tar zcf $PDIR.tgz -C $PDIR bin lib buildlog.txt

if [ "$(uname -s)" = "Darwin" ]; then
  PLATFORM=Mac
else
  PLATFORM=Linux_x64
fi

echo To upload, run:
echo gsutil cp -a public-read $PDIR.tgz \
     gs://chromium-browser-clang/$PLATFORM/$PDIR.tgz
