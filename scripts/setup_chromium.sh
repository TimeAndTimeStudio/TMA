#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# Bootstraps a Chromium checkout with TMA grafted onto it.
#
# Chromium is pinned to the tag in ../CHROMIUM_VERSION (a released
# Chrome/Brave version, not ToT). Re-running is idempotent: it syncs deps,
# re-pins if the checkout moved, and re-grafts tma/.
#
# This downloads source code and therefore has to be run by a developer on a
# machine that already has a toolchain (see README.md for the Fedora package
# list). It is NOT meant to be run from inside an agent or CI sandbox.
#
# Usage:
#   scripts/setup_chromium.sh [chromium-src-dir]
#
# Environment:
#   CHROMIUM_SRC   Chromium src/ directory. Default: $HOME/chromium/src
#   DEPOT_TOOLS    depot_tools checkout.  Default: $HOME/depot_tools
#   TMA_OUT        GN output directory.   Default: <chromium-src>/out/Default
#   CHROMIUM_TAG   Override the pinned tag (otherwise read from CHROMIUM_VERSION)

set -euo pipefail

log() { printf '\n==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# TMA is Linux and Wayland only, and //tma/BUILD.gn asserts the same two facts
# at GN time. Fail here instead, before gclient sync downloads a tree that gn
# gen would then reject: Chromium would otherwise be fetched in full on a host
# where this checkout can never be built.
case "$(uname -s)" in
  Linux) ;;
  *) die "TMA only supports Linux (this host is $(uname -s))" ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMA_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CHROMIUM_SRC="${1:-${CHROMIUM_SRC:-$HOME/chromium/src}}"
DEPOT_TOOLS="${DEPOT_TOOLS:-$HOME/depot_tools}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

# --- application identity from tma.conf --------------------------------------
# tma.conf is the one place app_name / app_id / startup / window floor are
# written down. It is read and checked before any download, copy or patch, so
# that a bad config fails in under a second instead of after a gclient sync;
# the values become GN arguments further down, next to gn gen. //tma/BUILD.gn
# cannot read the file itself because the checkout is a copy of this repo made
# at setup time, so a config-only edit has to reach that gn gen invocation to
# have any effect at all.
source "$SCRIPT_DIR/tma_conf.sh"

_tma_app_name="$(tma_conf_get app_name)" || _tma_app_name=""
_tma_app_id="$(tma_conf_get app_id)" || _tma_app_id=""
_tma_startup="$(tma_conf_get startup)" || _tma_startup=""
_tma_version="$(tma_conf_get version)" || _tma_version=""
_tma_min_width="$(tma_conf_get min_width)" || _tma_min_width=""
_tma_min_height="$(tma_conf_get min_height)" || _tma_min_height=""

# A missing value falls back to the stock identity, but a present value that is
# not usable dies here. app_name becomes the name of the binary on disk, so
# substituting a name silently would resurface much later as a missing file
# instead of as its cause -- and its case matters there, unlike in app_id, so
# it gets the lowercase pattern rather than tma_conf_valid_name.
if [[ -z "$_tma_app_name" ]]; then
  _tma_app_name="tma"
elif ! tma_conf_valid_app_name "$_tma_app_name"; then
  die "tma.conf: app_name '$_tma_app_name' must be lowercase: start with a letter or digit, then only lowercase letters, digits, '.', '_' and '-'"
fi

[[ -n "$_tma_app_id" ]] || _tma_app_id="$_tma_app_name"
if ! tma_conf_valid_name "$_tma_app_id"; then
  die "tma.conf: app_id '$_tma_app_id' must start with a letter or digit and use only letters, digits, '.', '_' and '-'"
fi

# version names the shell itself. It is the one key with no sensible default:
# falling back to a number would make --version claim to be something it is
# not, so an empty value dies here instead.
if [[ -z "$_tma_version" ]]; then
  die "tma.conf: version is required -- it is what --version reports"
elif ! [[ "$_tma_version" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]*$ ]]; then
  die "tma.conf: version '$_tma_version' must start with a letter or digit and use only letters, digits, '.', '_', '+' and '-'"
