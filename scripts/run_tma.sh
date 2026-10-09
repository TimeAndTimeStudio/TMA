#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

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

# Chromium aborts (LOG(FATAL)) only when the setuid helper exists but is not
# root-owned, setuid and world-executable — sandbox/linux/suid/client/
# setuid_sandbox_host.cc:170-176. When the helper is absent entirely it uses
# the user-namespace sandbox instead, which Fedora allows by default, so no
# flag is needed at all. Force --no-sandbox for the one case Chromium itself
# would refuse to start.
args=()
for helper in "$TMA_OUT/chrome-sandbox"; do
  if [[ -f "$helper" ]]; then
    mode="$(stat -c '%a' "$helper" 2>/dev/null || echo 0)"
    owner="$(stat -c '%u' "$helper" 2>/dev/null || echo 1)"
    if [[ "$owner" != "0" ]] || (( (8#$mode & 4000) == 0 )) || (( (8#$mode & 1) == 0 )); then
      log "note: $helper is mode $mode owner $owner; using --no-sandbox."
      log "      To use the sandbox: sudo chown root:root $helper && sudo chmod 4755 $helper"
      args+=(--no-sandbox)
    fi
  fi
done

if [[ "${1:-}" == "--" ]]; then
  shift
fi

exec "$EXE" "${args[@]}" "$@"
