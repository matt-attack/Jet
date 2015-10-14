#wget http://www.cmake.org/files/v3.2/cmake-3.2.2.tar.gz
#tar -xzvf cmake-3.2.2.tar.gz
#cd cmake-3.2.2
#./configure --prefix=$HOME/cmake-3.2.2
#make
#make install

wget http://llvm.org/releases/3.7.0/llvm-3.7.0.src.tar.xz
tar -xf llvm-3.7.0.src.tar.xz
mkdir llvm-3.7.0
cd llvm-3.7.0
cmake ../llvm-3.7.0.src/CMakeLists.txt

cmake --build .