fi

# min_width / min_height become the compile-time floor TmaView::GetMinimumSize()
# returns, so a value that is not a positive integer would reach the window as
# zero and look like a bug rather than a typo. Checked here for the same reason
# startup is: in a second, not after a build. `[1-9][0-9]*` rejects leading
# zeros as well, so there is no octal surprise to guard against later.
if [[ -z "$_tma_min_width" ]]; then
  _tma_min_width="320"
elif ! [[ "$_tma_min_width" =~ ^[1-9][0-9]*$ ]]; then
  die "tma.conf: min_width '$_tma_min_width' must be a positive integer"
fi
if [[ -z "$_tma_min_height" ]]; then
  _tma_min_height="200"
elif ! [[ "$_tma_min_height" =~ ^[1-9][0-9]*$ ]]; then
  die "tma.conf: min_height '$_tma_min_height' must be a positive integer"
fi

# startup is the one key whose format this script owns, so it is checked here:
# catching a typo costs a second, catching it later costs a build plus a window
# that opens the wrong thing, or nothing at all. The prefix is stripped here as
# well rather than in //tma/browser, so the C++ side never interprets a string
# (which also keeps clang from constant-folding a branch and failing the build
# on -Wunreachable-code): setup hands the URL over on its own.
_tma_startup_url=""
case "$_tma_startup" in
  "")      die "tma.conf: startup is required -- set 'url:<url>'" ;;
  url:?*)  _tma_startup_url="${_tma_startup#url:}" ;;
  url:)    die "tma.conf: startup 'url:' has no URL after it" ;;
  file)    die "tma.conf: startup 'file' is gone -- TMA has no packaged page; set 'url:<url>'" ;;
  *)       die "tma.conf: startup must be 'url:<url>' -- got '$_tma_startup'" ;;
esac

# The prefix is only half of a URL. TmaBrowserMainParts asks GURL whether the
# value is usable, and one with no scheme fails there and opens the "bad
# startup" page instead of the site. Catching it here turns a runtime surprise
# into a message that names the key.
if ! [[ "$_tma_startup_url" =~ ^[A-Za-z][A-Za-z0-9+.-]*: ]]; then
  die "tma.conf: startup URL '$_tma_startup_url' has no scheme -- write url:<scheme>://..."
fi

log "identity: version=$_tma_version app_name=$_tma_app_name app_id=$_tma_app_id"
log "startup:   $_tma_startup"
log "min size:  ${_tma_min_width}x${_tma_min_height}"

# --- depot_tools ------------------------------------------------------------
if [[ ! -d "$DEPOT_TOOLS" ]]; then
  log "Cloning depot_tools into $DEPOT_TOOLS"
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git \
    "$DEPOT_TOOLS"
fi
export PATH="$DEPOT_TOOLS:$PATH"
command -v fetch >/dev/null || die "depot_tools is not on PATH"

# --- Chromium checkout ------------------------------------------------------
# Pinned to a released tag, not ToT: CHROMIUM_VERSION is the single source of
# truth for which Chromium a given TMA revision builds against (see README).
CHROMIUM_TAG="${CHROMIUM_TAG:-$(tr -d '[:space:]' < "$TMA_ROOT/CHROMIUM_VERSION")}"
[[ -n "$CHROMIUM_TAG" ]] || die "CHROMIUM_VERSION is empty"

if [[ ! -e "$CHROMIUM_SRC/.gclient" && ! -e "$(dirname "$CHROMIUM_SRC")/.gclient" ]]; then
  log "Fetching Chromium into $(dirname "$CHROMIUM_SRC")"
  mkdir -p "$(dirname "$CHROMIUM_SRC")"
  (cd "$(dirname "$CHROMIUM_SRC")" && fetch --no-history chromium)
fi

[[ -e "$CHROMIUM_SRC/.gn" ]] || die "no Chromium source tree at $CHROMIUM_SRC"

