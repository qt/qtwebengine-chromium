# What's in this folder?

The `streaming_schema.json` file in this directory contains a
[JsonSchema](https://json-schema.org/) for the validation of control messaging
defined in the Cast Streaming Control Protocol. This includes comprehensive
rules for message definitions as well as valid values and ranges. Similarly,
the `receiver_schema.json` file contains a JsonSchema for validating receiver
control messages, such as `LAUNCH` and `GET_APP_AVAILABILITY`, and their
corresponding receiver response messages.

## Example Validation

The `validate_examples.sh` runs the control protocol against a wide variety of
example files in this directory, one for each type of supported message in the
streaming and the receiver schema definitions. In order to see what kind of
validation this library provides, you can modify these example files and see
what kind of errors this script presents.

### yajsv installation

NOTE: this script requires [`yajsv`](https://github.com/neilpa/yajsv), a JSON
schema validator, in order to run. Follow the link to see latest installation
instructions.

### Example validation

For example, if we modify the launch.json to not have a language field, the
script will fail and print an error message indicating that the `language` field
is required.

## Updating schemas

When updating the JSON schema files, take care to ensure consistent formatting.
Since `clang-format` doesn't support JSON files (currently only Python, C++,
and JavaScript), the JSON files here are instead formatted using
[json-stringify-pretty-compact](https://github.com/lydell/json-stringify-pretty-compact)
with a max line length of 80 and a 2-character indent. Many IDEs have an
extension for this, such as VSCode's
[json-compact-prettifier](https://marketplace.visualstudio.com/items?itemName=inadarei.json-compact-prettifier).

You can also use `npx` to run the formatter from the command line:

```bash
npx json-stringify-pretty-compact --maxLength 80 --indent 2 my_schema.json
```
