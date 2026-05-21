# Cross Compilation

Open Screen Library supports cross compilation using the `target_cpu` GN
argument. Since we use the same build tooling as the Chromium project, most
cross compiling scenarios supported in Chrome should also be supported in
Open Screen, at least for platforms that have a proper `platform/`
implementation.

## How the Sysroots Work

Setting an arm/arm64 target_cpu causes GN to download a sysroot image from
Chrome's public cloud storage bucket. This image is referenced by GN when
building using the compiler `--sysroot` flag. This allows the compiler to
effectively cross compile for different architectures.

## Updating the Sysroots

Sysroot versions are maintained upstream in Chromium. Updates flow into
Open Screen through rolls to the DEPS/gclient files for the `build/` directory.

## Testing on an Embedded Device

To run executables on an embedded device, after copying the relevant binaries
you may need to install additional dependencies, such as `libavcodec` and
`libsdl`. For a list of these packages specifically for cast streaming, see the
[external libraries.md](../cast/docs/external_libraries.md) documentation.
