git clone -b v1.3.2 --depth 1 https://github.com/madler/zlib.git

cd zlib
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF ..
make
cd ../..

git clone -b v1.6.58 --depth 1 https://github.com/glennrp/libpng.git
cd libpng
mkdir build
cd build
ZLIB_ROOT="../../zlib" cmake -DBUILD_SHARED_LIBS=OFF ..
make
cd ../..
