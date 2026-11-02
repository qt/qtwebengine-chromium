---
name: json-schema-compiler
description: Helps developers work with Chromium Extension API schemas. Use this skill when editing JSON or IDL files in the extensions API directories to compile schemas, generate C++ types, or validate changes.
---

# JSON Schema Compiler Skill

This skill assists with compiling and validating Chromium Extension API schemas
(JSON and IDL files) using the JSON Schema Compiler tool.

## Scope of Skill

Use this skill when the user asks to:

1.  "Compile the schema", "generate types", "generate bindings", or "generate
    externs" for an Extension API.
2.  "Validate my extension API changes" or "check schema syntax/formatting".
3.  Work on files located in:
    -   `chrome/common/extensions/api/`
    -   `extensions/common/api/`
    -   Other extension API schema locations in the Chromium source tree.

## Underlying Tool

The JSON Schema Compiler tool in Chromium is located at
`tools/json_schema_compiler/compiler.py`. The compiler command must be run from
the Chromium `src` root, invoking it with `vpython3` (from `depot_tools`).

### Common Generators (`-g` or `--generator`)

-   `cpp` (default): Generates C++ headers (`.h`) and implementation (`.cc`)
    files containing type definitions and deserialization code.
-   `externs`: Generates Javascript externs (`.js`) used for Closure compiler
    type-checking.
-   `interface`: Generates JS interface definition files.
-   `ts_definitions`: Generates TypeScript definition files (`.d.ts`).
-   `cpp-bundle-registration`: Generates bundle registration C++ files.
-   `cpp-bundle-schema`: Generates bundled C++ schema definitions.

--------------------------------------------------------------------------------

## Usage

### 1. Verification via Wrapper Script (Recommended)

To run the JSON Schema Compiler by automatically detecting paths and namespace
settings, use the helper script `compiler_wrapper.py`.

```bash
python3 scripts/compiler_wrapper.py --file <path_to_schema> --generator <generator_type>
```

The helper script is located relative to the skill's root directory:
`scripts/compiler_wrapper.py`.

#### Examples:

-   **C++ Types (to stdout):**

    ```bash
    python3 scripts/compiler_wrapper.py --file extensions/common/api/system_display.idl --generator cpp
    ```

-   **C++ Types (to directory):**

    ```bash
    python3 scripts/compiler_wrapper.py --file extensions/common/api/system_display.idl --generator cpp --destdir out/Default/gen
    ```

-   **JS Externs (to file):**

    ```bash
    python3 scripts/compiler_wrapper.py --file extensions/common/api/system_display.idl --generator externs > third_party/closure_compiler/externs/system_display.js
    ```

### 2. Direct Invocation of compiler.py

Commands must be run from the Chromium `src` root:

```bash
vpython3 tools/json_schema_compiler/compiler.py --root . --namespace extensions --generator <generator> <schema_file>
```

#### Direct Gen Examples:

-   **Generating C++ types:**

    ```bash
    vpython3 tools/json_schema_compiler/compiler.py --root . --namespace extensions --generator cpp extensions/common/api/system_display.idl
    ```

-   **Generating C++ to a specific build gen directory:**

    ```bash
    vpython3 tools/json_schema_compiler/compiler.py --root . --destdir out/Default/gen/extensions/common/api --namespace extensions --generator cpp extensions/common/api/system_display.idl
    ```

-   **Generating JS externs for Closure Compiler:**

    ```bash
    vpython3 tools/json_schema_compiler/compiler.py --root . --namespace extensions --generator externs extensions/common/api/system_display.idl > third_party/closure_compiler/externs/system_display.js
    ```

### 3. Validating and Compiling with GN and Autoninja

When schemas are added, modified, or deleted, verify the changes by compiling
the corresponding target using `autoninja`:

-   **For schemas under `chrome/common/extensions/api/`:**

    ```bash
    autoninja -C out/Default chrome/common/extensions/api:generated_api_types
    autoninja -C out/Default chrome/common/extensions/api:generated_api_json_strings
    ```

-   **For schemas under `extensions/common/api/`:**

    ```bash
    autoninja -C out/Default extensions/common/api:generated_api_types
    autoninja -C out/Default extensions/common/api:generated_api_json_strings
    ```

--------------------------------------------------------------------------------

## Errors and Troubleshooting

-   **Validation / Syntax Failure:** The JSON and IDL parsers are strict. If
    there is a syntax error (e.g., missing comma, invalid comment format, or
    unsupported IDL features), the python compiler script will output a
    traceback or validation error to standard error. Pay close attention to
    references or missing properties.
-   **Header Include Mismatches:** C++ compilation of generated files depends on
    referenced schemas. If you reference another schema type (e.g. `tabs.Tab`),
    verify that both schemas are listed as dependencies in the relevant
    `BUILD.gn` file.
-   **JSON format limits:** JSON files must follow standard JSON but can include
    `//` comments which `compiler.py` strips automatically. Avoid trailing
    commas or other non-JSON formats in other sections.
