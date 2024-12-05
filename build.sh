#!/bin/sh

cd analysis/tools/egalito
export USE_LOADER=0
make -j 
cd analysis/app
make
