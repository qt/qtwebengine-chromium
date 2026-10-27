---
name: skill-creation
description: >-
  Use this skill to create new skills for agents or update existing ones.
  It includes documentation on best practices and Chromium-specific guidance.
---

# Skill Creation

## Overview

Skills are a type of resource for AI agents that are commonly supported across
different agentic tools. They are essentially a bundle of additional Markdown
context or simple scripts that are automatically and lazily loaded if the agent
believes that they are relevant for the current task based on the short
description of the skill.

## When To Use Skills

Skills are a useful tool for agents, but they are not universally applicable to
all situations. The following situations are not good fits for skills:

1. If you want to strictly enforce certain behaviors such as disallowing certain
   terminal commands. Because skills are effectively additional context, there
   is no guarantee that agents will strictly follow their instructions or even
   load them 100% of the time they are relevant. If you need to strictly enforce
   something, functionality such as the excludeTools field for Gemini CLI
   extensions are the correct solution.
2. If you are essentially trying to provide your agent access to an API. In this
   case, an MCP server is a more appropriate tool for the job.

On the other hand, some examples of appropriate uses for skills are:

1. Providing relevant information on how agents should format commit messages
   for a repo
2. Describing how to debug tests on physically attached test devices
3. Outlining how to remove a Chromium feature flag

## Skill Creation

Refer to `references/new_skill.md` for documentation on creating a new skill and
`references/best_practices.md` for documentation on best practices when creating
or updating a skill.
