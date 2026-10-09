#!/usr/bin/env bash
# Runs the TMA display shell on a Wayland session.
#
# Usage:
#   scripts/run_tma.sh [-- <extra tma args...>]
#
# Environment:
#   CHROMIUM_SRC  Chromium src/ directory. Default: $HOME/chromium/src
#   TMA_OUT       GN output directory.     Default: <chromium-src>/out/Default

set -euo pipefail

log() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CHROMIUM_SRC="${CHROMIUM_SRC:-$HOME/chromium/src}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

EXE="$TMA_OUT/tma"
[[ -x "$EXE" ]] || die "$EXE not found; run scripts/build_tma.sh first"

# TMA is built with ozone_auto_platforms=false / ozone_platform="wayland", so
# there is no X11 or headless fallback: without a Wayland connection the
# process cannot open a window at all.
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  die "WAYLAND_DISPLAY is not set. TMA only supports Wayland; start it from a Wayland session (e.g. GNOME on Wayland, KDE Plasma Wayland, sway, Hyprland)."
fi

args=()

# Chromium's setuid sandbox helper must be installed setuid root. A checkout
# build never is, so fall back to --no-sandbox rather than fail at startup.
SANDBOX="$TMA_OUT/chrome-sandbox"
if [[ -f "$SANDBOX" ]]; then
  mode="$(stat -c '%a' "$SANDBOX" 2>/dev/null || true)"
  if [[ "$mode" != "4755" ]]; then
    log "note: $SANDBOX has mode $mode (expected 4755); using --no-sandbox."
    log "      To use the sandbox: sudo chown root:root $SANDBOX && sudo chmod 4755 $SANDBOX"
    args+=(--no-sandbox)
  fi
else
  log "note: chrome-sandbox helper not found; using --no-sandbox."
  args+=(--no-sandbox)
fi

if [[ "${1:-}" == "--" ]]; then
  shift
fi

exec "$EXE" "${args[@]}" "$@"
