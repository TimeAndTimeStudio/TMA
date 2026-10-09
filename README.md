# TMA — Time Mini App

TMA is a **Linux display shell written in C++** that embeds **official Chromium**
directly. Chromium renders the application (HTML/CSS/JavaScript); TMA contributes
only a frame around it:

* a **28 px dark strip** (`#202124`) along the top, with **no text on it**;
* **minimise / maximise-restore / close** buttons, drawn as glyphs and centred as
  a group in the middle of that strip;
* **no visible border** — the web content runs edge to edge;
* an **invisible 8 px resize ring** (16 px squares in each corner) that replaces
  the border you cannot see;
* drag-to-move, drag-to-resize, double-click-to-maximise, **F11** for fullscreen.

No CEF. No Electron. No Chromium fork. TMA is a small `source_set` compiled
inside a Chromium checkout, reusing `content/shell` as the embedder layer, and it
has **no backend** — the page's own JavaScript does every HTTP request and every
WebSocket connection itself.

---

## Table of contents

1. [Quick start](#1-quick-start)
2. [Using TMA](#2-using-tma)
3. [Writing your own application](#3-writing-your-own-application)
4. [How it works](#4-how-it-works)
5. [Build dependencies](#5-build-dependencies-fedora)
6. [Building](#6-building)
7. [Packaging](#7-packaging)
8. [Build configuration](#8-build-configuration-argsgn)
9. [What cannot be removed](#9-what-cannot-be-removed-webrtc-and-perfetto)
10. [Troubleshooting](#10-troubleshooting)
11. [Status and limitations](#11-status-and-limitations)

---

## 1. Quick start

```bash
./build.sh
```

That is the whole workflow. One command, no arguments. It installs the missing
Fedora packages, fetches the pinned Chromium source and its toolchains, grafts
`tma/` into the checkout, generates the build graph and compiles everything.

The first run is heavy (Chromium source + toolchains + a full compile). Every
run after that is idempotent: it only rebuilds what actually changed, typically
40–50 s.

When it finishes it prints where the result is, e.g.:

```
==> 3/3 build

done

  binary     /home/you/chromium/src/out/Default/tma
  resources  /home/you/chromium/src/out/Default/tma_resources
  app page   /home/you/chromium/src/out/Default/tma_resources/index.html
  size       274M (strip it for shipping: scripts/package_tma.sh)

run it with:
  env WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 /home/you/chromium/src/out/Default/tma --no-sandbox
```

That printed directory is called **`<out>`** in the rest of this document, and
the binary inside it is `<out>/tma`. Run it, and you get a window showing a live
clock plus a demo page that exercises `fetch()` and `WebSocket` — see
[§2](#2-using-tma).

---

## 2. Using TMA

### 2.1 The window

TMA draws a **28 px dark strip** on top and nothing else — no border, no
bevel, no shadow, no title text. The drawing below is the *hit map*, which is
the part that is not obvious, because every boundary in it is invisible.

```
     ┌──────────────────────────────────────────────────────────┐  y = 0
     │▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒ -  []  X ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒│  y = 8
     │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ -  []  X ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
     │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  y = 28  strip ends
     ├─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤  (dashed: nothing is drawn here)
     │                                                          │
     │           web content — HTCLIENT, edge to edge           │
     │                                                          │
     │                                                          │
     └──────────────────────────────────────────────────────────┘
```

| Mark | Zone | Behaviour |
|---|---|---|
| `▒` | the outer 8 px ring, on **all four sides**, corners included | resize. Invisible, because there is no border left to paint it with. Each corner is a full 16×16 px square rather than just the ring, so the very corner of the window is grabbable |
| ` -  []  X ` | three caption buttons, 34 × 28 px each, **centred as a group** | minimise / maximise-restore / close. Checked **before** the resize ring, so a button keeps its full height even where it overlaps the top ring |
| `▓` | the rest of the strip | drag — move the window. Double-click to maximise / restore |
| blank | everything below y = 28 | your page. Edge to edge, including the left, right and bottom 8 px, because the client area is the whole window |

The **top 8 px of the window is a resize zone, not a drag zone** — it is inside
the strip, but `ResizeComponentAt()` is answered before the strip falls through
to `HTCAPTION`. The drag zone is therefore the strip from y = 8 downward,
excluding the buttons.

The buttons are centred rather than flush right so that the top-right corner and
the right edge stay clear for the resize ring: with them flush right, the 16 px
corner square sat *underneath* the close button and answered `HTCLOSE` instead
of `HTTOPRIGHT`.

* The strip is the **same colour** whether the window is focused or not, so it
  never changes shade when you switch workspaces.
* In fullscreen (`F11` or `--fullscreen`) the whole strip is removed and the
  client starts at y = 0.

### 2.2 Controls

| Input | What it does |
|---|---|
| Drag the strip below its top 8 px | move the window |
| Double-click the strip | maximise / restore |
| Drag a window **edge** | resize |
| Drag a window **corner** | resize both axes at once |
| `F11` | native fullscreen on / off (the strip disappears entirely) |
| `-` button | minimise |
| `[]` button | maximise / restore |
| `X` button | close |
| **Fullscreen** button on the demo page | HTML5 `requestFullscreen()`, mapped onto the same native fullscreen as `F11` |

The **resize ring is invisible**, because the frame has no border to draw it
with. It is 8 DIP wide, measured inward from the window perimeter, and each
corner is a 16×16 DIP square so the very corner is grabbable. That is
deliberately generous: without a visual cue you need a grip you can find without
aiming.

### 2.3 Command-line switches

| Switch | Meaning |
|---|---|
| `--url=<url-or-path>` | the application to show; a bare path or a relative path also works |
| `--window-size=<width>x<height>` | initial window size, default `1280x800` (minimum `320x200`) |
| `--fullscreen` | start already in native fullscreen; leave with `F11` |
| `--no-sandbox` | run without the setuid sandbox helper |

All of Chromium's own switches still work (`--enable-logging`, `--v=1`,
`--disable-gpu`, …).

```bash
<out>/tma --no-sandbox --url=https://example.com --window-size=800x600
<out>/tma --no-sandbox --url=file:///path/to/myapp/index.html
```

### 2.4 What the demo page does

`tma/resources/index.html` (copied next to the binary as `tma_resources/`) is
the default application. It is a working example of everything TMA promises:

| Section on the page | Proves |
|---|---|
| the live clock | plain DOM + `setInterval`, no special API needed |
| **GET** | `fetch()` works — the page talks to the network directly, no backend |
| **WebSocket** | `new WebSocket(...)` works end to end |
| **Fullscreen** | the page can drive *native* window fullscreen, not just tab fullscreen |

The last one is the interesting bit: a web page normally only controls its own
tab. TMA bridges `DidToggleFullscreenModeForTab()` onto `Widget::SetFullscreen()`
so the window itself goes fullscreen — see [§4](#4-how-it-works).

### 2.5 Where the files go

| Path | What |
|---|---|
| `<out>/tma` | the executable |
| `<out>/content_shell.pak` | Chromium's resource pack, trimmed for TMA |
| `<out>/tma_resources/` | the application directory (`index.html`, …) |

Without `--url`, TMA opens `tma_resources/index.html` **from the directory next
to the executable**, so the app is served over `file://` and needs no web server
of any kind.

---

## 3. Writing your own application

TMA is a shell. Your application is a web page. There is nothing else to learn.

### 3.1 Simplest path: replace the demo page

```bash
cp -r <out>/tma_resources ~/my-tma-app
$EDITOR ~/my-tma-app/index.html
<out>/tma --no-sandbox --url=file://$HOME/my-tma-app/index.html
```

Whatever you put in that directory is what TMA shows. Plain HTML, a SPA, a
static site — it does not matter, because TMA does not know or care.

### 3.2 Talking to a server

```js
// HTTP
const r = await fetch("https://api.example.com/data");
const body = await r.json();

// WebSocket
const ws = new WebSocket("wss://example.com/stream");
ws.onmessage = (e) => console.log(e.data);
```

No server process is started by TMA, no IPC endpoint is opened, and there is no
privileged bridge. **The page's JavaScript is the whole application layer.**

### 3.3 Driving the window from the page

```js
// Fullscreen — TMA maps this onto native window fullscreen.
await document.documentElement.requestFullscreen();
document.exitFullscreen();

// A page cannot move or resize its own window; that is window-manager
// territory. Use the shell's own controls, or the --window-size switch.
```

### 3.4 Things the page can use

Everything a normal Chromium page gets: CSS, ES modules, WebAssembly, `fetch`,
`WebSocket`, `localStorage`, `IndexedDB`, Service Workers over `file://` where
Chromium allows it, **WebGL** (via ANGLE on top of desktop GL) and **WebGPU**
(via Dawn) — both are compiled in and switched on. See
[§8](#8-build-configuration-argsgn).

### 3.5 Things TMA deliberately does not provide

| Not provided | Why |
|---|---|
| An HTTP server | the page fetches remote URLs itself |
| A DevTools HTTP/CDP port | `ShellDevToolsManagerDelegate::StartHttpHandler()` is never called |
| A Node.js runtime | there is no backend process at all |
| A privileged JS API | no `chrome.*`, no IPC, no mojo bindings exposed to the page |

---

## 4. How it works

### 4.1 Why C++ alone is enough

TMA **does not render the web**. It is an *embedder*: it opens a native window,
paints a 28 px strip on top of it with Skia, and hands the rest of the space to
Chromium's `content::WebContents`. The rendering is entirely Chromium's job, and
Chromium is itself C++ — so an application written for TMA needs no JavaScript
toolchain, no bundler, no server, and no second process.

The process tree at run time looks like this:

```
tma  (browser process)
 ├─ aura::WindowTreeHost + views::Widget + TmaFrameView (Skia) + views::WebView
 ├─ renderer process   ← Blink + V8: HTML/CSS/JS → display list
 └─ gpu process        ← viz compositor → Ganesh(GL) → ANGLE → wl_linux_dmabuf
```

The pipeline, end to end:

```
  your JS runs in V8 ──► Blink lays out & paints ──► display list
                                                          │ (crosses process)
                                                          ▼
                        viz compositor (GPU process) ──► Ganesh(GL)
                                                          │
                                                          ▼
                              ANGLE ──► OpenGL ──► wl_linux_dmabuf ──► GNOME
```

TMA's own drawing uses the **same** Ganesh/GL path as the page, so the 28 px
strip and the web content are composited into one frame by Chromium's compositor
— never by the window manager.

### 4.2 The five subclasses

TMA is grafted onto `content/shell` by overriding exactly one hook in each of
five classes:

```
tma::TmaMainDelegate         : content::ShellMainDelegate
  └ CreateContentBrowserClient()
tma::TmaContentBrowserClient : content::ShellContentBrowserClient
  └ CreateBrowserMainParts()
tma::TmaBrowserMainParts     : content::ShellBrowserMainParts
  └ CreateShellPlatformDelegate()
tma::TmaPlatformDelegate     : content::ShellPlatformDelegate
  └ CreatePlatformWindow()   → tma::TmaWidgetDelegate + tma::TmaView
tma::TmaWidgetDelegate       : views::WidgetDelegate
  └ CreateFrameView()        → tma::TmaFrameView
```

* **`TmaFrameView`** paints the strip and answers `NonClientHitTest()`. Hit
  testing runs caption buttons first, then the 8 px resize ring, then falls
  through to `HTCLIENT` (web content) or `HTCAPTION` (drag).
* **`TmaView`** owns the `content::Shell` and hosts its `WebContents` in a
  `views::WebView`.
* **`TmaFullscreenKeyHandler`** is a pre-target key handler plus a
  `aura::WindowObserver` that owns the `F11` path. It exists because
  `views::FocusManagerEventHandler` refuses to act unless something inside the
  window has focus, and the window border is 0 px, so there is no non-client
  area to fall back on.
* **`TmaWidgetDelegate::ShouldDescendIntoChildForEventHandling()`** is the
  cursor hook: `wm::CompoundEventFilter` asks the *renderer* window for the hit
  component and always gets `HTCLIENT`, so TMA returns `false` for the resize
  ring and lets the event reach the window instead. This is what makes the cursor
  switch to a resize arrow.
* **`TmaFullscreenObserver`** is a `content::WebContentsObserver` overriding
  `DidToggleFullscreenModeForTab()`. `content::Shell`'s own
  `ToggleFullscreenModeForTab()` only talks to a platform on Android and iOS, so
  on Linux the renderer is *told* it is fullscreen while the window stays put.
  The observer closes that gap without patching Chromium.

The native window is created with `remove_standard_frame = true` (the compositor
must not draw a decoration of its own) and with `SetHasWindowSizeControls(true)`
(the platform then honours caption drag, border resize and double-click).

### 4.3 What TMA does *not* do

* It does **not** patch Chromium's source. Grafting is done by
  `scripts/setup_chromium.sh`, which appends a marker-guarded block to the root
  `BUILD.gn` and replaces the `sources`/`deps` lists of `//content/shell:pak`.
* It does **not** ship DevTools. The front-end is still compiled (it is reached
  through `content_shell_lib`) but it is no longer written into `content_shell.pak`.
* It does **not** own the page. There is no privileged API between the shell and
  the document.

---

## 5. Build dependencies (Fedora)

TMA adds no libraries of its own, and neither does the Chromium build: it
compiles and links against a Debian sysroot that it downloads itself. The host
therefore needs only a small toolchain. Everything here is written for
**Fedora 44 with dnf5**.

### 5.1 Install

`./build.sh` installs these itself: it probes each one with `rpm -q` and only
calls `sudo dnf install` for the packages that are missing, so the second run
asks for no password. If you would rather do it once by hand:

```bash
sudo dnf install git perl binutils bison flex gperf
```

| Package | Why |
|---|---|
| `git` | `fetch`, `gclient sync`, and pinning the checkout to `CHROMIUM_VERSION` |
| `perl` | several `third_party` build scripts |
| `binutils` | host `ar`, `ld`, `nm`, `objcopy`, `strip` |
| `bison` | parser generators used by `third_party` |
| `flex` | lexer generators |
| `gperf` | perfect-hash generators |

Packages Fedora already guarantees are **not** listed: `rpm`, `curl` (mandatory
in `@core`), `python3` (required by every Wayland session, which TMA requires
anyway), `xz` (required by `dracut`), `pkgconf-pkg-config` (required by `kmod`),
`tar`.

Packages deliberately **not** installed:

| Not installed | Why not |
|---|---|
| `gcc`, `gcc-c++`, `clang`, `lld`, `llvm` | every file is compiled by Chromium's hermetic clang in `third_party/llvm-build/Release+Asserts` |
| `ninja-build` | the build driver comes from the checkout (`third_party/siso`) |
| `make`, `cmake` | upstream does not install either for a normal build |
| `nodejs` | Chromium ships `third_party/node` |
| `which` | the scripts use the bash builtin `command -v` |
| any `*-devel` header package | not read at all, see below |

### 5.2 Why the `-devel` packages are not needed

The usual advice — install every `-devel` package Chromium might touch — does not
apply, because **the build never reads the host's headers**:

| Evidence | |
|---|---|
| `build/config/posix/BUILD.gn` | `sysroot_flags = ["--sysroot=" + …]` is added to `cflags_c`, `cflags_cc`, `asmflags` **and** `ldflags` |
| `build/config/linux/pkg_config.gni` | pkg-config runs as `-s <sysroot> -a <cpu>`, resolving `.pc` files inside the sysroot (313 of them) |
| `build/toolchain/toolchain.gni` | `system_headers_in_deps = !use_sysroot` — with `use_sysroot = true` the host headers are not even in the dependency graph |
| the sysroot itself | 19.7 MB Debian bullseye sysroot carrying `nss`, `gtk-3.0`, `cups`, `pulse`, `wayland`, `xkbcommon`, `dbus-1.0`, `freetype2`, `harfbuzz`, `EGL`, … |

**Runtime libraries are a separate matter.** `--sysroot` covers link time only. At
run time `tma` links against the host's shared libraries — `wayland`,
`mesa-libEGL`, `gbm`, `libX11`, `pulseaudio-libs`, `dbus-libs`, `libxkbcommon` —
all of which a Fedora Wayland session already ships.

> **Alternative:** `sudo dnf builddep chromium` after enabling the source repos
> (`sudo dnf config-manager --set-enabled fedora-source updates-source`). That
> installs a substantially larger set, including every `-devel` package shown
> above to be unnecessary. It is valid, not required. Do **not** run Chromium's
> own `install-build-deps.sh` — it is apt/dpkg only.

---

## 6. Building

### 6.1 The command

```bash
./build.sh
```

There is nothing else to invoke. `build.sh` is a thin dispatcher over three
scripts in `scripts/`, and it runs them in order every time:

| Step | Script | What it does | Cost on a re-run |
|---|---|---|---|
| 1 | `scripts/install_deps.sh` | probe and install the missing Fedora packages | skips `sudo` when nothing is missing |
| 2 | `scripts/setup_chromium.sh` | depot_tools → fetch → **pin to `CHROMIUM_VERSION`** → `gclient sync` → graft `tma/` → hook `//tma` into the root `BUILD.gn` → trim the resource pack → `gn gen` | no-op once on the tag |
| 3 | `scripts/build_tma.sh` | `autoninja -C out/Default tma` | incremental, ~40–50 s |

Each script's own header comment documents its internal flags, for the rare case
you need one. Nothing in this README depends on them.

Environment overrides (all optional): `CHROMIUM_SRC` (default
`$HOME/chromium/src`), `DEPOT_TOOLS` (default `$HOME/depot_tools`), `TMA_OUT`
(default `<src>/out/Default`), `CHROMIUM_TAG`.

### 6.2 Why you must re-run `./build.sh` after editing `tma/`

Step 2 ***copies* `tma/` into the checkout** (`$CHROMIUM_SRC/tma`) — GN has to
see the files where the build graph expects them, so they cannot stay in this
repository. Editing anything under `./tma/` therefore does nothing until the
graft is refreshed, and `./build.sh` refreshes it every time.

This is the single most common mistake when iterating on TMA's own C++: edit,
then re-run `./build.sh` — not a bare `autoninja`.

### 6.3 The version pin

TMA never builds Chromium `main`. The tag lives in one file, `CHROMIUM_VERSION`,
and `setup_chromium.sh` re-pins to it on every run, so a fresh clone and an
existing checkout always end up on the same revision.

| TMA | Chromium tag | Shipped by |
|---|---|---|
| current | `155.0.8059.39` | Chrome Stable (Linux) |

To move or roll back, edit `CHROMIUM_VERSION` and re-run `./build.sh`. Check a
tag exists before committing to it:

```bash
curl -o /dev/null -w '%{http_code}\n' \
  "https://chromium.googlesource.com/chromium/src/+refs/tags/155.0.8059.39?format=JSON"
```

### 6.4 The `//tma` hook

GN only loads a `BUILD.gn` when some already-loaded file references it, so an
otherwise unreferenced `//tma` is invisible (`gn desc` reports
`matches no targets`). `setup_chromium.sh` appends a marker-guarded block to the
root `BUILD.gn`:

```gn
# --- TMA hook (added by scripts/setup_chromium.sh) ---
group("tma_hook") {
  testonly = true
  deps = [ "//tma:tma" ]
}
```

It is deliberately not named `tma`: a root group called `tma` would emit a phony
output colliding with the `./tma` the executable already produces, and every
ninja invocation would fail with `multiple rules generate tma`.

The block is also why TMA **must** be built inside a Chromium checkout —
`//content/shell:content_shell_lib` is a `testonly` target, so `//tma` inherits
`testonly = true`.

### 6.5 The trimmed resource pack

The embedder loads resources from a fixed filename (`content_shell.pak`), and GN
rejects two producers of the same output file. So instead of adding a repack,
`setup_chromium.sh` replaces the `sources`/`deps` lists of the existing
`repack("pak")` in `//content/shell/BUILD.gn` with TMA's own lists from
`tma/tma_pak.gni`. What is dropped from the pack:

| Removed | What it is |
|---|---|
| `devtools_resources.pak` | the DevTools front-end |
| `inspector_overlay_resources.pak` | DevTools "inspect element" overlay |
| `media_internals`, `webrtc_internals`, `quota_internals` | `chrome://` internals pages |
| `histograms`, `ukm`, `traces_internals`, `tracing_*` | `chrome://` internals pages |
| `webxr_internals` | VR internals page |

Result: `content_shell.pak` is **1.76 MB**.

---

## 7. Packaging

`./build.sh` leaves the binary unstripped (273 MB) because debug symbols make
compiling faster. To stage something you can actually ship, run the packaging
script:

```bash
scripts/package_tma.sh            # strip + copy → <out>/package/
scripts/package_tma.sh --xz       # the same, but tma.xz instead of tma
```

`strip` is **not** optional on Linux: Chromium never strips its own output
(`enable_stripping` is only referenced from `build/config/apple/`), so the debug
symbols stay in the binary unless packaging removes them. It is by far the
largest single saving.

| | size |
|---|---:|
| `tma`, unstripped | 273.1 MB |
| `tma`, stripped | **145.6 MB** |
| `tma.xz` (`xz -9`) | **38.9 MB** |
| `content_shell.pak` | 1.76 MB |

The staged directory contains `tma`, `content_shell.pak`, `tma_resources/` and,
if present, `chrome-sandbox` (copied with mode `4755`).

### 7.1 The sandbox

Chromium's setuid helper `chrome-sandbox` must be owned by root with mode
`4755`, otherwise the process refuses to start. A checkout build never ships it
that way, so either:

```bash
sudo chown root:root <out>/chrome-sandbox
sudo chmod 4755   <out>/chrome-sandbox
```

or pass `--no-sandbox`, which is what the examples in this document already do.

### 7.2 Desktop entry

`packaging/tma.desktop` and `packaging/tma.svg` are provided:

```bash
install -Dm644 packaging/tma.desktop ~/.local/share/applications/tma.desktop
install -Dm644 packaging/tma.svg     ~/.local/share/icons/hicolor/scalable/apps/tma.svg
```

`Exec=tma %U`, so put the directory containing the `tma` binary on your `PATH`
(or edit the `Exec=` line to an absolute path).

---

## 8. Build configuration (`args.gn`)

`setup_chromium.sh` writes `out/Default/args.gn` on every `gn gen`. The
configuration is Wayland-only and off-by-default-heavy, in two halves.

### 8.1 Platform and optimisation

```gn
is_debug = false
symbol_level = 0
blink_symbol_level = 0

ozone_auto_platforms = false
ozone_platform = "wayland"
ozone_platform_wayland = true

is_official_build = true
chrome_pgo_phase = 0
is_cfi = false
optimize_for_size = true
dcheck_always_on = false
```

The last four are what actually bought the size:

| | baseline | TMA |
|---|---:|---:|
| all sections | 327.5 MB | 146.5 MB |
| binary, stripped | 326.2 MB | **145.6 MB** |

**The honest answer to "can you make Chromium small" is: not by turning features
off — by changing how it is compiled.** `optimize_for_size` switches the whole
tree to `-Os` plus `-fdata-sections -ffunction-sections` (which ICF then folds);
`is_official_build` adds `-fvisibility=hidden` and drops the `DCHECK` bodies.

### 8.2 What is switched off

About 120 arguments. The categories:

| Category | Examples |
|---|---|
| media codecs TMA never decodes | `media_use_ffmpeg`, `media_use_libvpx`, `media_use_openh264`, `use_vaapi`, `enable_av1_decoder`, `enable_jxl_decoder` |
| platform features TMA does not have | `enable_vr`, `enable_openxr`, `enable_remoting`, `enable_cast_receiver`, `enable_library_cdms`, `enable_widevine` |
| network features unused | `enable_reporting`, `enable_hls_demuxer`, `disable_brotli_filter` |
| desktop chrome | `use_cups`, `enable_linux_installer`, `enable_offline_pages`, `enable_rlz` |
| host tooling / test leftovers | `enable_gwp_asan`, `enable_perfetto_unittests`, `enable_pseudolocales` |
| AI / ML | `use_on_device_model_service`, `webnn_use_litert`, `enable_compute_pressure` |
| devtools bundling | `devtools_bundle = false`, `optimize_webui = false` |
| ANGLE backends unused | `angle_enable_vulkan = false`, `angle_enable_wgpu = false` (renders with desktop **GL** on Wayland) |

### 8.3 Things that look safe and are not

Every row below was settled by an actual `gn gen` or an actual build, never by
reading the code. `setup_chromium.sh` carries the same notes as comments next to
the argument list, because the failure mode is always the same: **Chromium
guards the `#include` with a buildflag and leaves the type in the function
signature unguarded**, so `gn gen` says yes and the compiler says no half an hour
later.

| Candidate | Why it stays on |
|---|---|
| `enable_vulkan` | `gpu/command_buffer/service` compiles `dawn_ozone_image_representation.cc` whenever `use_dawn && use_ozone`, but the `//third_party/libsync` dep supplying `<sync/sync.h>` sits inside `if (enable_vulkan)` → *file not found* |
| `rtc_build_libsrtp` | links with 17 undefined `srtp_*` symbols; webrtc calls them unguarded |
| `is_p2p_enabled` | blink's `mediastream` loses `-Ithird_party/webrtc` entirely |
| `rtc_use_pipewire` | drops `pipewire_config` out of `webrtc_component`'s `public_configs`, which is what carries the webrtc include paths into blink |
| `tint_build_glsl_writer` | Dawn's GL backend needs `tint::glsl` / `tint::null` (the `msl` and `hlsl` writers *are* safe to disable on Linux, and are) |
| `enable_websockets` | `devtools_http_handler.cc` calls `net::HttpServer` with no `ENABLE_WEBSOCKETS` guard → 26 undefined symbols |
| `enable_gpu_channel_media_capture` | `gpu_channel_host.h` include is guarded but the `scoped_refptr<gpu::GpuChannelHost>` parameter on the next line is not → compile error |
| `safe_browsing_mode = 0` | root `//BUILD.gn` loads `//chrome/test` targets whose dependencies vanish → `Unresolved dependencies` at generation |
| `enable_dawn` | **not a build argument at all**; GN warns *never appeared in a declare_args() block* |
| `enable_devtools_frontend` | not a build argument — `content/browser/devtools/features.gni` assigns it as a plain global, so GN ignores it in `--args` |

---

## 9. What cannot be removed: WebRTC and Perfetto

Two libraries account for ~7 MB of the binary and **there is no build argument to
drop either**. They are not pulled in by a feature TMA happens to enable — they
are baked into the targets every embedder must link.

| Library | Targets in `//tma`'s closure | Size in binary | Reached through |
|---|---:|---:|---|
| `//third_party/webrtc` | 724 | ~4.3 MB | `content/public/renderer` depends on `//third_party/webrtc_overrides:webrtc_component` with **no condition**, and `public_deps` edges propagate to *every* transitive dependent |
| `//third_party/perfetto` | 783 | ~2.7 MB | `base/BUILD.gn` declares `public_deps += [ "//third_party/perfetto:libperfetto" ]`; `//base` is the root of the graph, so everything links it |
| `//third_party/dawn` + tint + spvtools | 186 | ~5.8 MB | WebGPU — **kept deliberately** (see below) |

### 9.1 What WebRTC is for

Peer-to-peer audio/video/data on the web: `RTCPeerConnection`, `getUserMedia`,
`RTCDataChannel`. The `third_party/webrtc` tree is the reference implementation of
ICE, DTLS-SRTP, RTP/RTCP and congestion control. Video calling, screen sharing
and camera/microphone capture all use it. It cannot be turned off, because it is
a web platform standard, not a product feature.

### 9.2 What Perfetto is for

Chromium's tracing and profiling system — the engine behind `chrome://tracing`,
memory-infra dumps and `--trace-startup`. `libperfetto` is the backend that every
`TRACE_EVENT0(...)` in Chromium writes into. It is genuinely dead weight for a
shipped display shell, but it is a `public_deps` of `//base`, and `//base` has no
opt-out.

### 9.3 Why they cannot be removed

1. **They arrive through `public_deps`.** Those edges are visible to GN, and
   neither `remove_public_deps` nor swapping `static_library` for `source_set`
   changes which objects the linker pulls in.
2. **The compile is unconditional.** `content/renderer/*.cc` `#include` webrtc
   headers directly, so removing the dependency from `BUILD.gn` fails at
   compilation, not at link time.
3. **There is no feature flag.** Chromium exposes no `enable_webrtc` and no
   `enable_perfetto` for the client library. The `enable_perfetto_*` flags that
   do exist only gate `tools/`, `traced` and `trace_processor`.
4. **Deleting the sources would break the build.** `//base_unittests` is still
   generated and still compiles against them.

This is also why CEF and Electron ship binaries of comparable weight: they have
the same problem and the same unsolved answer. The only options are (a) accept
them, or (b) patch `//base` and `content/public/*` — which is exactly the
source-editing this project's own rules forbid. TMA accepts them. They are
excluded from `content_shell.pak`, so they cost binary size and link time but
**no shipped resource**, and they are dead weight rather than running code: no
WebRTC call is made and no Perfetto trace is recorded unless something explicitly
asks.

### 9.4 Why Dawn / WebGPU stays on

WebGPU is compiled in (`use_dawn` is left at its default `true`) so that pages
can use it. That decision costs ~5.8 MB across 186 targets and it is what forces
`enable_vulkan`, `tint_build_glsl_writer`, `rtc_build_libsrtp` and friends to
stay `true` — see the trap table in [§8.3](#83-things-that-look-safe-and-are-not).
WebGL runs on ANGLE over desktop GL (`angle_enable_vulkan = false`).

---

## 10. Troubleshooting

| Symptom | Cause and action |
|---|---|
| `error: dnf not found; this script is Fedora-only` | [§5](#5-build-dependencies-fedora) targets Fedora; use the package list directly on another distribution |
| `WAYLAND_DISPLAY is not set` | start TMA from a Wayland session — there is no X11 and no headless fallback compiled in |
| the process exits immediately, sandbox related | see [§7.1](#71-the-sandbox) |
| `tma` starts but shows nothing / 0 processes | the `WAYLAND_DISPLAY` variable was lost by your shell. Re-run with `env WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 <out>/tma --no-sandbox` |
| `autoninja: command not found` | `PATH` is missing `depot_tools`: `export PATH="$HOME/depot_tools:$PATH"` |
| edited `./tma/**` and nothing changed | the graft **copies** the tree; re-run `./build.sh` — see [§6.2](#62-why-you-must-re-run-buildsh-after-editing-tma) |
| `matches no targets` / `unknown target 'tma'` | the root `BUILD.gn` hook is missing; re-run `./build.sh` |
| `clang_revision=… but update.py expected …` | the checkout is out of sync; re-run `./build.sh` |
| a missing header or `.pc` file at compile time | the sysroot is missing or `use_sysroot` was turned off; see [§5.2](#52-why-the--devel-packages-are-not-needed) |
| a step fails with `command not found: <tool>` | install it (`sudo dnf install <tool>`) and re-run `./build.sh` |
| `multiple rules generate tma` | a root group named `tma` was added by hand; it must be named `tma_hook` |

---

## 11. Status and limitations

* **Linux and Wayland only.** No X11 and no headless code path is compiled in.
* **`testonly = true`**, because `//content/shell:content_shell_lib` is a
  `testonly` target. TMA must be built **inside** a Chromium checkout.
* The top strip carries **no title text**; the task switcher still shows the page
  title, as set by `<title>`.
* The tree is written against Chromium's `main` branch but builds are pinned to
  the tag in `CHROMIUM_VERSION` (`155.0.8059.39`). This tree **has** been built
  and run against that tag; if the pin is ever moved, expect to adjust small API
  differences.
* `F11` and the resize grips are verified by static analysis and by a compiled
  hit-test dump, **not** by end-to-end input — the test machine has no
  input-injection tool. Try them and report back.

### Files

```
TMA.md                     plan and requirements
CHROMIUM_VERSION           the Chromium release tag TMA builds against
build.sh                   single entry point for the whole workflow
Makefile                   thin wrapper over build.sh
tma/BUILD.gn               GN target; lives at the root of a Chromium checkout
tma/tma_pak.gni            the trimmed resource-pack lists
tma/app/                   process entry point and ContentMainDelegate
tma/browser/               browser client, main parts, platform delegate, frame
tma/resources/index.html   default application page
scripts/                   deps, setup, build, run and package helpers
packaging/                 .desktop entry and icon
```
