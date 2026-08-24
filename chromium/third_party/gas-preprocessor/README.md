gas-preprocessor
================

This is a script for parsing GAS (GNU Assembler) files, expanding the
macros and similar directives into a linear sequence of instructions,
to allow building such files with assemblers that don't support the
GAS directives.

Currently, this tool is primarily useful for building GAS files with
MSVC, for ARM and ARM64. The tool can be used for building the
ARM/AArch64 assembly in projects like FFmpeg, dav1d, x264 and openh264.

Additionally, for ARM assembly, the tool can rewrite some instructions
that only can be assembled in ARM mode, into the corresponding sequences
of instructions that can be assembled in Thumb mode. This can allow
assembling sources that have been written without support for Thumb
mode for Windows (where Thumb mode is the default).

Earlier use cases
-----------------

Originally, the tool was made for building things for Apple platforms;
ARM and PowerPC assembly. Apple used to ship an ancient version of
GAS, which used a different macroing syntax than the modern GNU
assembler.

Since Apple have replaced the ancient version of GAS it with LLVM/Clang
based tooling, and LLVM's assembler support has improved to handle
most relevant directives, gas-preprocessor is no longer needed for
Apple platforms. Since Xcode 9.3 (LLVM 5.0), those tools support
everything needed for building FFmpeg's ARM assembly without
gas-preprocessor. (FFmpeg's AArch64 assembly never required
gas-preprocessor on Apple platforms.)

The main remaining platform where gas-preprocessor is needed, is for
MSVC tooling, to make GAS assembly files buildable with the MS
armasm/armasm64 assemblers.

Usage
-----

The projects listed above automatically try to use gas-preprocessor
when configuring for a toolchain and architecture where needed; it
doesn't need to be manually specified.
