#!/usr/bin/env bash
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMA_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CHROMIUM_SRC="${1:-${CHROMIUM_SRC:-$HOME/chromium/src}}"
DEPOT_TOOLS="${DEPOT_TOOLS:-$HOME/depot_tools}"
TMA_OUT="${TMA_OUT:-$CHROMIUM_SRC/out/Default}"

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
if grep -qF "$ROOT_BUILD_MARK" "$ROOT_BUILD"; then
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
# See README.md section 5.
GN_ARGS=$(cat <<'EOF'
# --- build type ---------------------------------------------------------------
is_debug = false
symbol_level = 0
blink_symbol_level = 0

# --- platform: Wayland only ---------------------------------------------------
ozone_auto_platforms = false
ozone_platform = "wayland"
ozone_platform_wayland = true

# --- WebGPU -------------------------------------------------------------------
# //third_party/dawn plus its SPIR-V tooling, ~740 edges.

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
#   rtc_use_pipewire           MUST stay true -> drops pipewire_config from
#                              //third_party/webrtc_overrides:webrtc_component.
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

# Vulkan backends ANGLE does not need (TMA renders with GL on Wayland).
angle_enable_vulkan = false
angle_shared_libvulkan = false
angle_use_custom_libvulkan = false
angle_enable_wgpu = false
angle_enable_vulkan_validation_layers = false
angle_enable_perfetto = false
angle_debug_layers_enabled = false
angle_assert_always_on = false
skia_use_dawn = false
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

log "gn gen $TMA_OUT"
# Run from inside src/, not from the TMA tree: depot_tools' gn shim resolves
# buildtools/linux64/gn relative to the solution directory, and from outside
# the checkout it falls through to a PATH lookup that only finds the shim
# itself ("Unable to find gn in your $PATH").
(cd "$CHROMIUM_SRC" && gn gen "$TMA_OUT" --args="$GN_ARGS")

log "Done. Next: ./build.sh build"
