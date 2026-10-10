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
# than compiling a binary that opens the wrong URL and finding out at runtime.
conf_app="$(tma_conf_name)"
conf_startup="$(tma_conf_get startup)" || conf_startup=""
built_app="$(sed -nE 's/^tma_app_name = "(.*)"$/\1/p' "$TMA_OUT/args.gn" 2>/dev/null | tail -n1)"
if [[ -n "$built_app" && "$built_app" != "$conf_app" ]]; then
  die "tma.conf app_name is '$conf_app' but this build was configured with '$built_app'; run scripts/setup_chromium.sh first"
fi
if grep -q '^tma_startup_url = ' "$TMA_OUT/args.gn" 2>/dev/null; then
  built_url="$(sed -nE 's/^tma_startup_url = "(.*)"$/\1/p' "$TMA_OUT/args.gn" | tail -n1)"
  built_startup="url:$built_url"
  if [[ "$built_startup" != "$conf_startup" ]]; then
    die "tma.conf startup is '$conf_startup' but this build was configured with '$built_startup'; run scripts/setup_chromium.sh first"
  fi
fi

# ninja only ever adds outputs, so a <app_name>_resources directory left by an
# older build would sit there forever. There is no packaged page any more, so
# drop any copy rather than ship one nobody can open.
stale="$TMA_OUT/${conf_app}_resources"
if [[ -d "$stale" ]]; then
  log "Removing $stale (TMA has no packaged page any more)"
  rm -rf "$stale"
fi

export PATH="$DEPOT_TOOLS:$PATH"
command -v autoninja >/dev/null || die "autoninja not found; is depot_tools on PATH?"

log "autoninja -C $TMA_OUT tma"
# siso announces that it is running with --offline on every invocation. That is
# the only mode TMA has -- use_remoteexec is forced off in the GN args, see
# scripts/setup_chromium.sh -- so the line is pure noise. Drop exactly that line
# from stderr and leave everything else, including failures, untouched.
autoninja -C "$TMA_OUT" tma 2> >(grep -vxF -- "offline mode" >&2)

log "Built $TMA_OUT/$conf_app"
