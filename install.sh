set -e
if [ ! -d "/tmp/st-dns-build" ]; then
  mkdir /tmp/st-dns-build
fi
CMAKE_INSTALL_PREFIX="/usr/local"
if [ "" != "$1" ]; then
  CMAKE_INSTALL_PREFIX=$1
fi
baseDir=$(pwd)
cd /tmp/st-dns-build
cmake -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=/Release ${baseDir}
make -j8
make install
