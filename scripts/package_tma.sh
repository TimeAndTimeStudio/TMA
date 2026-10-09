#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# Stages a distributable TMA directory: strip the binary, copy the resources.
#
# Chromium never strips its own output on Linux — `enable_stripping` is only
# referenced from build/config/apple/ — so on this platform stripping is a
# packaging step you run yourself. It is by far the largest single saving:
# the symbol table alone is ~140 MB of the binary.
#
# The staged copy lands in this repository's build/ directory by default, not
# in the Chromium checkout: the build tree is disposable, the staged copy is
# what you keep or ship.
#
# Usage:
#   scripts/package_tma.sh [--xz] [output-dir]
#
# Arguments:
#   --xz          also write <output-dir>/tma.xz (xz -9)
#   output-dir    default: <this repository>/build
#
# Environment:
#   CHROMIUM_SRC  Chromium src/ directory. Default: $HOME/chromium/src
#   TMA_OUT       GN output directory.     Default: <chromium-src>/out/Default

set -euo pipefail

log() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CHROMIUM_SRC="${CHROMIUM_SRC:-$HOME/chromium/src}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

do_xz=0
if [[ "${1:-}" == "--xz" ]]; then
  do_xz=1
  shift
fi

DIST="${1:-$PROJECT_ROOT/build}"

EXE="$TMA_OUT/tma"
PAK="$TMA_OUT/content_shell.pak"
RES="$TMA_OUT/tma_resources"

[[ -x "$EXE" ]] || die "$EXE not found; run scripts/build_tma.sh first"
[[ -f "$PAK" ]] || die "$PAK not found; run scripts/build_tma.sh first"
[[ -d "$RES" ]] || die "$RES not found; run scripts/build_tma.sh first"

bytes() { # bytes <file> -> "12345678 B (117.7 MB)"
  local n
  n="$(stat -c '%s' "$1")"
  awk -v n="$n" 'BEGIN { printf "%d B (%.1f MB)", n, n / 1048576 }'
}

mkdir -p "$DIST"
rm -rf "${DIST:?}/tma_resources"

log "staging -> $DIST"
cp -f "$EXE"   "$DIST/tma"
cp -f "$PAK"   "$DIST/content_shell.pak"
cp -a "$RES"   "$DIST/tma_resources"

# Runtime files that must sit next to the binary. Chromium resolves them
# relative to /proc/self/exe, so a staged copy without them dies at startup
# with "Invalid file descriptor to ICU data received" — icudtl.dat in
# particular. This is the same set Chrome's own Linux packaging ships.
# (.TOC files are link-time artifacts; libtest_*, libVkLayer_* and
# libVkICD_* are test/validation only, and the qt shims are optional.)
runtime_files=(
  icudtl.dat               # ICU data, 10.8 MB — without it the process aborts
  snapshot_blob.bin        # V8 external startup data (gin/v8_initializer.cc:
                           #   kSnapshotFileName) — without it: "Error loading
                           #   V8 startup snapshot file", then the zygote dies
  v8_context_snapshot.bin  # V8 context snapshot, second of the two
  libEGL.so                # ANGLE, the GL bindings Chromium uses
  libGLESv2.so
  libvk_swiftshader.so     # software Vulkan fallback
  libvulkan.so.1
  chrome_crashpad_handler  # crash reporting; missing is survivable
)
staged_runtime=0
for f in "${runtime_files[@]}"; do
  if [[ -e "$TMA_OUT/$f" ]]; then
    cp -a "$TMA_OUT/$f" "$DIST/"
    staged_runtime=$((staged_runtime + 1))
  else
    log "  note: $f is not in $TMA_OUT; skipped"
  fi
done
# Locale .pak files. Small, and without them Chromium falls back to en-US.
if [[ -d "$TMA_OUT/locales" ]]; then
  rm -rf "${DIST:?}/locales"
  cp -a "$TMA_OUT/locales" "$DIST/"
fi

# The setuid sandbox helper is optional: without it Chromium uses the
# Linux user-namespace sandbox, which Fedora allows by default
# (kernel.unprivileged_userns_clone does not exist outside Debian/Ubuntu, and
# /proc/sys/user/max_user_namespaces is non-zero on stock Fedora). Chromium
# only looks for this file; if it is absent it falls back on its own.
if [[ -f "$TMA_OUT/chrome-sandbox" ]]; then
  cp -f "$TMA_OUT/chrome-sandbox" "$DIST/chrome-sandbox"
  chmod 4755 "$DIST/chrome-sandbox"
fi

raw="$(bytes "$DIST/tma")"
log "stripping $DIST/tma"
strip "$DIST/tma"
stripped="$(bytes "$DIST/tma")"

log ""
log "  tma, unstripped   $raw"
log "  tma, stripped     $stripped"
log "  runtime files     $staged_runtime (+ locales/ if present)"

if [[ "$do_xz" == 1 ]]; then
  command -v xz >/dev/null || die "xz not found"
  log "  compressing (xz -9), replaces $DIST/tma ..."
  xz -9f "$DIST/tma"
  log "  tma.xz            $(bytes "$DIST/tma.xz")"
fi

log ""
log "  content_shell.pak $(bytes "$DIST/content_shell.pak")"
log "  tma_resources     $(du -sh --apparent-size "$DIST/tma_resources" | cut -f1)"
log ""
log "total: $(du -sh --apparent-size "$DIST" | cut -f1) in $DIST"
if [[ -x "$DIST/tma" ]]; then
  log "run it with: $DIST/tma   (needs a Wayland session)"
else
  log "unpack first: xz -dk $DIST/tma.xz"
fi
