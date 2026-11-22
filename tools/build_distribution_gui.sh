#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Both distributions link identical reader bindings. The GUI records its channel.
set -eo pipefail
case "$1" in
    --nightly) nightly=1 ;;
    --candidate) nightly=0 ;;
    *) echo 'Expected --nightly or --candidate' >&2; exit 2 ;;
esac
source .deps/kos/environ.sh
bundle_dir=$(mktemp -d)
trap 'rm -rf "$bundle_dir"' EXIT
python3 tools/stage_distribution.py --gui-inputs "$bundle_dir"
revision=$(git rev-parse HEAD)
make -j2 -f Makefile.dc build/kui-runtime.elf BUILD_ID="${revision:0:12}" \
    RUNTIME_MODULE_BUNDLE="$bundle_dir" \
    NATIVE_STANDARD_MANIFEST=resources/updater-bindings/installed-standard-9e3295bcfaec.h \
    KUI_SMASH_LAUNCH_TIMING=1 KUI_GITHUB_NIGHTLY="$nightly" KUI_UPDATE_REVIEW_FEED=0
