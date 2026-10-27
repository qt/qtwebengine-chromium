---
name: time-travel-code-explorer
description: >-
  Explore older code for local repositories without needing to sync to a
  particular revision
---

# Time Travel Code Explorer

## Instructions

- Use this skill when you want to look at git repository content at a previous
  revision.
- Prefer this over raw `git` commands if you are looking at a non-HEAD revision
  and the functionality you need is supported
- Use `scripts/code_explorer.py` for handling the underlying `git` commands to
  do this.
- Refer to the `--help` output of the script if you need more details about
  arguments, etc.
- If you need to run this against a repository other than the one that the
  current working directory belongs to, specify the path to the other repo using
  `--cwd`.

## Examples

- Get the commit description and commit content for a revision:
  `scripts/code_explorer.py view_cl --revision 0bad974b43c404aee61bba2127b27b4cc51bb92b`

- Get the full content of the file `path/to/file` at a revision:
  `scripts/code_explorer.py view_file --revision 0bad974b43c404aee61bba2127b27b4cc51bb92b --path path/to/file`

- Get the full content of the file `path/to/file` in the repo under
  `third_party/skia` at a revision:
  `scripts/code_explorer.py view_file --revision 27aaf3d1921c074a0aaebf582136219d98302473 --path path/to/file --cwd third_party/skia`

- List the contents of `some/directory/` at a revision:
  `scripts/code_explorer.py list_dir --revision 0bad974b43c404aee61bba2127b27b4cc51bb92b --path some/directory`

- Search for the string `some_substring` in all files at a revision:
  `scripts/code_explorer.py search_files --revision 0bad974b43c404aee61bba2127b27b4cc51bb92b --query some_substring`
