#!/usr/bin/env bash
# TMA — one command.
#
#   ./build.sh            install missing deps, fetch pinned Chromium, graft tma/, build
#   ./build.sh run        same, then launch (Wayland only)
#   ./build.sh deps       install Fedora packages only
#   ./build.sh setup      fetch/pin Chromium + graft + gn gen only
#   ./build.sh build      incremental build of the `tma` target only
#   ./build.sh package    stage a distributable dir: strip the binary, copy
#                         resources; add --xz for a compressed archive
#   ./build.sh clean      drop out/Default, keep the checkout
#   ./build.sh distclean  drop the checkout and depot_tools as well
#
# The first run is heavy (Chromium source + toolchains + a full compile).
# After that every step is idempotent: re-running only rebuilds what changed.
#
# Environment (all optional):
#   CHROMIUM_SRC    Chromium src/ directory.  Default: $HOME/chromium/src
#   DEPOT_TOOLS     depot_tools checkout.     Default: $HOME/depot_tools
#   TMA_OUT         GN output directory.      Default: <chromium-src>/out/Default
#   CHROMIUM_TAG    Override the tag pinned in CHROMIUM_VERSION

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

case "${1:-all}" in
  all)
    step "1/3 deps"
    scripts/install_deps.sh
    step "2/3 chromium (pinned) + tma"
    scripts/setup_chromium.sh
    step "3/3 build"
    scripts/build_tma.sh
    printf '\nDone. Run it with: ./build.sh run\nFor a shippable copy: ./build.sh package\n'
    ;;
  deps)
    shift
    scripts/install_deps.sh "$@"
    ;;
  setup)   scripts/setup_chromium.sh ;;
  build)   scripts/build_tma.sh ;;
  run)     scripts/run_tma.sh ;;
  package)
    shift
    scripts/package_tma.sh "$@"
    ;;
  clean)
    out="${CHROMIUM_SRC:-$HOME/chromium/src}/out/Default"
    step "removing $out"
    rm -rf "$out"
    ;;
  distclean)
    step "removing the Chromium checkout and depot_tools"
    rm -rf "${CHROMIUM_SRC:-$HOME/chromium/src}" "${DEPOT_TOOLS:-$HOME/depot_tools}"
    ;;
  *)
    printf 'usage: %s [all|deps|setup|build|run|package|clean|distclean]\n' "$0" >&2
    exit 2
    ;;
esac