# --- pin to the released tag ------------------------------------------------
# `fetch` lands on main/HEAD; a stable Chrome/Brave release tag is what we
# actually build. Re-running is a no-op once the checkout is on the tag.
at="$(git -C "$CHROMIUM_SRC" describe --tags --exact-match 2>/dev/null || true)"
if [[ "$at" != "$CHROMIUM_TAG" ]]; then
  if [[ -n "$at" ]]; then
    log "Pinning Chromium $at -> $CHROMIUM_TAG"
  else
    log "Pinning Chromium $(git -C "$CHROMIUM_SRC" rev-parse --short HEAD) -> $CHROMIUM_TAG"
  fi
  # --depth=1 is not optional here. The checkout is already shallow (fetch
  # --no-history), and in a shallow repository a plain `git fetch` of a new ref
  # asks for that ref's *complete* history -- for Chromium that is tens of GB.
  git -C "$CHROMIUM_SRC" fetch --no-tags --depth=1 origin \
    "refs/tags/$CHROMIUM_TAG:refs/tags/$CHROMIUM_TAG" ||
    die "no such Chromium tag: $CHROMIUM_TAG"
  git -C "$CHROMIUM_SRC" checkout --force --detach "refs/tags/$CHROMIUM_TAG"
else
  log "Chromium already at $CHROMIUM_TAG"
fi

log "Chromium $(git -C "$CHROMIUM_SRC" describe --tags --always)"

# --- deps for exactly that revision -----------------------------------------
# gclient sync is idempotent and resumes from where it stopped, so retrying a
# transient failure costs only time. googlesource answers HTTP 429 "Short term
# server-time rate limit exceeded" when too many repos are fetched at once;
# --jobs=4 keeps the parallel fetch count below that shared quota.
log "gclient sync --no-history --jobs=4"
sync_ok=0
for attempt in 1 2 3; do
  if (cd "$(dirname "$CHROMIUM_SRC")" && gclient sync --no-history --jobs=4); then
    sync_ok=1
    break
  fi
  log "gclient sync failed (attempt $attempt/3); retrying in 60s"
  sleep 60
done
((sync_ok)) || die "gclient sync failed after 3 attempts"

# --- graft TMA --------------------------------------------------------------
log "Copying TMA into $CHROMIUM_SRC/tma"
mkdir -p "$CHROMIUM_SRC/tma"
# rsync would drop the .git-less tree just as well; cp keeps this dependency-free.
rm -rf "$CHROMIUM_SRC/tma"
cp -a "$TMA_ROOT/tma" "$CHROMIUM_SRC/tma"
rm -rf "$CHROMIUM_SRC/tma/out"

# --- hook //tma into the root build file -------------------------------------
# GN discovers BUILD.gn files only by following dependency paths from //BUILD.gn
# ("So if you add a new build file, there must be some path of dependencies
# from this file to your new one or GN won't know about it"), so an otherwise
# unreferenced //tma stays invisible and `ninja tma` answers "unknown target".
# One group at the end of the root file supplies that path.
#
# The group is named tma_hook, not tma: a root group would emit a phony output
# called `tma`, which collides with the `./tma` produced by the executable and
# makes every ninja run fail with "multiple rules generate tma". The marker
# comment keeps the append idempotent; `git checkout --force` during pinning
# drops the patch, which is why it is re-applied on every graft.
ROOT_BUILD="$CHROMIUM_SRC/BUILD.gn"
ROOT_BUILD_MARK="# --- TMA hook (added by scripts/setup_chromium.sh) ---"
ROOT_BUILD_DEPS='  deps = [ "//tma:tma" ]'
if grep -qF "$ROOT_BUILD_MARK" "$ROOT_BUILD"; then
  # The marker alone is not enough: a hand edit (or a half-applied patch) can
  # leave the block present but pointing at a target that does not exist, which
  # surfaces much later as `Unresolved dependencies: //tma:...` out of gn gen.
  # Repair in place so the failure mode is one warning instead of a dead build.
  if ! grep -qxF "$ROOT_BUILD_DEPS" "$ROOT_BUILD"; then
    log "Root //BUILD.gn hook is malformed; repairing its deps line"
    sed -i -e "s|^  deps = \[ \"//tma:[^\"]*\" \]$|$ROOT_BUILD_DEPS|" "$ROOT_BUILD"
  fi
  if ! grep -qxF "$ROOT_BUILD_DEPS" "$ROOT_BUILD"; then
    die "$ROOT_BUILD has the TMA hook marker but no //tma:tma deps line"
  fi
  log "Root //BUILD.gn already hooks in //tma"
