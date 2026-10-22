git clone -b v1.3.2 --depth 1 https://github.com/madler/zlib.git

cd zlib
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release ..
ninja
cd ../..

git clone -b v1.6.51 --depth 1 https://github.com/glennrp/libpng.git
cd libpng
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DZLIB_ROOT="../../zlib" -DZLIB_LIBRARY="../../zlib/build/z.lib" -DZLIB_INCLUDE_DIR="../../zlib" ..
cmake --build . --config Release
cd ../..
