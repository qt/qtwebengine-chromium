---
name: utr
description: >-
  UTR (Universal Test Runner) is a tool to locally compile and/or run tests the
  same way it's done on Chromium CI/try builders (aka "bots"). It's particularly
  useful for reproducing bot failures or running tests on platforms you don't
  have access to locally.
---

## Basic Usage

The UTR tool is located at `tools/utr/run.py`. Always run it with `vpython3`.

When operating in remote or non-interactive workspace environments like Cider G,
ensure `depot_tools` is in your `PATH` and authenticate before invoking UTR:

```sh
luci-auth login -scopes https://www.googleapis.com/auth/userinfo.email
vpython3 tools/utr/run.py -B <bucket> -b <builder> -t <test_suite> <action>
```

### Actions

- `compile`: Only compile the targets.
- `test`: Only run the tests (assumes already compiled).
- `compile-and-test`: Compile and then run tests.

*Note: For `test` and `compile-and-test` actions, UTR will automatically track
and wait for all distributed Swarming shards to complete execution on the bot
farm and output the final aggregated results URL before exiting.*

### Common Flags

- `-B <bucket>`: The bucket name (e.g., `ci` or `try`).
- `-b <builder>`: The builder name (e.g., `Linux Tests`, `Win10 Tests x64`).
- `-t <test_suite>`: The test suite to run (e.g., `viz_unittests`, `url_unittests`).
- `--build-dir <dir>` or `-o <dir>`: The build directory to use for compiling
  and invoking test targets. Will use a build dir in `//out/` named after the
  builder if not specified: `//out/UTR${{builder_name}}`
- `--force` or `-f`: Skip all prompts about config mismatches especially useful
  when cross-compiling or using a custom build directory.
- `-n N`: Runs the build/test command N times without cleaning the build dir,
  and exits on the first failure.
- `--no-rbe`: Disables remote execution (RBE) and forces local compilation.
- `--`: Any args after this will be passed directly to the test executable.

## Examples and Advanced Usage

More information including examples with builder names and advanced usage can be
found at
[tools/utr/README.md](https://chromium.googlesource.com/chromium/src/+/main/tools/utr/README.md).

Information about cross-compiling Windows targets on Linux can be found at
[docs/win_cross.md](https://chromium.googlesource.com/chromium/src/+/main/docs/win_cross.md).

## Troubleshooting in Non-Interactive Environments

When running UTR inside non-interactive remote sessions, you may encounter
BeyondCorp / Context Aware Access (CAA) authentication blockers, missing remote
`.cipd_bin/` packages, `.gclient` toolchain mismatches, or RBE CAS syncing
issues:

1. **Explicit Re-authentication:** If fetching binaries or updating datasets
   stalls or raises an authentication failure, explicitly generate a fresh
   Context Aware Access token in the terminal:
   ```sh
   luci-auth login -scopes https://www.googleapis.com/auth/userinfo.email
   ```
2. **Forcing Narrow Execution Scope:** Avoid broad isolation failures by always
   supplying specific test targets and the force flag:
   ```sh
   vpython3 tools/utr/run.py --force -t <test_suite> -p chromium -B try -b <builder> compile
   ```
3. **Missing Target OS Toolchains (e.g., Android/iOS):** If GN generation fails
   with `Missing native Android toolchain support` (or similar `target_os`
   assertions), ensure your workspace's `.gclient` configuration includes the
   necessary platform in `target_os` and run `gclient sync`:
   ```python
   solutions = [
     ...
   ]
   target_os = ["linux", "android"]
   ```
4. **Missing CAS Inputs on Remote Workers (RBE Failures):** If remote
   compilation fails because RBE cloud workers cannot find local Cog virtual
   files (e.g., `build/util/LASTCHANGE.dummy`), pass `--no-rbe` to force local
   execution where the files are successfully present:
   ```sh
   vpython3 tools/utr/run.py --no-rbe --force -p chromium -B ci -b <builder> -t <test> compile-and-test
   ```

## Guidelines for AI Agents (User Interaction & Prompts)

When invoking UTR as an AI agent, you must follow these guidelines to ensure a
safe and smooth user experience:

### 1. Present Warnings Before Prompts

If UTR stops and prompts for input (e.g., expecting `y` to continue or `i` to
ignore), **DO NOT** simply ask the user "Should I proceed?".

1. Read the UTR command logs to identify the specific warning messages (e.g.,
   `.gclient configuration mismatches (missing target_os = ["win"])` or
   cross-compilation warnings).
2. Present these warnings clearly to the user in the chat.
3. Ask the user for their decision **after** they have seen the warnings, so
   they know exactly what they are agreeing to.

### 2. Smart Use of the `--force` Flag

The `--force` (or `-f`) flag bypasses all prompts. To avoid annoying the user
with repeated prompts while also ensuring they see important warnings:

- **First Run:** **NEVER** use the `--force` flag on the initial UTR run unless
  the user has already explicitly authorized it. Run UTR normally so that any
  configuration mismatches or warnings are caught and prompted.
- **Subsequent Runs:** If you need to run UTR multiple times (e.g., during
  iterative debugging or multiple test runs) and the user **has already accepted
  all the warnings** in a previous run, you **SHOULD** append the `--force` flag
  to all subsequent UTR runs. This prevents the user from having to approve the
  same warnings repeatedly.
- **Safety Constraint:** **ONLY** use `--force` if the user has already seen and
  accepted *all* the warnings. Never use it to preemptively override warnings
  that the user has not yet approved at least once.