else
  log "Hooking //tma into the root //BUILD.gn"
  cat >> "$ROOT_BUILD" <<'EOF'

# --- TMA hook (added by scripts/setup_chromium.sh) ---
# GN only loads BUILD.gn files it can reach from this file, so the out-of-tree
# //tma tree is referenced here. Delete this block to unhook it.
group("tma_hook") {
  testonly = true
  deps = [ "//tma:tma" ]
}
EOF
fi

# --- drop the expand_directory allowlist a previous setup added ---------------
# //tma/BUILD.gn used to enumerate tma/resources/ with expand_directory(), and
# GN allowlists that per build file so a stray directory cannot pull thousands
# of files into the build. There is no tma/resources/ any more, so the entry is
# removed rather than left behind -- the marker line is what this looks for,
# which keeps it idempotent.
GN_DOTFILE="$CHROMIUM_SRC/.gn"
EXPAND_MARK="# --- TMA lists tma/resources (added by scripts/setup_chromium.sh) ---"
if grep -qF "$EXPAND_MARK" "$GN_DOTFILE"; then
  log "Dropping the stale expand_directory allowlist entry in //.gn"
  python3 - "$GN_DOTFILE" "$EXPAND_MARK" <<'PYPATCH'
import sys

path, mark = sys.argv[1], sys.argv[2]
kept = []
for line in open(path, encoding="utf-8"):
    if mark in line or line.strip() == '"//tma/BUILD.gn",':
        continue
    kept.append(line)
open(path, "w", encoding="utf-8").write("".join(kept))
PYPATCH
fi

# --- trim the resource pack that ships ---------------------------------------
# The embedder always loads `content_shell.pak` from next to the executable
# (//content/shell/app/shell_main_delegate.cc:400,422) and GN rejects any two
# targets with the same output, so TMA cannot define its own repack — even
# though //content/test:test_support already drags //content/shell:pak into the
# graph through content_shell_lib. The only way to change what ships is to
# change that one repack.
#
# Both anchors below are single-occurrence lines of this pinned Chromium
# revision; if a bump ever moves them the script fails loudly instead of
# producing a pack with the wrong contents. Assigning `sources`/`deps` after
# the original list has been accumulated replaces it wholesale, which keeps the
# edit to two inserted lines rather than a dozen deletions that would drift.
# The marker keeps it idempotent, and `git checkout --force` during pinning
# drops it, so it is re-applied on every graft — same pattern as the root hook.
SHELL_BUILD="$CHROMIUM_SRC/content/shell/BUILD.gn"
PAK_MARK="# --- TMA trimmed resource pack (added by scripts/setup_chromium.sh) ---"
if grep -qF "$PAK_MARK" "$SHELL_BUILD"; then
  log "//content/shell:BUILD.gn already ships TMA's resource list"
else
  log "Trimming content_shell.pak down to TMA's resource list"
  python3 - "$SHELL_BUILD" "$PAK_MARK" <<'PY'
import sys

path, mark = sys.argv[1], sys.argv[2]
text = open(path, encoding="utf-8").read()

anchor_import = "shell_use_toolkit_views = toolkit_views && !is_castos"
if anchor_import not in text:
    sys.exit("setup: import anchor missing from %s" % path)
text = text.replace(anchor_import, 'import("//tma/tma_pak.gni")\n' + anchor_import, 1)

anchor_output = '  output = "$root_out_dir/content_shell.pak"'
if anchor_output not in text:
    sys.exit("setup: repack anchor missing from %s" % path)
