: # If you want to use a local build of libavm, you must clone the avm repo in this directory first, then enable the "avm" Cargo feature.
: # The git tag below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

: # The directory is named "build.CrabbyAvif" because "build" already exists in the libavm repository.

: # CMAKE_POLICY_VERSION_MINIMUM set to avoid CI errors such as
: #   build.CrabbyAvif/neon2sse/CMakeLists.txt:4
: #   Compatibility with CMake < 3.5 has been removed from CMake.

git clone -b research-v13.0.0 --depth 1 https://gitlab.com/AOMediaCodec/avm
echo "build.CrabbyAvif" > avm/.git/info/exclude
cmake -S avm -B avm/build.CrabbyAvif -G Ninja -DBUILD_SHARED_LIBS=OFF -DCONFIG_PIC=1 -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
ninja -C avm/build.CrabbyAvif
