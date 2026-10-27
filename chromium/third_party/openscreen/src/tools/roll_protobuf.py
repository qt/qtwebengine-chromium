#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Automates the protobuf roll process.

This script implements the manual steps described in
third_party/protobuf/README.chromium.
"""

import argparse
from dataclasses import dataclass
import glob
import os
import re
import shutil
import subprocess
import sys
import tarfile
from typing import List, Optional, Set, Tuple

import curlish
from cli_utils import print_error, print_step, print_substep, print_success

PROTOBUF_DIR = 'third_party/protobuf'
PROTOBUF_GITHUB_URL = 'https://github.com/protocolbuffers/protobuf.git'
README_PATH = os.path.join(PROTOBUF_DIR, 'README.chromium')


@dataclass
class ProtobufVersion:
    """Encapsulates a protobuf version and its corresponding git revision."""
    version: str
    revision: str


def _run_cmd(cmd: List[str], **kwargs) -> subprocess.CompletedProcess:
    """Runs a shell command and returns the CompletedProcess object.

    Args:
        cmd: A list of strings representing the command and its arguments.
        **kwargs: Additional keyword arguments passed to subprocess.run.

    Returns:
        The CompletedProcess instance.
    """
    kwargs.setdefault('check', True)
    kwargs.setdefault('text', True)
    return subprocess.run(cmd, **kwargs)


def _get_repo_root() -> str:
    """Returns the absolute path to the root of the git repository."""
    try:
        result = _run_cmd(['git', 'rev-parse', '--show-toplevel'],
                          capture_output=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        print_error('This script must be run from within the openscreen '
                    'git repository.')
        sys.exit(1)


def _get_available_versions() -> List[str]:
    """Fetches a list of recent potential version upgrades from GitHub tags.

    Returns:
        A list of tag strings sorted by semantic version.
    """
    try:
        result = _run_cmd(['git', 'ls-remote', '--tags', PROTOBUF_GITHUB_URL],
                          capture_output=True)
        tags = []
        for line in result.stdout.splitlines():
            match = re.search(r'refs/tags/v([0-9]+\.[0-9]+(?:\.[0-9]+)?)$',
                              line)
            if match:
                tags.append(match.group(1))

        tags.sort(key=lambda s: [int(x) for x in s.split('.')])
        return tags[-10:]
    except subprocess.CalledProcessError:
        return []


def _check_working_tree_is_clean() -> None:
    """Ensures the local git working tree has no uncommitted changes."""
    if _run_cmd(['git', 'diff', '--quiet', 'HEAD'],
                check=False).returncode != 0:
        print_error('You have uncommitted changes. Please commit or stash '
                    'them before rolling.')
        sys.exit(1)


def _parse_current_versions(
        readme_path: str = README_PATH) -> Optional[ProtobufVersion]:
    """Parses README.chromium to extract the current version and revision.

    Args:
        readme_path: Path to the README.chromium file.

    Returns:
        A ProtobufVersion object if found, otherwise None.
    """
    version = None
    revision = None

    if os.path.exists(readme_path):
        with open(readme_path, 'r') as f:
            for line in f:
                v_match = re.match(r'^Version:\s+(.*)', line)
                if v_match:
                    version = v_match.group(1).strip()
                r_match = re.match(r'^Revision:\s+(.*)', line)
                if r_match:
                    revision = r_match.group(1).strip()

    if version and revision:
        return ProtobufVersion(version, revision)
    return None


def _fetch_git_revision(version: str) -> str:
    """Fetches the exact git commit revision for a given GitHub tag version.

    Args:
        version: The semantic version string (e.g. '33.1').

    Returns:
        The full SHA hash of the commit.
    """
    print_substep(f'Fetching remote revision hash for v{version}...')
    result = _run_cmd(
        ['git', 'ls-remote', PROTOBUF_GITHUB_URL, f'tags/v{version}^{{}}'],
        capture_output=True)
    revision = result.stdout.split()[0] if result.stdout else ''
    if not revision:
        print_error(f'Could not find git revision for tag v{version}.')
        sys.exit(1)
    return revision


def _clean_protobuf_directory(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Removes the existing protobuf directory.

    Args:
        protobuf_dir: The target directory to clean.
    """
    print_step('2', 'Cleaning existing protobuf directory')
    print_substep('Nuking third_party/protobuf from orbit...')
    if os.path.exists(protobuf_dir):
        shutil.rmtree(protobuf_dir)


