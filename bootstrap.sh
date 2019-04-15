#!/bin/sh
rm -rf ~/cmake
mkdir ~/cmake
/bin/bash cmake-3.6.3-Linux-i386.sh --skip-license --prefix=/home/utnso/cmake --exclude-subdir

mkdir build
cd build
/home/utnso/cmake/bin/cmake -G"Unix Makefiles" ../
make -j 4
