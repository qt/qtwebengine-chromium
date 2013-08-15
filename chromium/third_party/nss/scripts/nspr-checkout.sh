#!/bin/sh
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This shell script checks out the NSPR source tree from CVS and prepares
# it for Chromium.

# Make the script exit as soon as something fails.
set -ex

rm -rf mozilla/nsprpub
cvs -q -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot export \
    -r NSPR_4_9_5_BETA2 NSPR

rm -r mozilla/nsprpub/admin
rm -r mozilla/nsprpub/build
rm -r mozilla/nsprpub/config
rm -r mozilla/nsprpub/lib/prstreams
rm -r mozilla/nsprpub/lib/tests
rm -r mozilla/nsprpub/pkg
rm -r mozilla/nsprpub/pr/src/cplus
rm -r mozilla/nsprpub/pr/tests
rm -r mozilla/nsprpub/tools

# Remove unneeded platform-specific directories.
rm -r mozilla/nsprpub/pr/src/bthreads
rm -r mozilla/nsprpub/pr/src/md/beos
rm -r mozilla/nsprpub/pr/src/md/os2

find mozilla/nsprpub -name .cvsignore -print | xargs rm
find mozilla/nsprpub -name README -print | xargs rm

# Remove the build system.
rm mozilla/nsprpub/aclocal.m4
rm mozilla/nsprpub/configure
rm mozilla/nsprpub/configure.in
find mozilla/nsprpub -name Makefile.in -print | xargs rm
find mozilla/nsprpub -name "*.mk" -print | xargs rm

# Remove files for building shared libraries/DLLs.
find mozilla/nsprpub -name "*.def" -print | xargs rm
find mozilla/nsprpub -name "*.rc" -print | xargs rm
find mozilla/nsprpub -name prvrsion.c -print | xargs rm
find mozilla/nsprpub -name plvrsion.c -print | xargs rm

# Remove unneeded platform-specific files in mozilla/nsprpub/pr/include/md.
find mozilla/nsprpub/pr/include/md -name "_*" ! -name "_darwin.*" \
    ! -name "_linux.*" ! -name "_win95.*" ! -name _pth.h ! -name _pcos.h \
    ! -name _unixos.h ! -name _unix_errors.h ! -name _win32_errors.h -print \
    | xargs rm

# Remove files for unneeded Unix flavors.
find mozilla/nsprpub/pr/src/md/unix -type f ! -name "ux*.c" ! -name unix.c \
    ! -name unix_errors.c ! -name darwin.c ! -name "os_Darwin*.s" \
    ! -name linux.c ! -name "os_Linux*.s" -print \
    | xargs rm
rm mozilla/nsprpub/pr/src/md/unix/os_Darwin_ppc.s
rm mozilla/nsprpub/pr/src/md/unix/os_Linux_ppc.s
rm mozilla/nsprpub/pr/src/md/unix/os_Linux_ia64.s
rm mozilla/nsprpub/pr/src/md/unix/uxpoll.c

# Remove files for the WINNT build configuration.
rm mozilla/nsprpub/pr/src/md/windows/ntdllmn.c
rm mozilla/nsprpub/pr/src/md/windows/ntio.c
rm mozilla/nsprpub/pr/src/md/windows/ntthread.c

# Remove obsolete files or files we don't need.
rm mozilla/nsprpub/pr/include/gencfg.c
rm mozilla/nsprpub/pr/src/misc/compile-et.pl
rm mozilla/nsprpub/pr/src/misc/dtoa.c
rm mozilla/nsprpub/pr/src/misc/prerr.et
rm mozilla/nsprpub/pr/src/misc/prerr.properties