def _download_and_extract_release(version: str,
                                  protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Downloads the protobuf release tarball and extracts it.

    Args:
        version: The protobuf version to download.
        protobuf_dir: The target directory for extraction.
    """
    print_step('3', 'Downloading and extracting release')
    print_substep(f'Downloading v{version} tarball from GitHub...')
    os.makedirs(protobuf_dir, exist_ok=True)
    url = (f'https://github.com/protocolbuffers/protobuf/archive/'
           f'refs/tags/v{version}.tar.gz')
    tar_path = f'protobuf-{version}.tar.gz'

    if not curlish.curlish(url, tar_path):
        print_error(f'Failed to download {url}')
        sys.exit(1)

    try:
        print_substep('Extracting archive contents...')
        with tarfile.open(tar_path, 'r:gz') as tar:
            members = []
            for member in tar.getmembers():
                parts = member.name.split('/', 1)
                if len(parts) > 1:
                    member.name = parts[1]
                    members.append(member)
            if hasattr(tarfile, 'data_filter'):
                tar.extractall(path=protobuf_dir,
                               members=members,
                               filter='data')
            else:
                tar.extractall(path=protobuf_dir, members=members)
    finally:
        os.remove(tar_path)


def _prune_unused_language_bindings(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Removes language bindings and build systems not used by Open Screen.

    Args:
        protobuf_dir: The target directory to prune.
    """
    print_step('4', 'Pruning unused language bindings and build systems')
    unused_dirs = [
        '.github', 'bazel', 'csharp', 'java', 'objectivec', 'php', 'ruby',
        'rust'
    ]
    for d in unused_dirs:
        p = os.path.join(protobuf_dir, d)
        if os.path.exists(p):
            print_substep(f'Removing {d}/')
            shutil.rmtree(p)


def _restore_chromium_specific_files(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Restores chromium-specific build configurations from git history.

    Args:
        protobuf_dir: The target protobuf directory.
    """
    print_step('5', 'Restoring OpenScreen/Chromium specific files')
    files_to_restore = [
        'BUILD.gn', 'DEPS', 'DIR_METADATA', 'OWNERS', 'README.chromium',
        'gen_extra_chromium_files.py', 'proto_library.gni',
        'proto_sources.gni', 'patches/'
    ]
    paths = [os.path.join(protobuf_dir, f) for f in files_to_restore]
    print_substep('Restoring from HEAD...')
    _run_cmd(['git', 'checkout', 'HEAD', '--'] + paths)

    compat_dir = os.path.join(protobuf_dir, 'compatibility')
    if os.path.exists(compat_dir):
        print_substep('Removing compatibility/ directory...')
        shutil.rmtree(compat_dir)


def _apply_patches(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Applies local patches to the upstream protobuf source.

    Args:
        protobuf_dir: The target protobuf directory.
    """
    print_step('6', 'Applying upstream patches')
    patches_dir = os.path.join(protobuf_dir, 'patches')
    for patch in sorted(glob.glob(os.path.join(patches_dir, '*.patch'))):
        print_substep(f'Applying {os.path.basename(patch)}...')
        with open(patch, 'r') as f:
            try:
                _run_cmd(['patch', '-s', '-p1'], cwd=protobuf_dir, stdin=f)
            except subprocess.CalledProcessError:
                print_error(f'Patch {patch} failed to apply!')
                sys.exit(1)

    print_substep('Cleaning up .orig backup files...')
    for root, _, files in os.walk(protobuf_dir):
        for file in files:
            if file.endswith('.orig'):
                os.remove(os.path.join(root, file))


def _generate_descriptor_files(build_dir: str,
                               protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Runs the python script to generate pb2 files.

    Args:
        build_dir: The output build directory (e.g., 'out/Default').
        protobuf_dir: The target protobuf directory.
    """
    print_step('7', 'Generating python bindings')
    print_substep('Generating descriptor_pb2.py and plugin_pb2.py...')
    _run_cmd(['./gen_extra_chromium_files.py', '-C', f'../../{build_dir}'],
             cwd=protobuf_dir)


def _skip_bazel_generation_for_python(
        protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Creates stub python files to avoid requiring bazel to generate them.

    Args:
        protobuf_dir: The target protobuf directory.
    """
    print_step('8', 'Configuring Python edition defaults')
    print_substep('Skipping bazel generation (creating stub files)...')
    internal_dir = os.path.join(protobuf_dir, 'python', 'google', 'protobuf',
                                'internal')
    os.makedirs(internal_dir, exist_ok=True)
    open(os.path.join(internal_dir, 'python_edition_defaults.py'), 'a').close()


def _replace_in_file(path: str, old_str: str, new_str: str) -> None:
    """Helper to perform inline string replacement within a file.

    Args:
        path: Target file path.
        old_str: The string to be replaced.
        new_str: The string to replace with.
    """
    if not os.path.exists(path):
        return
    with open(path, 'r') as f:
        content = f.read()
    content = content.replace(old_str, new_str)
    with open(path, 'w') as f:
        f.write(content)


def _update_version_identifiers(current: ProtobufVersion,
                                target: ProtobufVersion,
                                protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Updates version numbers and revision SHAs in metadata files.

    Args:
        current: The old ProtobufVersion.
        target: The new ProtobufVersion.
        protobuf_dir: The target protobuf directory.
    """
    print_step('9', 'Updating version identifiers')
    readme_path = os.path.join(protobuf_dir, 'README.chromium')

    print_substep(
        f'Updating README.chromium ({current.version} -> {target.version})...')
    _replace_in_file(readme_path, current.version, target.version)
    _replace_in_file(readme_path, current.revision, target.revision)

    files_to_update = [
        'version.json', 'protobuf_version.bzl',
        'python/google/protobuf/__init__.py'
    ]
    for f in files_to_update:
        print_substep(f'Updating {f}...')
        _replace_in_file(os.path.join(protobuf_dir, f), current.version,
                         target.version)


def _validate_patches(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Ensures patches listed in README.chromium match the patches/ directory.

    Args:
        protobuf_dir: The target protobuf directory.
    """
    print_step('10', 'Validating patches against README.chromium')
    readme_patches: Set[str] = set()
    readme_path = os.path.join(protobuf_dir, 'README.chromium')
    with open(readme_path, 'r') as f:
        for line in f:
            m = re.match(r'^- (\d{4}-.*\.patch)$', line)
            if m:
                readme_patches.add(m.group(1))

    patches_dir = os.path.join(protobuf_dir, 'patches')
    local_patches = set(
        os.path.basename(p)
        for p in glob.glob(os.path.join(patches_dir, '*.patch')))

    missing_in_readme = local_patches - readme_patches
    missing_on_disk = readme_patches - local_patches

    if missing_in_readme or missing_on_disk:
        print_error('Patch validation failed! README.chromium patch list does '
                    'not match patches/ directory.')
        if missing_in_readme:
            print_error('The following patches exist in patches/ but are '
                        'missing from README.chromium:')
            for p in sorted(missing_in_readme):
                print(f'  {p}', file=sys.stderr)
        if missing_on_disk:
            print_error('The following patches are listed in README.chromium '
                        'but do not exist in patches/:')
            for p in sorted(missing_on_disk):
                print(f'  {p}', file=sys.stderr)
        sys.exit(1)

    print_substep('Patch validation passed successfully.')


def _print_usage_and_exit(parser: argparse.ArgumentParser,
                          current: Optional[ProtobufVersion]) -> None:
    """Prints tool usage and available upgrades, then exits.

    Args:
        parser: The argparse parser object.
        current: The currently installed ProtobufVersion, if available.
    """
    current_v = current.version if current else 'Unknown'
    current_r = current.revision if current else 'Unknown'
    print(f'Current protobuf version: {current_v} '
          f'(Revision: {current_r})\n')
    parser.print_usage()
    print('')
    print('Recent potential upgrades (from GitHub tags):')
    for tag in _get_available_versions():
        print(f'  {tag}')
    sys.exit(1)


def _parse_arguments() -> Tuple[argparse.ArgumentParser, argparse.Namespace]:
    """Sets up the argument parser and parses command-line arguments.

    Returns:
        A tuple of (parser, parsed_args).
    """
    parser = argparse.ArgumentParser(
        description='Automates rolling third_party/protobuf.',
        epilog=('You MUST specify a target version. Rolling to an arbitrary '
                'new version (e.g. 34.1) without manually updating the '
                'patches/ directory will likely result in patch failures.'))
    parser.add_argument('-C',
                        '--build-dir',
                        default='out/Default',
                        help='Specify the build directory. '
                        'Defaults to "out/Default".')
    parser.add_argument('version',
                        nargs='?',
                        help='Target protobuf version (e.g., 33.1).')

    return parser, parser.parse_args()


def _verify_environment(protobuf_dir: str = PROTOBUF_DIR) -> None:
    """Ensures we are in the correct repository and the working tree is clean.

    Args:
        protobuf_dir: The target protobuf directory.
    """
    repo_root = _get_repo_root()
    os.chdir(repo_root)

    if not os.path.isdir(protobuf_dir):
        print_error(f'Could not find {protobuf_dir}. '
                    'Are you in the openscreen repository?')
        sys.exit(1)

    _check_working_tree_is_clean()


def _resolve_and_verify_versions(
        args: argparse.Namespace,
        parser: argparse.ArgumentParser,
        readme_path: str = README_PATH
) -> Tuple[ProtobufVersion, ProtobufVersion]:
    """Resolves current/target versions and revisions, exiting if invalid.

    Args:
        args: The parsed command-line arguments.
        parser: The argument parser object.
        readme_path: Path to the README.chromium file.

    Returns:
        A tuple of (current, target) ProtobufVersion objects.
    """
    current = _parse_current_versions(readme_path)

    if not args.version:
        _print_usage_and_exit(parser, current)

    target_version = args.version

    print_step('1', 'Identifying and verifying versions')
    if not current:
        print_error('Could not determine current version/revision from '
                    'README.chromium')
        sys.exit(1)

    print_substep(f'Current Version: {current.version}')
    print_substep(f'Target Version:  {target_version}')

    target_revision = _fetch_git_revision(target_version)
    target = ProtobufVersion(version=target_version, revision=target_revision)
    return current, target


def main() -> None:
    """Main entry point for the protobuf roll script."""
    parser, args = _parse_arguments()

    _verify_environment()

    current, target = _resolve_and_verify_versions(args, parser)

    _clean_protobuf_directory()
    _download_and_extract_release(target.version)
    _prune_unused_language_bindings()
    _restore_chromium_specific_files()
    _apply_patches()
    _generate_descriptor_files(args.build_dir)
    _skip_bazel_generation_for_python()
    _update_version_identifiers(current, target)
    _validate_patches()

    print_success('Roll complete!')


if __name__ == '__main__':
    main()
