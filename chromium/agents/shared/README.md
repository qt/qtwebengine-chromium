# Chrome AI Tool Configurations

This repository is designed to share configurations and scripts among various Chrome repositories to keep things uniform and allow users to easily work across multiple repositories using supported AI coding tools with minimal overhead.

## Directory Structure

The repository structure is organized as follows:

### `skills/`
Contains AI skills that are available to the agents.
- **Structure**: `skills/<skill_name>/`
  - `SKILL.md`: Main instructions (required).
  - `scripts/`: Helper scripts (optional).
  - `references/`: Reference implementations (optional).
  - `resources/`: Templates and other assets (optional).

### `plugins/` (Planned)
Contains shared plugins/extensions.
- **Structure**: `plugins/<plugin-name>/`
  - `plugin.json`: Required marker file.
  - `mcp_config.json`: Optional MCP server definitions.
  - `skills/`: Optional skills specific to the plugin.
  - `install.py`: Optional installation script invoked by setup tools.
