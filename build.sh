#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# TMA — one command, no arguments.
#
#   ./build.sh            install deps, fetch pinned Chromium, graft tma/,
#                         build, then package. Prints every output path.
#
# The subcommands below still exist, but nothing needs them:
#
#   ./build.sh run        same as above, then launch (Wayland only)
#   ./build.sh deps       install Fedora packages only
#   ./build.sh setup      fetch/pin Chromium + graft + gn gen only
#   ./build.sh build      incremental build of the `tma` target only
#   ./build.sh package    stage a distributable dir; add --xz to compress
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

source "$SCRIPT_DIR/scripts/tma_conf.sh"

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# Where the build lands.  Kept in sync with TMA_OUT / CHROMIUM_SRC below.
out_dir() { printf '%s' "${TMA_OUT:-${CHROMIUM_SRC:-$HOME/chromium/src}/out/Default}"; }

# Where the staged, stripped copy lands: this repository's build/ directory.
stage_dir() { printf '%s' "$SCRIPT_DIR/build"; }

report() {
  local out pkg app res_dir exe res
  out="$(out_dir)"
  pkg="$(stage_dir)"
  app="$(tma_conf_name)"
  res_dir="${app}_resources"
  exe="$out/$app"
  res="$out/$res_dir"

  printf '\n\033[1mdone\033[0m\n\n'
  if [[ ! -x "$exe" ]]; then
    printf '  expected %s but it is missing\n' "$exe"
    return 1
  fi

  printf '  binary     %s\n' "$exe"
  printf '  resources  %s\n' "$res"
  printf '  app page   %s\n' "$res/index.html"
  printf '  size       %s\n' "$(du -h --apparent-size "$exe" | cut -f1)"

  printf '\n  packaged   %s\n' "$pkg"
  if [[ -x "$pkg/$app" ]]; then
    printf '    %-17s %s (stripped)\n' "$app" "$(du -h --apparent-size "$pkg/$app" | cut -f1)"
    [[ -f "$pkg/$app.xz" ]] && printf '    %-17s %s\n' "$app.xz" "$(du -h --apparent-size "$pkg/$app.xz" | cut -f1)"
    printf '    %-17s %s\n' "content_shell.pak" "$(du -h --apparent-size "$pkg/content_shell.pak" | cut -f1)"
    printf '    %-17s %s\n' "total" "$(du -sh --apparent-size "$pkg" | cut -f1)"
  fi

  printf '\nrun it with:\n'
  if [[ -x "$pkg/$app" ]]; then
    printf '  %q\n' "$pkg/$app"
  else
    printf '  %q\n' "$exe"
  fi
  printf 'ship it with:\n'
  printf '  %s\n' "$pkg"
  if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    printf '\n(note: your shell has no WAYLAND_DISPLAY; start TMA from a Wayland session)\n'
  fi
}

case "${1:-all}" in
  all)
    step "1/4 deps"
    scripts/install_deps.sh
    step "2/4 chromium (pinned) + tma"
    scripts/setup_chromium.sh
    step "3/4 build"
    scripts/build_tma.sh
    step "4/4 package"
    scripts/package_tma.sh
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
    step "removing $out and $SCRIPT_DIR/build"
    rm -rf "$out" "$SCRIPT_DIR/build"
    ;;
  distclean)
    step "removing the Chromium checkout, depot_tools and build/"
    rm -rf "${CHROMIUM_SRC:-$HOME/chromium/src}" "${DEPOT_TOOLS:-$HOME/depot_tools}" "$SCRIPT_DIR/build"
    ;;
  *)
    printf 'usage: %s [all|deps|setup|build|run|package|clean|distclean]\n' "$0" >&2
    exit 2
    ;;
esac
