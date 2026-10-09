#!/usr/bin/env bash
# Owner: Time And Time Studio
# Date: 2026-10-09 08:50 +0700
# License: GPL-3.0-or-later

# Installs the Fedora packages TMA needs to build.
#
# Usage:
#   scripts/install_deps.sh            install whatever is missing
#   scripts/install_deps.sh --dry-run  report only; never calls sudo
#
# Idempotent: every package is probed with `rpm -q`, and `sudo dnf install`
# is invoked only for the ones that are genuinely absent.
#
# SCOPE. What each script in this repo reaches for off $PATH:
#
#   scripts/setup_chromium.sh   git, gclient, gn     (last two from depot_tools)
#   scripts/build_tma.sh        autoninja            (from depot_tools)
#   scripts/package_tma.sh      strip                (from binutils)
#   scripts/run_tma.sh          nothing -- it only execs build/tma
#
# gclient, gn and autoninja ship with depot_tools, which setup_chromium.sh
# installs itself, so no Fedora package provides them. That leaves git and
# binutils as the entire REQUIRED list below.
#
# It does not cover Chromium's test suite, 32-bit builds, cross builds or
# NaCl. If a step fails on a tool this list did not anticipate, install that
# tool and re-run — do not widen the list speculatively.
#
# DELIBERATELY NOT INSTALLED, and why:
#
#   gcc, g++        Chromium compiles every source file with the hermetic
#                   clang that `gclient runhooks` downloads into
#                   third_party/llvm-build/Release+Asserts. Fedora's own
#                   chromium BuildRequires does not name gcc either.
#   clang, lld, llvm  same reason; also pinned to an LLVM revision that
#                   Fedora's packages do not ship.
#   ninja-build     the build driver comes from the checkout
#                   (third_party/siso or third_party/ninja). Upstream's
#                   install-build-deps.py does not install ninja either.
#   make, cmake     upstream never installs them for a normal build; cmake
#                   appears only in its backwards_compatible_list() for NaCl.
#   nodejs          Chromium ships third_party/node. On Fedora the RPM is
#                   also not named `nodejs`, so probing it would force a
#                   pointless install.
#   which           build.sh and scripts/ use the bash builtin `command -v`.
#   *-devel headers the build passes --sysroot=<checkout>/build/linux/
#                   debian_bullseye_amd64-sysroot to cc, c++, as and ld, and
#                   points pkg-config at the same sysroot.
#   dnf5-plugins    needed only by the optional `dnf builddep` shortcut, not
#                   by ./build.sh.

set -euo pipefail

log() { printf '\n==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

DRY_RUN=0
case "${1:-}" in
  ''|--install) ;;
  --dry-run|--check) DRY_RUN=1 ;;
  -h|--help)
    printf 'usage: %s [--dry-run]\n' "$0"
    exit 0
    ;;
  *) die "unknown argument: $1 (try --dry-run)" ;;
esac

command -v dnf >/dev/null || die "dnf not found; this script is Fedora-only"
command -v rpm  >/dev/null || die "rpm not found; this script is Fedora-only"

# Not listed, because Fedora already provides them:
#
#   rpm, curl            Mandatory packages of the `@core` group ("Smallest
#                        possible installation"). `rpm` is additionally a hard
#                        precondition of this script (`command -v rpm`).
#   xz                   required by dracut, which every bootable Fedora
#                        system has. (Fedora's tar does not link liblzma
#                        itself -- it execs the external `xz` -- so xz really
#                        is needed; dracut is what guarantees it.)
#   pkgconf-pkg-config   required by kmod, fwupd-efi and gnome-control-center.
#   python3              required by gdm, gnome-shell, mutter, at-spi2-core
#                        and firewalld -- i.e. by every Wayland session, which
#                        TMA requires in its own right.
#   tar                  on every Fedora desktop image, and TMA requires a
#                        Wayland desktop session regardless.
REQUIRED=(
  git          # fetch, gclient, pinning to the CHROMIUM_VERSION tag
  binutils     # `strip`, used once by scripts/package_tma.sh
)

# HOW REQUIRED WAS SETTLED. Nothing here was taken on faith from
# upstream's install-build-deps.py dev_list. Each tool was shadowed by a stub
# that prints BLOCKED and exits 1, then the pipeline was run for real:
#
#   KEPT
#     git      ./build.sh setup    fails at "Pinning Chromium" --
#                                  "BLOCKED: git called unexpectedly",
#                                  "no such Chromium tag"
#     binutils ./build.sh package  fails at "stripping <dist>/tma"
#
#   DROPPED (setup succeeded and the build finished 905 steps with 0 failures
#   and 0 BLOCKED lines, so none of these is ever invoked)
#     perl     0 hits in `ninja -t commands tma` (39,479 lines); no perl rule
#              in out/Default/toolchain.ninja
#     bison    0 commands, 0 rules. The `*.y` parsers in third_party ship
#              their generated output, and TMA's closure reaches none of the
#              remaining generators.
#     flex     0 commands, 0 rules. The 7 apparent `flex` hits are
#              flex_layout_algorithm.o and friends -- filenames, not the lexer.
#     gperf    every rule names third_party/gperf/cipd/bin/gperf, the copy
#              Chromium ships. The `*.gperf` inputs under net/ and components/
#              go through net/tools/dafsa/make_dafsa.py, which is Python.
#
# git is never named on a command line in any of these scripts; it is reached
# through `gclient sync` plus the tag-pinning step, both of which call it.
#
# binutils stays for `strip` alone: ar, ld, nm and objcopy all come from the
# hermetic third_party/llvm-build toolchain (llvm-ar, lld, llvm-nm,
# llvm-objcopy), and scripts/package_tma.sh has no llvm-strip wrapper yet.

installed=()
missing=()
for p in "${REQUIRED[@]}"; do
  if rpm -q "$p" >/dev/null 2>&1; then
    installed+=("$p")
  else
    missing+=("$p")
  fi
done

# name + version-release, one per line
show() {
  local p
  for p in "$@"; do
    printf '    %-22s %s\n' "$p" \
      "$(rpm -q --qf '%{VERSION}-%{RELEASE}' "$p" 2>/dev/null || printf 'not installed')"
  done
}

if ((DRY_RUN)); then
  printf 'required %d   installed %d   missing %d\n' \
    "${#REQUIRED[@]}" "${#installed[@]}" "${#missing[@]}"
  if ((${#installed[@]} > 0)); then
    printf 'installed:\n'
    show "${installed[@]}"
  fi
  if ((${#missing[@]} > 0)); then
    printf 'would install:\n'
    show "${missing[@]}"
    exit 0
  fi
  exit 0
fi

if ((${#missing[@]} == 0)); then
  log "all ${#REQUIRED[@]} build dependencies already installed"
  show "${installed[@]}"
  exit 0
fi

log "installing ${#missing[@]} package(s)"
show "${missing[@]}"
sudo dnf install -y "${missing[@]}"

still=()
for p in "${missing[@]}"; do
  rpm -q "$p" >/dev/null 2>&1 || still+=("$p")
done
if ((${#still[@]} > 0)); then
  die "not available in your repos: ${still[*]}"
fi

log "build dependencies ready"
show "${REQUIRED[@]}"

