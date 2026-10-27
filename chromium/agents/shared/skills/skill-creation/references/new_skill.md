# Adding New Skills

## Choose A Suitable Location

Skills can technically live anywhere, but should generally be kept in
standardized locations for discoverability and compatibility with supporting
tooling such as setup scripts.

### Determine Visibility (Public vs. Internal)

Since Chromium is an open source project, skills should be public by default and
only made internal (Googler-only) when strictly necessary. The most common
reason why this may be necessary is if the skill’s behavior relies on
Google-only tools or services.

### Determine Scope (Shared vs. Repo-specific vs. Private)

Skills can be shared across multiple repos using the public
[chromium/agents repo](https://chromium.googlesource.com/chromium/agents/+/refs/heads/main)
and its internal equivalent. However, skills that are only relevant to a
specific repo should be stored in that repo instead of in the shared repo.
Additionally, skills that are only relevant to a particular user should not be
landed in any repo and instead kept private on their local disk.

### Decision Table

The following table summarizes the decisions made above.

| | | | | | :------- | :--------------------- |
:--------------------------------- | :--------- | | | Shared | Repo-specific |
Private | | Public | chromium/agents | Specified repo (e.g. chromium/src) |
Local disk | | Internal | chrome/agents-internal | Specified repo (e.g.
src-internal) | Local disk |

## Set Up The Directory Structure

Every skill must follow this directory structure:

```
skill-name/
├── SKILL.md              # Required: Metadata + core instructions (<500 lines)
├── scripts/              # Executable code (Python/Bash) designed as tiny CLIs
├── references/           # Supplementary context (schemas, cheatsheets)
└── assets/               # Templates or static files used in output
```

- **SKILL.md**: Entry point for the skill. Should be kept relatively short, but
  can reference other resources for the agent to load when relevant.
- **References**: Additional context that is linked directly from SKILL.md. Keep
  them one level deep only, i.e. don’t create any subdirectories under this
  directory.
- **Scripts**: Use for fragile/repetitive operations where variation is a bug.
  Do not bundle library code here; long-lived library code belongs in standard
  repo CLI directories.

### Set Up SKILL.md

The SKILL.md file is made up of two components: the frontmatter, which is
written in YAML, and the body, which is written in Markdown. The frontmatter is
always loaded by the agent and contains the information it uses to decide when
to lazily load the body of the rest of the skill. The body is the initial
content loaded by the agent when the skill is loaded, although the agent may
choose to load additional context referred to by the body.

The frontmatter is separated by a starting and ending --- on their own lines,
resulting in file content that looks along the lines of

```
---
name: cl-description
description: >-
  Use this skill to draft, write, or format a Changelist (CL) description or
  commit message strictly following Chromium's guidelines.
---
<Arbitrary Markdown content>
```

The name and description in the frontmatter of your SKILL.md are the only fields
that the agent sees before triggering a skill and are subject to several
restrictions.

- **Strict Naming**: The name field must be 1-64 characters, contain only
  lowercase letters, numbers, and hyphens (no consecutive hyphens), and must
  exactly match the parent directory name (e.g., name: wiz-testing must live in
  wiz-testing/SKILL.md).
- **Trigger-Optimized Descriptions**: (Max 1,024 characters). Describe the
  capability in the third person and include specific triggers and “negative
  triggers”. Cannot contain XML tags.
  - Bad: “Wiz skills.” (Too vague).
  - Good: “Creates and builds Wiz components using external Sass. Use when the
    user wants to update component styles or build configurations. Don't use it
    for external Sass styles with other frameworks.”

## Create Evals

While not strictly required yet, the best practice when landing a new skill is
to create one or more prompt eval tests that will exercise your skill. This will
help to ensure that your skill continues to work in the long term as agents
evolve over time. However, this is currently only supported for skills that live
in either Chromium itself or the src-internal repo. Additionally, the tests
currently only run on Linux.

General documentation on Chromium’s prompt eval testing is available under
[//agents/testing](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/agents/testing/).
Reading through that is heavily encouraged, but a brief summary of how to add
tests for skills is:

1. Create a new subdirectory for your skill under
   [//agents/prompts/eval](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/agents/prompts/eval/)
2. Add one or more .promptfoo.yaml files in the new directory which will be used
   to define your test(s)
3. Populate the .promptfoo.yaml files. The exact contents will vary depending on
   what is being tested, but existing .promptfoo.yaml files under
   //agents/prompts/eval should act as good examples to follow
   1. Ensure that your skill is actually loaded by including it in the skills
      list for the test, e.g.

```
providers:
  - id: python:../../../testing/gemini_provider.py
    config:
      skills:
        - my-skill

```

Once added, your test should be automatically picked up by the test runner.
Consult the documentation in //agents/testing on how to run them locally or use
the linux-prompt-evals trybot on your CL.
