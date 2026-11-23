# Building K-UI from source

[Project overview](../README.md) · [Documentation index](README.md)

This page is for developers. To use K-UI on a Dreamcast, install a published
package using [Getting started](getting-started.md).

Before choosing a build recipe or preparing a console package, read
[AGENTS.md](../AGENTS.md) and [accepted progress](accepted-progress.md).
They record the required component set, preservation rules and hardware
evidence. A successful build alone does not qualify a new game-reader release.

## Toolchain and host checks

On Ubuntu 24.04 or a compatible Linux/WSL installation, the general setup is:

```sh
sudo apt-get install build-essential git curl wget patch python3 bison flex \
  texinfo gettext libgmp-dev libmpfr-dev libmpc-dev libisl-dev \
  meson ninja-build pkg-config libisofs-dev libpng-dev libjpeg-dev \
  dosfstools exfatprogs mtools ffmpeg
python3 tools/fetch_deps.py --fatfs-only
make test test-images
bash tools/setup_kos.sh
source .deps/kos/environ.sh
make diagnostic
python3 tools/package.py
```

Run commands from the repository root. The initial compiler build is
substantial; subsequent builds reuse it. Dependencies are pinned in
[dependencies.json](../dependencies.json). K-UI builds on upstream KallistiOS;
no DreamShell checkout is required.

Packaging requires a clean, committed source tree so its source archive
matches the executable. For a current complete release, follow the applicable
distribution workflow and accepted component records rather than assuming a
single diagnostic package contains every game reader.

Host tests exercise the portable C command, TOC, block-device and filesystem
code used by the console app. Filesystem tests use disposable image files and
independent filesystem utilities; they do not operate on physical disks.
Choose focused checks appropriate to the change, as directed by AGENTS.md.

## Development references

- [Universal Loader roadmap](universal-loader-roadmap.md)
- [GUI line preservation](gui-lines.md)
- [Shared loader performance policy](shared-loader-performance-policy.md)
- [Release and nightly publishing](online-update-channels.md)
- [Dependency provenance and licenses](../THIRD_PARTY.md)

Historical experiments and their results remain under `docs/` and `evidence/`.
Keep hardware observations distinct from host tests and source inspection.
