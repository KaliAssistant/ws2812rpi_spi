#!/usr/bin/bash

set -e

PWDD=`pwd`
BCM2835_LIB=./lib/bcm2835-1.75
LIBINIH_LIB=./lib/inih-r60

cd $PWDD
cd "$BCM2835_LIB" && ./configure
make
cd $PWDD
cp "$BCM2835_LIB"/src/libbcm2835.a "$PWDD"/lib/
cp "$BCM2835_LIB"/src/bcm2835.h "$PWDD"/include/

cd $PWDD
cd "$LIBINIH_LIB"
gcc -O3 -Wall -c ini.c
ar -rcs libinih.a ini.o
g++ -Wall -O3 -std=c++11 -c ./cpp/INIReader.cpp
ar -rcs libINIReader.a INIReader.o
cd $PWDD
cp "$LIBINIH_LIB"/libinih.a "$LIBINIH_LIB"/libINIReader.a "$PWDD"/lib/
cp "$LIBINIH_LIB"/cpp/INIReader.h "$LIBINIH_LIB"/ini.h "$PWD"/include/

cd "$PWDD"
mkdir -p bin
g++ -Wall -O3 -Wno-unused-variable -std=c++11 -Iinclude ./src/ws2812rpi_spi.cpp -o "$PWDD"/bin/ws2812rpi_spi ./lib/libbcm2835.a ./lib/libINIReader.a ./lib/libinih.a  -fPIC -lm -lrt -lpthread

g++ -Wall -O3 -Wno-unused-variable -std=c++11 -Iinclude ./src/ws2812rpi_pipe.cpp -o "$PWDD"/bin/ws2812rpi_pipe ./lib/libINIReader.a ./lib/libinih.a -fPIC -lm -lrt -lpthread
