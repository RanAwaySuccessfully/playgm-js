#!/bin/bash

cd ..
source ./emsdk/emsdk_env.sh

# building

echo ""
cd player/build

# game-music-emu

#mkdir -p game-music-emu
#rm -r game-music-emu/*
cd game-music-emu
#emcmake cmake ../../lib/game-music-emu -DGME_YM2612_EMU=GENS
#emmake make
emcc -lembind -fsanitize=undefined gme/libgme.a ../../lib/gme.cpp -o gme.js -sEXPORTED_RUNTIME_METHODS=HEAPU8 -sMODULARIZE -sEXPORT_NAME=GME
#cp gme/libgme.so.1 ../
cd ../
echo ""

echo "END OF BUILD EARLY"
exit

# psflib

mkdir -p psflib
rm -r psflib/*
cp -r ../lib/psflib ./
cd psflib
emmake make all
cd ../
echo ""

# lazyusf2

mkdir -p lazyusf2
rm -r lazyusf2/*
cp -r ../lib/lazyusf2 ./
cd lazyusf2
# diff -Naru lazyusf2/Makefile lazyusf2.mk > lazyusf2.patch
#patch -p1 -i ../lazyusf2.patch
emmake all
cd ../
echo ""

echo "END OF BUILD"
exit