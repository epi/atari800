# Atari800 Tizen package

## Demo

If you're too lazy or don't have a Gear Fit, watch a demo on [youtube](https://www.youtube.com/watch?v=j1grm0DAzqM).

## Prerequisites

Install Tizen Studio (including Tizen 2.3.1 for wearable devices and gcc-6.2) into `$HOME/tizen-studio`. Configure the certificates and make sure you can connect to your Gear Fit 2 Pro using sdb.

## Build the package

```
git clone https://github.com/epi/atari800.git
cd atari800
git checkout tizen
cd src
./autogen.sh 
./configure --host=arm-linux-gnueabi --target=tizen
make -j
cd tizen
./package.sh
```

The last command may print some Java exception messages. Just ignore them, as long as `src/tizen/Debug/io.github.atari800-1.0.0.tpk` is created.

## Install the package

Well, just install it on your watch. Connect some bluetooth headphones. Touch the `A` app icon once installation is complete and have fun watching the demo. Don't forget to kill the app after, or it'll eat up your battery very quickly.

