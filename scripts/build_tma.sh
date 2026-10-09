#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# Builds the TMA display shell.
#
# Usage:
#   scripts/build_tma.sh [chromium-src-dir] [output-dir]
#
# Environment:
#   CHROMIUM_SRC  Chromium src/ directory. Default: $HOME/chromium/src
#   TMA_OUT       GN output directory.     Default: <chromium-src>/out/Default

set -euo pipefail

log() { printf '\n==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

source "$SCRIPT_DIR/tma_conf.sh"

CHROMIUM_SRC="${1:-${CHROMIUM_SRC:-$HOME/chromium/src}}"
TMA_OUT="${2:-${TMA_OUT:-$CHROMIUM_SRC/out/Default}}"
DEPOT_TOOLS="${DEPOT_TOOLS:-$HOME/depot_tools}"

[[ -e "$CHROMIUM_SRC/.gn" ]] || die "no Chromium source tree at $CHROMIUM_SRC"
[[ -f "$CHROMIUM_SRC/tma/BUILD.gn" ]] || die "TMA not grafted onto the checkout; run scripts/setup_chromium.sh first"
[[ -d "$TMA_OUT" ]] || die "no build directory at $TMA_OUT; run scripts/setup_chromium.sh first"

# tma.conf is baked into args.gn by setup_chromium.sh, so an edit to the file
# only takes effect on the next setup. Catching the difference here is cheaper
# than compiling a binary under the wrong name and finding it missing later.
conf_app="$(tma_conf_name)"
built_app="$(sed -nE 's/^tma_app_name = "(.*)"$/\1/p' "$TMA_OUT/args.gn" 2>/dev/null | tail -n1)"
if [[ -n "$built_app" && "$built_app" != "$conf_app" ]]; then
  die "tma.conf app_name is '$conf_app' but this build was configured with '$built_app'; run scripts/setup_chromium.sh first"
fi

export PATH="$DEPOT_TOOLS:$PATH"
command -v autoninja >/dev/null || die "autoninja not found; is depot_tools on PATH?"

log "autoninja -C $TMA_OUT tma"
autoninja -C "$TMA_OUT" tma

log "Built $TMA_OUT/$conf_app"
