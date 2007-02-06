#!/bin/sh

cd kernel
make || exit(1)
cd ../osd_initiator
./autogen.sh --prefix=/usr/local || exit(1)
make || exit(1)
cd ../osd-target
make || exit(1)