block = (
    "  " + mark + "\n"
    "  # TMA carries no DevTools front-end, no test fixtures and none of the\n"
    "  # chrome:// internals pages. See //tma/tma_pak.gni for the list and why.\n"
    "  # GN refuses to overwrite a nonempty list, so clear it first.\n"
    "  sources = []\n"
    "  sources += tma_pak_sources\n"
    "  deps = []\n"
    "  deps += tma_pak_deps\n"
)
text = text.replace(anchor_output, block + anchor_output, 1)

open(path, "w", encoding="utf-8").write(text)
PY
fi

# --- compile ShellFileSelectHelper on Linux ---------------------------------
# content shell has no file chooser on Linux. Shell::RunFileChooser hands off
# to ShellPlatformDelegate::RunFileChooser, whose generic implementation just
# calls listener->FileSelectionCanceled()
# (//content/shell/browser/shell_platform_delegate.cc:51), so <input
# type="file"> opened nothing at all until TmaPlatformDelegate started routing
# it through ShellFileSelectHelper.
#
# The helper itself is platform-agnostic -- no IS_IOS, IS_MAC, UIKit or Cocoa
# anywhere in it -- and it sits under `if (is_ios)` only because
# shell_platform_delegate_ios.mm:741 is its one caller upstream. Adding it to
# the shared list does not collide with that block, which GN never evaluates
# for a Linux build.
#
# The dialog it drives lands on ui::SelectFileDialogLinuxPortal through
# //ui/shell_dialogs/shell_dialog_linux.cc under BUILDFLAG(USE_DBUS), the same
# freedesktop portal TMA already reads the desktop color scheme from.
FILESEL_MARK="# --- TMA compiles ShellFileSelectHelper (added by scripts/setup_chromium.sh) ---"
if grep -qF "$FILESEL_MARK" "$SHELL_BUILD"; then
  log "//content/shell:BUILD.gn already compiles ShellFileSelectHelper"
else
  log "Compiling content/shell's ShellFileSelectHelper on Linux"
  python3 - "$SHELL_BUILD" "$FILESEL_MARK" <<'PY'
import sys

path, mark = sys.argv[1], sys.argv[2]
text = open(path, encoding="utf-8").read()

anchor = '    "browser/shell_platform_delegate.h",'
if text.count(anchor) != 1:
    sys.exit("setup: file-select anchor count is %d in %s"
             % (text.count(anchor), path))

helper = (
    "    " + mark + "\n"
    '    "browser/shell_file_select_helper.cc",\n'
    '    "browser/shell_file_select_helper.h",\n'
)
text = text.replace(anchor, helper + anchor, 1)

open(path, "w", encoding="utf-8").write(text)
PY
fi

