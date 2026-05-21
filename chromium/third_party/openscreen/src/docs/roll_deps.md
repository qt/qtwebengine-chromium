# Updating Dependencies

The Open Screen Library uses several external libraries that are compiled from
source and fetched from additional git repositories into folders below
`third_party/`.  These libraries are listed in the `DEPS` file in the root of
the repository.  (`DEPS` listed with `dep_type: cipd` or `gcs` are not managed
through git.)

Periodically the versions of these libraries are updated to pick up bug and
security fixes from upstream repositories.

Library versions are tracked both in the `DEPS` file and in git submodules.
Using the roll-dep script below will keep the two in sync.

The process is roughly as follows:

1. Identify the git commit for the new version of the library that you want to
   roll.
   - For libraries that are also used by Chromium, prefer rolling to the same
     version listed in [`Chromium DEPS`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/DEPS).
     These libraries are listed below.
   - For other libraries, prefer the most recent stable or general availability release.
2. Use the `roll-dep` script to update the git hashes in `DEPS` and the
   corresponding git submodule.
   - This will generate and upload a CL to Gerrit.
   - Example: `roll-dep -r jophba --roll-to 229b04537a191ce272a96734b0f4ddcccc31f241 third_party/quiche/src`
   - More documentation in [Rolling Dependencies](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/dependencies.md#rolling-dependencies)
3. Update your CL if needed, such as updating the `BUILD.gn` file for the
   corresponding library to handle files moving around, compiler flag
   adjustments, etc.
4. Use a tryjob to verify that nothing is broken and send for code review.

## Libraries used in Chromium

- `build` (mirrored from Chromium)
- `third_party/libprotobuf-mutator/src`
- `third_party/jsoncpp/src`
- `third_party/googletest/src` (not kept in sync with Chromium, see `DEPS`)
- `third_party/quiche/src`
- `third_party/abseil/src` (not kept in sync with Chromium, see `DEPS`)
- `third_party/libfuzzer/src`
- `third_party/googleurl/src` (not kept in sync with Chromium, see `DEPS`)

## Rolling buildtools/

`buildtools/` is a special case because it is interdependent with
`third_party/libc++/src` and `third_party/libc++abi/src`; they must all be
rolled in one CL.

The steps to roll:

1. Get the [current commit hash](https://chromium.googlesource.com/chromium/src/buildtools/)
   of the buildtools mirror (call it X).
2. Get the [commit hash of libc++](https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main/deps_revisions.gni)
   from `deps_revisions.gni` (call it Y).
3. Get the [commit hash of libc++abi](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/DEPS)
   (call it Z) from `chromium/src/DEPS`.
4. In the same local branch:
   - `$ roll-dep --roll-to X -r <reviewer>@chromium.org buildtools/`
   - `$ roll=dep --ignore-dirty-tree --roll-to Y third_party/libc++/src`
   - `$ roll=dep --ignore-dirty-tree --roll-to Z third_party/libc++abi/src`
5. `$ git cl upload` the resulting commits.

[Roll buildtools/ 5df641722..eca5f0685 (13 commits)](https://chromium-review.googlesource.com/c/openscreen/+/7226339)
is an example of a successful roll CL.

## Other special cases that are updated manually

- `third_party/protobuf`: See instructions in
  `third-party/protobuf/README.chromium`
- `third_party/boringssl/src`: See instructions in
  `third_party/boringssl/README.openscreen.md`

## Rolling GCS dependencies

Google Cloud Storage (GCS) dependencies do not use `git`, instead they refer to
specific single non-archive files or tar archives that have been uploaded to the
cloud.

We try to avoid owning GCS objects in Open Screen, so currently the only GCS
dependencies come from the Chromium project, and are necessary since Open Screen
uses the Chromium project's build and check in tooling.

Generally speaking, updating these dependencies involves pulling the object
information from the root Chromium DEPS file.

For an example GCS roll, see:
[CL 7116905: Roll various build dependencies](https://chromium-review.googlesource.com/c/openscreen/+/7116905).

Chromium provides some documentation here:
[GCS objects for chromium dependencies](https://chromium.googlesource.com/chromium/src.git/+/refs/heads/lkgr/docs/gcs_dependencies.md).