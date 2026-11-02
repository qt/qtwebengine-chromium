# Instructions for Generating a Chromium Commit Message

As an expert Chromium developer and Gerrit expert, your goal is to analyze the
current session and generate a useful CL description. You understand that a
Chromium commit message is a permanent record of technical rationale and a
trigger for automated infrastructure.

### 1. Pre-flight Investigation (Interactivity)

Before generating the final draft, analyze the session history. If any of the
following are missing or ambiguous, **STOP and ask the user for clarification**:

- **The "Why":** If the technical rationale or motivation isn't explicitly clear
  from the session history.
- **Bug ID:** If no bug number was mentioned, ask if one should be associated.
- **Internal vs. Public Bug:** If a bug ID is present, confirm if it's a public
  Chromium bug or an internal Buganizer issue.
- **Manual Testing:** If no test commands were successfully run, ask the user
  how they verified the change to populate the `Test:` footer.
- **Histogram Obsoletion:** If non-expired UMA histograms are removed or
  renamed, and the obsoletion reason or replacement is unclear, ask the user.

### 2. Formatting Constraints (Mandatory)

- **72-Column Wrap:** Every line (Subject and Body) **MUST** be hard-wrapped at
  72 characters (exception: footer tags like `OBSOLETE_HISTOGRAM` must remain on
  a single line).
- **Subject Line:** A single, concise summary. Prefix it with the relevant
  component in brackets, e.g., `[Omnibox]: ...`. The entire subject line
  **MUST** be under 50 characters if possible, and no more than 72 characters.
- **Subject Spacing:** There **MUST** be exactly one blank line after the
  subject.
- **Footer Spacing:** There should be no blank lines within the footer block.
- **No Markdown-style Hyperlinks:** DO NOT use the markdown-style hyperlinks
  (e.g., `[link](url)`).

### 3. Body Content Requirements

- **Content over Code:** Do not just list what changed. Focus on **why** it was
  necessary.
- **Context:** Describe the "Before" (the problem/baseline) and the "After" (the
  solution/new behavior).
- **Documentation Links:** Do not include any links (e.g., to design docs or
  other CLs) unless specifically requested. Use the format
  `https://crrev.com/c/NUMBER` for Gerrit CL references.
- **Omit Boilerplate:** Omit tags like `RELNOTES` or `TESTED` unless
  specifically requested.

### 4. Critical Footer Logic

- **Internal Bugs (Buganizer):** MUST use the `b:` prefix. Example:
  `Bug: b:123456`.
  - **WARNING:** Never use the `b/123` format; it triggers OSS lint warnings.
- **Public Bugs:** Use the bare number. Example: `Bug: 123456`.
- **Bug: None:** IF NO BUG is associated with the session, **DO NOT** include a
  `Bug:` line at all. Do not write `Bug: None`.
- **Closing Bugs:** Use the `Fixed:` tag if the bug should be closed
  automatically.
- **Verification:** Populate the `Test:` footer with manual verification steps
  or the specific test suites run.
- **Histogram Obsoletion (UMA):** When removing or renaming non-expired
  histograms from `histograms.xml` (including patterned variants or suffixes),
  add obsoletion tags to the footer explaining why and when (YYYY-MM) they were
  removed, or what replaces them:
  - **Specific Histogram:** `OBSOLETE_HISTOGRAM[HistogramName]=message` (e.g.,
    `OBSOLETE_HISTOGRAM[Tab.Count]=Replaced by Tab.Count2`).
  - **All Removed Histograms:** `OBSOLETE_HISTOGRAMS=message` (used when all
    removed histograms share the same message, especially useful for patterned
    histograms; overridden by specific tags).
  - **Single Line:** The full tag **MUST** be on a single line, even if it
    exceeds 72 characters.
  - **Expired Histograms:** Can be omitted if the removed histogram was already
    expired.

______________________________________________________________________

## Final Message Template:

```
[Component] [Short summary of change (< 50 chars)]

[Description explaining the "Why" and "How". Focus on rationale,
previous behavior, and the impact of the change. Wrap this
block strictly at 72 characters. You can omit this body if the
diff is short and self-explanatory.]

Bug: [b:ID or ID]
Test: [Manual test commands or verification steps]
OBSOLETE_HISTOGRAM[HistogramName]=[Reason or replacement (single line)]
```
