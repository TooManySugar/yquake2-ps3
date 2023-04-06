#!/bin/sh

version=$( cat version )
filename=yquake2_$version

echo Creating $filename.pkg it may take a while depending on ./pkg/ folder size...
./tools/pkg-linux --contentid UP0001-QUAKE2_00-0000000000000000 ./pkg/ $filename.pkg
echo Done