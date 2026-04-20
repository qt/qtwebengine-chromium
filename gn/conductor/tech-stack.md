# Technology Stack

## Primary Language

*   **C++:** The core GN tool is written in C++.

## Build System
* The script `build/gen.py` regenerates ninja files.
* It generates two relevant targets - `gn` and `gn_unittests`
* Examples from the `examples` directory can be built with `gn gen` and then ran with `ninja`
* For the ultimate test of whether it works, you can use `gn` on a chromium checkout.

## Scripting

*  Scripting is done in either python or shell, whichever is easier.
