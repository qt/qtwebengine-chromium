# Skill Authoring Best Practices

## Use Progressive Disclosure and Consistent Resource Management

Maintain a pristine context window by loading information only when needed.

- **Keep SKILL.md Lean**: Limit the main file to \<500 lines. Use it for
  navigation and primary procedures.
- **Use Flat Subdirectories**: Move bulky context to standard folders. Keep
  files exactly one level deep (e.g., `references/schema.md`, not
  `references/db/v1/schema.md`).
- **Keep references one level deep** from SKILL.md to ensure complete file
  reads.
- **Just-in-Time (JiT) Loading**: Explicitly instruct the agent when to read a
  file (e.g., “See `references/auth-flow.md` for specific error codes” or “run
  `cl-tool -h` to get detailed usage”).
- **Explicit Pathing**: Always use relative paths with forward slashes (`/`),
  regardless of the OS. Avoid Windows-style paths.
- **For Agents, Not Humans**: Omit README.md, CHANGELOG.md, or installation
  guides. Delete redundant logic.
- **Self-Contained**: A skill should be entirely self-contained within its
  directory. Maintain isolation by keeping all files internal to the skill.

## Write Instructions For Agents

Create instructions for LLMs instead of humans.

- **Tone and Style**:
  - **Positive Framing**: Focus on what the agent *should* do. If you must
    specify a constraint, provide an alternative.
    - *Bad (Negative)*: “Do not use Python 2.”
    - *Good (Positive)*: “Always use Python 3 for new scripts.”
  - **Third-Person Imperative**: Frame instructions as direct commands (e.g.,
    “Extract the text...” rather than “I will extract...”).
- **Procedural Instructions**:
  - **Step-by-Step Numbering**: Define workflows as strict chronological
    sequences.
  - **Degrees of Freedom**: Match specificity to task fragility. Use low freedom
    (exact commands) for fragile operations like DB migrations, and high freedom
    for context-dependent tasks like code reviews.
- **Precision and Terminology**:
  - Use identical terminology consistently.
  - Use the most specific terminology native to the domain (e.g., “template”
    instead of “html” in Angular).
- **Templates and Examples**:
  - Provide concrete templates in assets/ and instruct the agent to copy them.
  - Provide input/output pairs to guide style and detail.
- **Intentional Modifications**: Only modify a skill when there is a concrete
  functional reason to do so. Avoid minor rephrasing, cosmetic cleanups, or
  no-op changes.

## Make Use Of Deterministic Scripts

Do not write instructions in SKILL.md that require the agent to generate complex
parsing logic or boilerplate code from scratch.

- **Offload Fragile Tasks**: Create tested Python scripts in scripts/ for
  operations like parsing complex datasets or querying databases. Instruct the
  agent to execute these scripts rather than generating code. Avoid
  platform-specific scripts such as bash or batch.
- **Graceful Error Handling**: Ensure scripts return highly descriptive,
  human-readable error messages on stdout/stderr so the agent can self-correct
  based on the output.
- **Avoid Voodoo Constants**: Document and justify configuration parameters
  within the scripts or references.
