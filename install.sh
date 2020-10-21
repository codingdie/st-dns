set -e
if [ ! -d "build" ]; then
    mkdir build
fi
CMAKE_INSTALL_PREFIX="/usr/local"
if [ "" != "$1" ];then
  CMAKE_INSTALL_PREFIX=$1
fi
cd build
cmake  -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}  ../
make -j8
sudo make install