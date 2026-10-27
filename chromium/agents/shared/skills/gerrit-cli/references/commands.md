# Gerrit CLI Common Commands & Usage (Public)

Run the tool directly using the wrapper script path with the `--help` flag:
- `python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py --help`
- `python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py changes --help`

## Global Flags

Use the following global flags for all commands:

- `--host <url>` (e.g., `--host https://chromium-review.googlesource.com`).
- `--project <project_name>` (e.g., `--project chromium/src`).
- `--json_file <path>` (saves the structured response as a JSON file).

______________________________________________________________________

## Common Invocations

### 1. Inspecting and Searching Changes

- **Query active changes:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    changes \
    --host https://chromium-review.googlesource.com \
    "owner:self status:open"
  ```
- **View the content of a specific file in a change:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    content \
    --host https://chromium-review.googlesource.com \
    --project <project> \
    --json_file output.json \
    <change_id> <revision> <file_path>
  ```
- **Get related changes:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    relatedchanges \
    --host https://chromium-review.googlesource.com \
    <change_id> <revision>
  ```

### 2. Reviewing and Voting

- **Add a patchset-level comment:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    addpatchsetcomment \
    --host https://chromium-review.googlesource.com \
    --message "Review findings: <message>" \
    <change_id> <revision>
  ```
- **Vote on a review label (e.g., Code-Review +1):**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    setlabel \
    --host https://chromium-review.googlesource.com \
    --label "Code-Review" \
    --value 1 \
    <change_id>
  ```

### 3. Actions and Shepherding

- **Submit/Merge a change:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    submitchange \
    --host https://chromium-review.googlesource.com \
    <change_id>
  ```
- **Abandon a change:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    abandon \
    --host https://chromium-review.googlesource.com \
    --message "<reason>" \
    <change_id>
  ```
- **Restore an abandoned change:**
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    restore \
    --host https://chromium-review.googlesource.com \
    --message "<reason>" \
    <change_id>
  ```

______________________________________________________________________

## Just-in-Time Help

Explore the built-in CLI help for additional subcommands or advanced syntax:

- Display all top-level commands:
  `python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py --help`
- Display help for a specific subcommand:
  ```bash
  python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
    help <command>
  ```
