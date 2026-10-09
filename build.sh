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

# Where the build lands.  Kept in sync with TMA_OUT / CHROMIUM_SRC below.
out_dir() { printf '%s' "${TMA_OUT:-${CHROMIUM_SRC:-$HOME/chromium/src}/out/Default}"; }

report() {
  local out exe res
  out="$(out_dir)"
  exe="$out/tma"
  res="$out/tma_resources"

  printf '\n\033[1mdone\033[0m\n\n'
  if [[ -x "$exe" ]]; then
    printf '  binary     \033[1m%s\033[0m\n' "$exe"
    printf '  resources  %s\n' "$res"
    printf '  app page   %s\n' "$res/index.html"
    printf '  size       %s (strip it for shipping: scripts/package_tma.sh)\n' \
      "$(du -h --apparent-size "$exe" | cut -f1)"

    printf '\nrun it with:\n'
    printf '  env WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 %q --no-sandbox\n' "$exe"
    if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
      printf '  (your shell has no WAYLAND_DISPLAY; start it from a Wayland session)\n'
    fi
  else
    printf '  expected %s but it is missing\n' "$exe"
    return 1
  fi
}

case "${1:-all}" in
  all)
    step "1/3 deps"
    scripts/install_deps.sh
    step "2/3 chromium (pinned) + tma"
    scripts/setup_chromium.sh
    step "3/3 build"
    scripts/build_tma.sh
    report
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
