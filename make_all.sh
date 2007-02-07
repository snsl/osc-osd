#!/bin/sh

cd kernel
make $@ 
cd ../util
make $2 || exit 1
cd ../osd_initiator
./autogen.sh --prefix=/usr/local || exit 1
make $@ || exit 1
cd ../osd-target
make $@ 
