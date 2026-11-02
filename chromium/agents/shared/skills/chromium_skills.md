# Chromium Skills

## Overview

Skills are a type of resource for AI agents that are commonly supported across
different agentic tools. They are essentially a bundle of additional Markdown
context or simple scripts that are automatically and lazily loaded if the agent
believes that they are relevant for the current task based on the short
description of the skill.

## Installation

All publicly available skills for Chromium live under the
[//agents/skills directory](https://source.chromium.org/chromium/chromium/src/+/main:agents/skills/)
and
[//agents/shared/skills directory](https://source.chromium.org/chromium/chromium/src/+/main:agents/shared/skills/).
For Googlers, any available internal skills live under the same location in the
src-internal repo. Skills for other repos should also typically be under an
//agents/skills directory, although whether any exist will be highly dependent
on the specific repo.

While skills are meant to provide additional context while taking up relatively
little of the context window when not needed, each skill does add a small amount
of context that your agent will always load. Thus, it is recommended that you
only install skills that you might actually use on occasion instead of
installing all available skills.

Note that while the instructions provided here are for tools that are officially
supported by the Chromium Infrastructure teams, most common agents support
skills. So, Chromium’s skills will likely work as-is, but you will need to
consult the relevant external documentation for installation instructions.

### Tool Setup Scripts (Google-only)

Googlers have access to setup scripts for any supported tools which include
automatic detection of available skills and allow users to select which ones to
install or remove. Since other setup steps can be skipped if desired and running
these scripts gives users a chance to pick up new recommended settings, this is
the recommended approach for Googlers.

### Gemini CLI Install Script

A public install script exists for Gemini CLI skills at
[//agents/skills/setup.py](https://source.chromium.org/chromium/chromium/src/+/main:agents/skills/setup.py).
This will automatically detect any installed or available skills and simplify
their management. This is the recommended approach for non-Googlers who are
using Gemini CLI.

### Manual Installation

Manual installation of skills is still relevant for certain use cases such as
highly personalized skills that will not be landed in Chromium. For manual
installation instructions, please refer to the relevant external documentation
for the most up-to-date information.

- [Gemini CLI](https://geminicli.com/docs/cli/skills/)
- [Antigravity](https://antigravity.google/docs/skills)

## Skill Creation

Most of the documentation for skill creation and modification is available as
part of
[Chromium’s skill creation skill](https://source.chromium.org/chromium/chromium/src/+/main:agents/shared/skills/skill-creation/SKILL.md).
Documentation is largely kept there in order to help prevent multiple copies of
similar documentation from getting out of sync.

If you just want to create a new skill, installing that skill and asking your
agent to create a skill for you will likely be sufficient. However, it may be
worth reading the documentation yourself in order to get a better understanding
of what goes on under the hood.

A common skill creation pattern that has yielded good results in the past is to
have an agent perform a task normally. Once it is able to complete that task
successfully, you can then ask it to document its workflow as a skill for reuse.
This will usually act as a good starting point, but will also likely require
some iteration.

### Differences

While the documentation in the skill creation skill is broadly correct, there
are a few inclusions that are solely meant for agents and can be ignored by
humans if writing or modifying a skill manually. The list of differences are as
follows:

- The best practices documentation notes that only intentional changes with
  concrete, functional differences should be made. This is included since agents
  have a habit of making unnecessary changes to wording, etc., but if you see
  minor changes to a skill that you believe would improve clarity or
  readability, feel free to make such changes

## External References

The following external references are not directly related to Chromium, but may
still serve as useful supplemental documentation around skills and best
practices for them.

- [Anthropic skill creation skill](https://github.com/anthropics/skills/blob/main/skills/skill-creator/SKILL.md)
- [Agent Skills Best Practices](https://agentskills.io/skill-creation/best-practices)
