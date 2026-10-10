#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# Runs the TMA display shell on a Wayland session.
#
# Usage:
#   scripts/run_tma.sh
#
# TMA takes no arguments at all. What it opens, its name, its Wayland app_id
# and its window floor are all decided by tma.conf at build time -- see that
# file -- which is what makes two runs of the same binary do the same thing.
#
# Environment:
#   CHROMIUM_SRC  Chromium src/ directory. Default: $HOME/chromium/src
#   TMA_OUT       GN output directory.     Default: <chromium-src>/out/Default

set -euo pipefail

log() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

source "$SCRIPT_DIR/tma_conf.sh"

CHROMIUM_SRC="${CHROMIUM_SRC:-$HOME/chromium/src}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

EXE="$TMA_OUT/$(tma_conf_name)"
[[ -x "$EXE" ]] || die "$EXE not found; run scripts/build_tma.sh first (or set app_name in tma.conf back to the name the build produced)"

# TMA is built with ozone_auto_platforms=false / ozone_platform="wayland", so
# there is no X11 or headless fallback: without a Wayland connection the
# process cannot open a window at all.
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  die "WAYLAND_DISPLAY is not set. TMA only supports Wayland; start it from a Wayland session (e.g. GNOME on Wayland, KDE Plasma Wayland, sway, Hyprland)."
fi

# Chromium aborts (LOG(FATAL)) only when the setuid sandbox helper exists but
# is not root-owned, setuid and world-executable -- sandbox/linux/suid/client/
# setuid_sandbox_host.cc:170-176. The build never produces chrome-sandbox, so
# the helper is absent entirely and Chromium falls back to the user-namespace
# sandbox, which Fedora allows by default. Nothing to pass, and nothing TMA
# would accept if there were.
exec "$EXE"