# --- generate ---------------------------------------------------------------
# Wayland-only: Ozone is narrowed down to the Wayland platform so that TMA
# cannot silently fall back to X11 or the headless platform.
#
# The rest switches off features a display shell rendering a local page over
# file:// can never reach, so their subtrees leave the build graph entirely.
# Measured on the `ninja -n tma` closure this removes 509 object files out of
# 37,849 (~1.3%): mostly third_party codecs (-298), then device (-74),
# components (-35), media (-28), content (-27), net (-22), gpu (-21).
#
# Three bigger wins are deliberately absent, each verified against this
# checkout rather than assumed:
#
#   enable_devtools_frontend   1,428 edges. Not a build argument -- content/
#                              browser/devtools/features.gni assigns it as a
#                              plain global, so GN ignores it in --args.
#   safe_browsing_mode = 0     ~1,500 edges. Root //BUILD.gn loads //chrome/test
#                              and //chrome/browser targets whose deps vanish;
#                              gen fails with "Unresolved dependencies" for
#                              components/safe_browsing/*.
#   enable_captive_portal...   same failure mode on
#                              //chrome/browser/captive_portal.
#
# enable_printing, enable_pdf, enable_extensions, enable_supervised_users and
# enable_platform_apps are off-limits for the same reason (BUILD.gn asserts in
# //chrome/test, //extensions and //components/printing). None of them are in
# the closure of //tma anyway -- printing contributes 28 objects, pdfium and
# chrome/ contribute 0 -- so leaving them on costs nothing measurable.
#
# One more must stay at its default even though it looks unused by a clock:
#
#   enable_gpu_channel_media_capture  its default is `is_linux || ...` (= true
#                              here). Setting it false compiles the flag out of
#                              media_buildflags.h, but
#                              content/browser/video_capture_service_impl.cc
#                              guards the #include of gpu_channel_host.h with
#                              BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE) while
#                              leaving the `scoped_refptr<gpu::GpuChannelHost>`
#                              parameter on line 79 unguarded, so Linux breaks
#                              with "no member named 'GpuChannelHost'". Keep it.
#
# And one that fails at link time rather than compile time:
#
#   enable_websockets = false  removes net/server/http_server.cc from //net
#                              (net/BUILD.gn:1768), but
#                              content/browser/devtools/devtools_http_handler.cc
#                              and devtools_pipe_handler.cc still call
#                              net::HttpServer with no ENABLE_WEBSOCKETS guard
#                              around them. The link then reports 26 undefined
#                              net::HttpServer / HttpConnection / HttpServer*
#                              symbols, all referenced only from those two
#                              files. Turning WebSockets off would only be safe
#                              once the DevTools HTTP endpoint goes too, which
#                              needs a source patch.
#
GN_ARGS=$(cat <<'EOF'
# --- where the build runs ----------------------------------------------------
# TMA never uses Remote Build Execution. Set explicitly rather than left to the
# default, because this is what makes autoninja pass `siso --offline`: no RBE
# and no remote cache are contacted, and every step compiles on this machine.
# See README, Network.
use_remoteexec = false

# --- build type ---------------------------------------------------------------
is_debug = false
symbol_level = 0
blink_symbol_level = 0

# --- platform: Wayland only ---------------------------------------------------
ozone_auto_platforms = false
ozone_platform = "wayland"
ozone_platform_wayland = true

# No X11 anywhere. ozone_platform_x11 already defaults to false and
# ozone_auto_platforms=false keeps it that way, so //ui/ozone/platform/x11 and
# //ui/x11 leave the graph entirely. Two system libraries still showed up in
# `ldd` after that though -- libX11/libXext/libXrender/libXi -- and they are
# not Chromium's X11 backend at all: libatk-bridge-2.0, libatspi and libcairo
# each dlopen an X11 backend of their own. They arrive because use_gtk and
# use_glib default to true on Linux, which pulls //build/config/linux/atk's
# pkg_config("atk") into the link.
#
# TMA never shows a GTK widget and never talks to AT-SPI (the binary has no
# libgtk or libgdk in `ldd` even with these on), so dropping them removes the
# X11 libraries from the process without losing anything TMA uses. The cost is
# that a screen reader will not find TMA; if that ever matters, put these two
# back and accept libX11 in the address space.
use_gtk = false
use_glib = false

# WebRTC's portal screencast (modules/portal/xdg_session_details.h) includes
# <gio/gio.h> unconditionally, and the -I paths for glib only exist while
# use_glib is true -- so the two above alone fail to compile
# content/public/browser/desktop_capture.cc. rtc_use_pipewire is what pulls
# //third_party/webrtc/modules/portal into the graph (webrtc_overrides/BUILD.gn
# :195), and TMA never captures the screen, so turning it off removes both the
# portal code and the video_capture targets it drags along.
rtc_use_pipewire = false

# --- WebGPU -------------------------------------------------------------------
# //third_party/dawn plus its SPIR-V tooling, ~740 edges.
#
# use_dawn already defaults true on Linux (ui/gl/features.gni) and Dawn's own
# backends -- Vulkan, desktop GL, GLES, SwiftShader -- all default on
# (third_party/dawn/scripts/dawn_features.gni), so the WebGPU implementation is
# in the graph either way. What this one adds is Skia's Dawn backend, and it is
# not optional: on Linux gpu/command_buffer/service/webgpu_decoder_impl.cc only
# offers WebGPU a real backend when the Skia context is Vulkan-backed, and
# Skia on Vulkan is a path that does not exist while Skia's Dawn side is
# compiled out. Ask for a WebGPU adapter otherwise and you get Null, i.e. no
# adapter at all.
skia_use_dawn = true

# --- remoting -----------------------------------------------------------------
# Chrome Remote Desktop and its crashpad component.
enable_remoting = false
enable_chromoting_crashpad = false

# --- network features TMA does not use ---------------------------------------
# Network Reporting API (net/features.gni).
enable_reporting = false
# mDNS/DNS-SD.
# Cast/remote media playback.
enable_media_remoting = false
enable_media_remoting_rpc = false

# --- platform features TMA does not have -------------------------------------
enable_vr = false
enable_openxr = false
enable_hosted_apps = false
enable_session_service = false
enable_paint_preview = false
enable_screen_ai_browsertests = false
enable_browser_speech_service = false
enable_message_center = false
enable_chrome_notifications = false
enable_library_cdms = false

# --- codecs TMA never decodes -------------------------------------------------
# AV1 (libaom + dav1d, ~330 edges) and JPEG XL.
enable_av1_decoder = false
enable_dav1d_decoder = false
enable_libaom = false
enable_jxl_decoder = false

# --- CUPS ---------------------------------------------------------------------
use_cups = false

# --- host tooling, installer, CI leftovers ------------------------------------
enable_gwp_asan = false
enable_pseudolocales = false
enable_linux_installer = false
enable_nocompile_tests = false
enable_perfetto_unittests = false
media_use_ffmpeg = false
media_use_libvpx = false
media_use_symphonia = false
media_use_openh264 = false
use_vaapi = false
use_pulseaudio = false
use_alsa = false
enable_disk_cache_sql_backend = false
enable_widevine = false
enable_hls_demuxer = false
enable_mse_mpeg2ts_stream_parser = false
enable_media_drm_storage = false
enable_cdm_storage_id = false
enable_offline_pages = false
enable_hangout_services_extension = false
enable_rlz = false
enable_cardboard = false
enable_cast_receiver = false

# --- TMA arg sweep ----------------------------------------------------------
# Swept every GN arg that defaults to true (367 of them) through `gn gen` +
# closure/object counts.  The block below is what survived a real build and a
# real Wayland run.  Do NOT "improve" these without rebuilding: several flags
# that look harmless break Chromium's compile/link guards.  The traps:
#
#   enable_vulkan              MUST stay true.  gpu/command_buffer/service
#                              compiles dawn_ozone_image_representation.cc
#                              whenever `use_dawn && use_ozone`, but the
#                              //third_party/libsync dep that supplies
#                              <sync/sync.h> is inside `if (enable_vulkan)`.
#                              Setting it false = "file not found".
#   rtc_build_libsrtp          MUST stay true -> 17 undefined `srtp_*` symbols
#                              at link time (webrtc calls them unguarded).
#   is_p2p_enabled             MUST stay true -> blink's mediastream loses
#                              -Ithird_party/webrtc entirely.
#   rtc_use_pipewire           now false on purpose (see the X11 block above).
#                              It used to be listed here as MUST-stay-true
#                              because it drops pipewire_config from
#                              //third_party/webrtc_overrides:webrtc_component;
#                              nothing in //tma's closure reads that config, and
#                              it is the only way to lose WebRTC's gio include.
#   tint_build_glsl_writer     MUST stay true -> Dawn's GL backend needs
#                              tint::glsl / tint::null (msl/hlsl are safe off).
#   enable_dawn                not a real arg at all (GN warns "no effect").
#   enable_perfetto_*_trace_*  assert()ed inside third_party/perfetto.
#   enable_fieldtrial_testing_ Use `disable_fieldtrial_testing_config` instead.
#
# compiler / codegen
is_official_build = true
chrome_pgo_phase = 0
is_cfi = false
optimize_for_size = true
dcheck_always_on = false
v8_dcheck_always_on = false
enable_expensive_dchecks = false
partition_alloc_dcheck_always_on = false
allow_avx512 = false

# --- ANGLE --------------------------------------------------------------------
# ANGLE cannot be left out: gpu/ipc/service/gpu_init.cc:516 initializes GL on
# every GPU process start, and there is no switch that skips that call without
# also losing WebGPU -- --use-gl=disabled leaves the page reporting "Failed to
# create WebGPU Context Provider". So ANGLE stays built, and the backend of
# that call is chosen at run time by --use-angle=vulkan, injected by
# TmaMainDelegate. It resolves to Vulkan instead of system EGL, which is what
# keeps Mesa's stack (libEGL.so.1, libEGL_mesa, libgallium, libLLVM) out of
# the process.
#
# The Vulkan switches are not written down here. angle.gni already computes
# angle_enable_vulkan, angle_shared_libvulkan and angle_use_custom_libvulkan
# to the values this build would have set on Linux -- angle_use_wayland
# defaults to true, which is what angle_enable_vulkan turns on -- so stating
# them again would only buy a rebuild for a sentence. WebGPU reaches Vulkan
# through skia_use_dawn and --enable-features=Vulkan, neither of which is
# ANGLE's.

# Development tooling a display shell never loads: ANGLE's WebGPU bridge,
# Vulkan validation layers, perfetto tracing, and the debug-layer asserts.
angle_enable_wgpu = false
angle_enable_vulkan_validation_layers = false
angle_enable_perfetto = false
angle_debug_layers_enabled = false
angle_assert_always_on = false

# ASTC codec sources for SwiftShader only -- the software renderer runs
# without them.
swiftshader_enable_astc = false

# on-device AI / ML (Chromium documents these as "exclude ... due to binary size")
use_on_device_model_service = false
enable_constraints = false
webnn_use_litert = false
build_tflite_with_xnnpack = false
enable_compute_pressure = false

# devtools: skip the closure bundle step (~900 targets, 51 more from WebUI)
devtools_bundle = false
optimize_webui = false

# WebRTC pieces TMA never exercises
rtc_enable_sctp = false
rtc_include_dav1d_in_internal_decoder_factory = false
chrome_wide_echo_cancellation_supported = false
media_use_iamf_tools = false

# rendering extras we do not use
enable_vrp_flags = false
tint_build_msl_writer = false
tint_build_hlsl_writer = false

# V8 / perfetto trims (only flags perfetto does not assert() on)
v8_enable_temporal_support = false
v8_use_perfetto = false
use_v8_context_snapshot = false
enable_trace_logging = false
optional_trace_events_enabled = false
enable_perfetto_zlib = false
enable_perfetto_re2 = false
enable_perfetto_system_consumer = false
enable_perfetto_grpc = false
enable_perfetto_x64_cpu_opt = false

# net / enterprise / misc
enterprise_proxy = false
enable_device_bound_sessions = false
include_transport_security_state_preload_list = false
disable_brotli_filter = true
enable_oop_printing = false
use_clang_modules = false
use_qt5 = false
use_qt6 = false
disable_fieldtrial_testing_config = false
EOF
)

# The values read at the top of this script, escaped for a GN string literal.
# Everything else about them is already validated by now.
GN_ARGS="$GN_ARGS
tma_app_name = \"$(tma_conf_gn_escape "$_tma_app_name")\"
tma_app_id = \"$(tma_conf_gn_escape "$_tma_app_id")\"
tma_version = \"$(tma_conf_gn_escape "$_tma_version")\"
tma_startup_url = \"$(tma_conf_gn_escape "$_tma_startup_url")\"tma_min_width = $_tma_min_width
tma_min_height = $_tma_min_height
# The tag the checkout was pinned to two screens ago, which is what the
# compiled Chromium actually is -- not merely what CHROMIUM_VERSION asked for.
tma_chromium_version = \"$CHROMIUM_TAG\"
"

log "gn gen $TMA_OUT"
# Run from inside src/, not from the TMA tree: depot_tools' gn shim resolves
# buildtools/linux64/gn relative to the solution directory, and from outside
# the checkout it falls through to a PATH lookup that only finds the shim
# itself ("Unable to find gn in your $PATH").
(cd "$CHROMIUM_SRC" && gn gen "$TMA_OUT" --args="$GN_ARGS")

log "Done. Next: ./build.sh build"
