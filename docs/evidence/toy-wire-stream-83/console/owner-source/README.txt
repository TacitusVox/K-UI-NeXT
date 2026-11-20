Stream Survey Test 83 / MDK2 reporting source and verification
The GUI and survey reader have separate histories. In a clone of
TacitusVox/K-UI-NeXT containing the listed parent, use the corresponding
commands to recover a clean revision. Both bundles were independently
restored using only their parent revision and checked against the tree
used by the compiler.

gui source
Parent 4441e92c65bc0d77667f318d67f2502f5d0ce9c6
Tip    4d4dcff77494bf36a7eb810eec6684b8b5d64cca
git bundle verify /path/to/gui.bundle
git fetch /path/to/gui.bundle refs/heads/fix/test83-reader-reporting:refs/heads/rebuild-test83-gui
git switch rebuild-test83-gui

survey source
Parent 3bd4bed2fca227530e343a2fd97a1d790b230e21
Tip    1d80f771f081280fc42bc3161304dd80f6e0ae55
git bundle verify /path/to/survey.bundle
git fetch /path/to/survey.bundle refs/heads/fix/test83-survey-reporting:refs/heads/rebuild-test83-survey
git switch rebuild-test83-survey

Build the GUI with the repository's pinned KallistiOS environment active
in the clean GUI checkout. Place the resulting runtime.kui under KUI on
the SD card and duplicate it as boot.kui.

make -j8 -f Makefile.dc build/kui-runtime.elf BUILD_ID=4d4dcff77494 KUI_SMASH_LAUNCH_TIMING=1 KUI_GITHUB_NIGHTLY=1 KUI_UPDATE_REVIEW_FEED=0
mkdir -p build/survey-package
PYTHONPATH=tools python3 -c "from nightly import build_gui;build_gui('4d4dcff77494bf36a7eb810eec6684b8b5d64cca','build/survey-package')"

In the clean survey checkout, use the pinned toolchain and run

make -j6 -f Makefile.vmu-native-toy-suite TOY_SUITE_MODE=66 TOY_WIRE_STREAM=1 TOY_WIRE_CACHED=1 TOY_WIRE_WORDS=1 TOY_WIRE_PACE=1 TOY_WIRE_IDLE=1 TOY_WIRE_LEAN=1 TOY_WIRE_STEADY=1 TOY_WIRE_FLUID=1 TOY_WIRE_SEQ=1 TOY_WIRE_ANY=1 TOY_WIRE_TITLES=1 TOY_WIRE_QUERY=1 TOY_WIRE_TIMER=1 TOY_WIRE_SAVE=1 KUI_NATIVE_BACKGROUND_MAP160=1 TOY_SURVEY_MIXED_PAD=1 BUILD_ID=1d80f771f081 BUILD=build/survey83 build/survey83/retail-native-dual.kui

Put the resulting retail-native-dual.kui in KUI/apps/games on the card.
Use validation.txt to compare the exact component hashes. To reproduce
the host cold-boot check, run

python3 verify_boot.py /path/to/gui-checkout /path/to/preceding-2cc57af0a920.zip /path/to/this-install.zip 4d4dcff77494

The preceding package in that command is KUI-Test83-MDK2-Omikron-
20NOV2026-1109-CST-2cc57af0a920.zip (one filename).

After reproducing the two release binaries, you can apply the included
corpus-test.patch to either checkout to run its original-executable
cache tests and shared launch audit. The corpus test recognizes all
supported exit families including the new family 11. The launch audit
follows the shared cache replay helper. These are test-only changes.
From the repository root use

git apply /path/to/corpus-test.patch
