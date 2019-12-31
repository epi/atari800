#!/bin/sh

TIZEN_STUDIO=$HOME/tizen-studio

mkdir -p Debug && \
cp ../atari800 Debug/ && \
$TIZEN_STUDIO/tools/ide/bin/tizen package -s D -t tpk -- Debug && \
echo "All good!"
