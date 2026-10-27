#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys


def main():
    args = _parse_input()

    assert _validate_header_input(args.header), \
           "Error: '%s' is not a valid .h file" % args.header
    assert _validate_code_input(args.cc), \
           "Error: '%s' is not a valid .cc file" % args.cc
    assert _validate_path_input(args.gen_dir), \
           "Error: '%s' is not a valid output directory" % args.gen_dir
    assert _validate_cddl_input(args.file), \
           "Error: '%s' is not a valid CDDL file" % args.file

    if args.log:
        log_path = os.path.join(args.gen_dir, args.log)
        log = open(log_path, "w")
        log.write("OUTPUT FOR CDDL CODE GENERATION TOOL:\n\n")
        log = open(log_path, "a")

        if (args.verbose):
            print("Logging to %s" % log_path)
    else:
        log = None

    if (args.verbose):
        print('Creating C++ files from provided CDDL file...')
    _echo_and_run_command([
        args.cddl, "--header", args.header, "--cc", args.cc, "--gen-dir",
        args.gen_dir, args.file
    ], False, log, args.verbose)

    clang_format_location = _find_clang_format()
    if not clang_format_location:
        if args.verbose:
            print("WARNING: clang-format could not be found")
        return

    for filename in [args.header, args.cc]:
        _echo_and_run_command([
            clang_format_location + 'clang-format', "-i",
            os.path.join(args.gen_dir, filename)
        ],
                              True,
                              verbose=args.verbose)


def _parse_input():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cddl", help="path to the cddl executable to use")
    parser.add_argument("--header",
                        help="Specify the filename of the output \
     header file. This is also the name that will be used for the include \
     guard and as the include path in the source file.")
    parser.add_argument("--cc",
                        help="Specify the filename of the output \
     source file")
    parser.add_argument("--gen-dir",
                        help="Specify the directory prefix that \
     should be added to the output header and source file.")
    parser.add_argument("--log",
                        help="Specify the file to which stdout should \
     be redirected.")
    parser.add_argument("--verbose",
                        help="Specify that we should log info \
     messages to stdout")
    parser.add_argument("file", help="the input file which contains the spec")
    return parser.parse_args()


def _validate_header_input(header_file):
    return header_file and header_file.endswith('.h')


def _validate_code_input(cc_file):
    return cc_file and cc_file.endswith('.cc')


def _validate_path_input(dir_path):
    return dir_path and os.path.isdir(dir_path)


def _validate_cddl_input(cddl_file):
    return cddl_file and os.path.isfile(cddl_file)


def _echo_and_run_command(command_array,
                          allow_failure,
                          logfile=None,
                          verbose=False):
    if verbose:
        print("\tExecuting Command: '%s'" % " ".join(command_array))

    if logfile != None:
        process = subprocess.Popen(command_array,
                                   stdout=logfile,
                                   stderr=logfile)
        process.wait()
        logfile.flush()
    else:
        process = subprocess.Popen(command_array)
        process.wait()

    returncode = process.returncode
    if returncode != None and returncode != 0:
        if not allow_failure:
            sys.exit("\t\tERROR: Command failed with error code: '%i'!" %
                     returncode)
        elif verbose:
            print("\t\tWARNING: Command failed with error code: '%i'!" %
                  returncode)


def _find_clang_format():
    executable = "clang-format"

    # Try and run from the environment variable
    for directory in os.environ["PATH"].split(os.pathsep):
        full_path = os.path.join(directory, executable)
        if os.path.isfile(full_path):
            return ""

    # Check 2 levels up since this should be correct on the build machine
    path = "../../"
    full_path = os.path.join(path, executable)
    if os.path.isfile(full_path):
        return path

    return None


if __name__ == "__main__":
    main()
