# Build Configuration Format

The `build.yaml` file contains common build setup (Python packages, feature dependencies) and distro-specific scripts (setup, install, cross-compilation).

## fcenv File (Optional)

If `.gitlab-ci/fcenv` exists, build.py will load environment variables from it before setting defaults.

**Format**: Shell variable assignments (one per line)
```bash
VARIABLE_NAME="value"
ANOTHER_VAR=value
```

Variables from fcenv override build.py defaults.

## Structure

```yaml
common:
  packages:                 # Python packages to install (optional)
    - package1==version     # List of packages for pip install
    - package2

  features:                 # Feature-specific setup (optional)
    <feature_name>:
      env:                  # Environment variables to set (optional)
        VAR_NAME: "value"   # Supports ${VAR} expansion
      shell: sh|bash        # Shell to use (optional, defaults to 'sh')
      script: |             # Shell script to set up feature dependencies
        # commands here

<distro_name>:
  setup:
    env:                    # Environment variables to set (optional)
      VAR_NAME: "value"     # Supports ${VAR} expansion
    shell: sh|bash          # Shell to use (optional, defaults to 'sh')
    script: |               # Shell script to execute (optional)
      # commands here

  install:
    shell: sh|bash          # Shell to use (optional, defaults to 'sh')
    script: |               # Shell script to execute (optional)
      # commands here

  cross:
    shell: sh|bash          # Shell to use (optional, defaults to 'sh')
    script: |               # Shell script to execute (optional)
      # commands here
```

## Notes

- **common.packages**: Python packages installed via pip. build.py creates a venv and installs these packages automatically
- **common.features**: Optional per-feature setup (e.g., `fontations` needs bindgen-cli from cargo)
- **distro_name**: Must match the ID field from `/etc/os-release` (or platform name like `darwin` for macOS, `windows` for Windows)
- **null**: Use when no configuration is needed for a phase
- **empty script**: Use `script: ""` for phases that should do nothing
- **env**: Applicable for `setup` phase and `features`. Environment variables are set in Python and persist across the build
- **needs_cert_setup**: Set to `true` in a phase to automatically configure SSL certificate paths using `python -m certifi` (useful for macOS)
