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
# SCOPE. The list below covers exactly two things:
#
#   scripts/setup_chromium.sh   depot_tools -> fetch -> pin -> gclient sync
#                               -> graft tma/ -> gn gen
#   scripts/build_tma.sh        autoninja -C <out> tma
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
#   dnf5-plugins    needed only by the optional `dnf builddep` shortcut in
#                   README section 2.4, not by ./build.sh.

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

# The required host toolchain. See the scope note at the top of this file and
# README section 2.2 for the justification of each entry.
#
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
#                        is a requirement of TMA in its own right (README 1).
#   tar                  on every Fedora desktop image, and TMA requires a
#                        Wayland desktop session regardless (README section 1).
REQUIRED=(
  git          # fetch, gclient, pinning to the CHROMIUM_VERSION tag
  binutils     # `strip`, used once by scripts/package_tma.sh
)

# DELIBERATELY DROPPED from upstream's install-build-deps.py dev_list, each
# verified rather than assumed. The check was to shadow the tool with a stub
# that prints BLOCKED and exits 1, then run `./build.sh setup` and rebuild
# every target those tools feed -- a build with zero BLOCKED lines proves the
# tool is never invoked.
#
#   perl   `grep -c` over `ninja -t commands tma` (39,479 lines) is 0, and
#          there is no perl rule in out/Default/toolchain.ninja at all.
#   bison  same: 0 commands, 0 rules. The `*.y` parsers in third_party ship
#          their generated output, and TMA's closure reaches none of the
#          remaining generators.
#   flex   same: 0 commands, 0 rules (the 7 apparent `flex` hits are
#          `flex_layout_algorithm.o` -- filenames, not the lexer).
#   gperf  Chromium carries its own at third_party/gperf/cipd/bin/gperf, and
#          that is the path every rule names; system gperf is never read.
#          (The `*.gperf` inputs under net/ and components/ are consumed by
#          net/tools/dafsa/make_dafsa.py, which is Python and calls no gperf.)
#
# Not dropped, because it really is used:
#
#   binutils  ar, ld, nm and objcopy all come from the hermetic
#             third_party/llvm-build toolchain (llvm-ar, lld, llvm-nm,
#             llvm-objcopy), so only `strip` needs the system copy -- but
#             scripts/package_tma.sh:111 does call it, and there is no
#             llvm-strip wrapper in this repo yet.


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

