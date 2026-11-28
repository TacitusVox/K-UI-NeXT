#!/bin/sh
# usage: build.sh OUT [extra flags] (the reviewer's; flags.txt was the drive test's compile flags, from make -n)
cd /home/user/K-UI-NeXT || exit 1
out=$1; shift
FLAGS="$(cat build/review4/flags.txt) -DKUI_TOY_CDDA_OUTPUT_HOST_TEST=1"
gcc $FLAGS -O2 "$@" -Ibuild/review4 build/review4/joint.c src/loader/toy_cdda_player.c src/core/retail_gd.c src/loader/toy_cdda_output.c -Wl,--wrap=kui_toy_cdda_player_hold -Wl,--gc-sections -lm -o build/review4/$out
