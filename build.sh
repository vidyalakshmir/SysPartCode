#!/bin/sh

cd analysis/tools/egalito
export USE_LOADER=0
make -j 8
cd analysis/app
make
