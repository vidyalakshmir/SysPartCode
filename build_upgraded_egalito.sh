#!/bin/sh


cd analysis/tools/egalito
git checkout egalito-upgrade
make clean
make -j 8
cd ../../app
make clean
make
