# Gerrit CLI Workflows (Public)

## 1. Change List Review Workflow

Execute these steps in chronological order to review a Change List (CL) when
provided with a link or ID:

1. **Query CL Metadata**: Retrieve CL details and verify its current status:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     changes \
     --host https://chromium-review.googlesource.com \
     "change:<id>"
   ```
2. **Fetch File Contents**: Retrieve the content of specific files modified in
   the CL:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     content \
     --host https://chromium-review.googlesource.com \
     --project <project> \
     <change_id> current <file_path>
   ```
3. **Analyze Local Modifications**: Inspect the fetched contents and local diffs
   for any logic or formatting issues.
4. **Add Review Comment**: Post a patchset-level comment containing the review
   findings:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     addpatchsetcomment \
     --host https://chromium-review.googlesource.com \
     --message "Review findings: ..." \
     <change_id> current
   ```

## 2. Shepherding & Submission Workflow

Execute these steps in chronological order to approve and merge a Change List:

1. **Check Related Changes**: Retrieve the status of all related changes to
   ensure there are no unresolved blockers:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     relatedchanges \
     --host https://chromium-review.googlesource.com \
     <change_id> current
   ```
2. **Vote on Review Labels**: Apply the required Code-Review approval vote:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     setlabel \
     --host https://chromium-review.googlesource.com \
     --label "Code-Review" \
     --value 1 \
     <change_id>
   ```
3. **Submit Change**: Merge the change once all CQ presubmits pass and all
   approval requirements are satisfied:
   ```bash
   python3 agents/shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py \
     submitchange \
     --host https://chromium-review.googlesource.com \
     <change_id>
   ```
