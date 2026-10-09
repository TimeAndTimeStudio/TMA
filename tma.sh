#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# TMA launcher — double-click this file to start the display shell.
#
# It finds the binary, fixes up the environment, and only adds --no-sandbox
# when the sandbox genuinely cannot work (see "Sandbox" below). Every run is
# logged to ~/.cache/tma-logs/.
#
# Usage:  ./tma.sh [extra tma args...]

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOGDIR="$HOME/.cache/tma-logs"
LOG="$LOGDIR/tma-$(date +%Y%m%d-%H%M%S).log"
mkdir -p "$LOGDIR"

# Errors must be visible even with no terminal (double-click has none).
fail() {
  printf 'TMA: %s\n' "$*" >&2
  printf 'TMA: log: %s\n' "$LOG" >&2
  command -v notify-send >/dev/null && \
    notify-send -u critical "TMA" "$*" 2>/dev/null || true
  printf 'TMA: %s\nlog: %s\n' "$*" "$LOG" >>"$LOG"
  exit 1
}

# --- 1. find the binary -----------------------------------------------------
# Prefer the packaged copy in this repository, then the raw build output.
CHROMIUM_SRC="${CHROMIUM_SRC:-$HOME/chromium/src}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

EXE=""
for candidate in "$ROOT/build/tma" "$TMA_OUT/tma"; do
  if [[ -x "$candidate" ]]; then
    EXE="$candidate"
    break
  fi
done
[[ -n "$EXE" ]] || fail "no tma binary found. Run ./build.sh first."

# --- 2. Wayland -------------------------------------------------------------
# TMA is built ozone_platform="wayland" only, so without a connection it
# cannot open a window at all. A double-clicked file often inherits a shell
# that lost WAYLAND_DISPLAY, so recover it from the runtime dir.
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  for sock in "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"/wayland-*; do
    if [[ -S "$sock" ]]; then
      export WAYLAND_DISPLAY="${sock##*/}"
      break
    fi
  done
fi
[[ -n "${WAYLAND_DISPLAY:-}" ]] || \
  fail "no Wayland session found. TMA supports Wayland only."

# --- 3. sandbox -------------------------------------------------------------
# Chromium aborts (LOG(FATAL)) only when the setuid helper exists but is not
# root-owned, setuid and world-executable — sandbox/linux/suid/client/
# setuid_sandbox_host.cc:170-176. When the helper is absent entirely it uses
# the user-namespace sandbox instead, which Fedora allows by default
# (max_user_namespaces is non-zero), so no flag is needed at all.
#
# In other words: only force --no-sandbox for the one case Chromium itself
# would refuse to start.
args=()
for helper in "$ROOT/build/chrome-sandbox" "$TMA_OUT/chrome-sandbox"; do
  if [[ -f "$helper" ]]; then
    mode="$(stat -c '%a' "$helper" 2>/dev/null || echo 0)"
    owner="$(stat -c '%u' "$helper" 2>/dev/null || echo 1)"
    if [[ "$owner" != "0" ]] || (( (8#$mode & 4000) == 0 )) || (( (8#$mode & 1) == 0 )); then
      printf 'TMA: %s is mode %s owner %s; using --no-sandbox\n' \
        "$helper" "$mode" "$owner" >>"$LOG"
      args+=(--no-sandbox)
    fi
    break
  fi
done

# --- 4. launch --------------------------------------------------------------
printf 'TMA: %s\n' "$EXE" >>"$LOG"
printf 'TMA: WAYLAND_DISPLAY=%s DISPLAY=%s\n' \
  "${WAYLAND_DISPLAY:-}" "${DISPLAY:-}" >>"$LOG"
printf 'TMA: args: %s\n' "${args[*]:-<none>}" >>"$LOG"

# Keep stdout/stderr out of the way of a double-click, but do not lose them.
exec "$EXE" "${args[@]}" "$@" >>"$LOG" 2>&1
