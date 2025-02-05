#!/bin/sh

cd analysis/tools/egalito
export USE_LOADER=0
git checkout syspart-updated
make -j 8
cd ../../app
make
