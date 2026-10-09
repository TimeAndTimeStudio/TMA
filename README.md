# TMA — Time Mini App

A **Linux display shell written in C++** that embeds **official Chromium**
directly. Chromium renders the page; TMA adds only a frame around it.

* 28 px dark strip (`#202124`) along the top, **no text on it**
* minimise / maximise / close drawn as `-` `[]` `X`, centred in the strip
* no border — web content runs edge to edge
* invisible 8 px resize ring (16 px corners)
* drag to move, drag to resize, double-click to maximise, `F11` fullscreen

No CEF, no Electron, no Chromium fork. TMA is a small `source_set` compiled
inside a Chromium checkout on top of `content/shell`, with **no backend** —
the page's own JavaScript does every HTTP request and WebSocket itself.

Linux and Wayland only. No X11, no headless path.

## Build

```bash
./build.sh
```

One command, no arguments: installs missing Fedora packages, fetches the pinned
Chromium source and toolchains, grafts `tma/` into the checkout, generates the
build graph, compiles, and stages a stripped copy into `./build/`. Re-runs are
incremental (~40–50 s).

**After editing `./tma/`, re-run `./build.sh`.** Step 2 *copies* the tree into
the checkout, so a bare `autoninja` sees stale files.

Host packages — the script probes each one and only installs what is missing:

```bash
sudo dnf install git perl binutils bison flex gperf
```

Nothing else is needed. The build compiles against a Debian sysroot it
downloads, so no `-devel` package is read, and Chromium's hermetic clang is the
compiler — no host `gcc`, `clang`, `ninja-build` or `nodejs`.

| | |
|---|---|
| Chromium tag | `155.0.8059.39` — the only pin, in `CHROMIUM_VERSION` |
| checkout | `$HOME/chromium/src` (`CHROMIUM_SRC`) |
| output | `<src>/out/Default` (`TMA_OUT`) |

## Run

```bash
<out>/tma --url=https://example.com --window-size=800x600
<out>/tma --url=file:///path/to/myapp/index.html
```

`<out>` is the path `./build.sh` prints. A terminal inside a Wayland session
already has `WAYLAND_DISPLAY`; if yours does not, prefix
`env WAYLAND_DISPLAY=wayland-0`.

| Switch | Meaning |
|---|---|
| `--url=<url-or-path>` | the application to show |
| `--window-size=<w>x<h>` | default `1280x800`, minimum `320x200` |
| `--fullscreen` | start in native fullscreen |

All of Chromium's own switches work too (`--enable-logging`, `--v=1`,
`--disable-gpu`, …).

| Input | Action |
|---|---|
| drag the strip below its top 8 px | move |
| double-click the strip | maximise / restore |
| drag an edge or corner | resize |
| `F11` | native fullscreen; the strip disappears |
| `-` `[]` `X` | minimise, maximise, close |

`requestFullscreen()` from the page is bridged onto the same native fullscreen,
so a page can drive the real window rather than only its tab.

## Your application

`tma/resources/index.html` is the default page and a working example: a live
clock on plain DOM, `fetch()` and `new WebSocket(...)` both talking to the
network directly, and a fullscreen button. To ship your own, replace that file
and re-run `./build.sh`.

There is no TMA-specific JS API. Whatever the page can do in a browser, it can
do here — that is the whole design.

## Build configuration

`setup_chromium.sh` writes `out/Default/args.gn` on every `gn gen`: Wayland only
(`ozone_platform = "wayland"`), release, and about 120 arguments switched off —
media codecs, VR, remoting, cast, Widevine, CUPS, the installer, DevTools
bundling, unused ANGLE backends.

Size comes from *how* it is compiled, not from features: `optimize_for_size`
(`-Os` + section GC + ICF) and `is_official_build` (`-fvisibility=hidden`, no
`DCHECK` bodies) take a stripped binary from 326.2 MB down to **145.6 MB**.

Two arguments look removable and are not:

| Argument | Why it stays |
|---|---|
| `enable_vulkan` | Dawn's ozone image rep compiles whenever `use_dawn && use_ozone`, but `//third_party/libsync` sits inside `if (enable_vulkan)` → file not found |
| `tint_build_glsl_writer` | Dawn's GL backend needs `tint::glsl`; WebGL renders through ANGLE over desktop GL |

## What stays in the binary

Three Chromium trees ship inside `tma` and **none has a build argument to drop
it**:

| | Size | Why |
|---|---:|---|
| `//third_party/webrtc` | ~4.3 MB | `content/public/renderer` depends on it unconditionally, and `content/renderer/*.cc` include it directly |
| `//third_party/perfetto` | ~2.7 MB | a `public_deps` of `//base`, and `//base` is the root of the graph |
| `//third_party/dawn` + tint + spvtools | ~5.8 MB | WebGPU — kept on purpose, `use_dawn` stays `true` |

No `enable_webrtc` and no `enable_perfetto` exists for the client library. They
are dead weight rather than running code: no WebRTC call is made and no trace is
recorded unless something asks. Dropping them means patching `//base` and
`content/public/*`, which this project does not do.

## Packaging

`./build.sh` stages into this repository's `build/` (gitignored): the binary,
`content_shell.pak`, `tma_resources/`, and the runtime files Chromium resolves
relative to `/proc/self/exe` — `icudtl.dat`, `snapshot_blob.bin`,
`v8_context_snapshot.bin`, `libEGL.so`, `libGLESv2.so`, `libvk_swiftshader.so`,
`libvulkan.so.1`, `chrome_crashpad_handler`, `locales/`. Copy the whole
directory.

| | size |
|---|---:|
| `tma`, unstripped | 273.1 MB |
| `tma`, stripped | **145.6 MB** |
| `tma.xz` (`xz -9`) | **38.9 MB** |
| `content_shell.pak` | 1.76 MB |

Those runtime files are not optional. Without `icudtl.dat` the process dies with
`Invalid file descriptor to ICU data received`; without `snapshot_blob.bin`, with
`Error loading V8 startup snapshot file`.

```bash
scripts/package_tma.sh --xz           # also write build/tma.xz
scripts/package_tma.sh /tmp/tma-dist  # stage somewhere else
```

## Sandbox

`--no-sandbox` is needed far less often than usual instructions claim. From
`sandbox/linux/suid/client/setuid_sandbox_host.cc:170-176`, Chromium aborts only
when the setuid helper **exists and is misconfigured**:

| `chrome-sandbox` | Chromium |
|---|---|
| absent | user-namespace sandbox; Fedora allows this by default, no flag needed |
| root-owned, mode `4755` | setuid sandbox |
| present but not root/`4755`/world-exec | `LOG(FATAL)` — **only this case needs `--no-sandbox`** |

A checkout build leaves it mode `644`, which is why the flag is so often
recommended. The helper being *wrong* is not the same as the helper being
*needed*: TMA runs sandboxed by default here.

## Troubleshooting

| Symptom | Action |
|---|---|
| `WAYLAND_DISPLAY is not set` | start from a Wayland session; no X11 or headless fallback is compiled in |
| window shows nothing, 0 processes | re-run with `env WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 <out>/tma` |
| `Invalid file descriptor to ICU data` | `icudtl.dat` is not next to the binary; re-run `scripts/package_tma.sh` |
| `Error loading V8 startup snapshot file` | `snapshot_blob.bin` is not next to it; same fix |
| edited `./tma/**`, nothing changed | the graft copies the tree; re-run `./build.sh` |
| `matches no targets` / `unknown target 'tma'` | the root `BUILD.gn` hook is missing; re-run `./build.sh` |
| `multiple rules generate tma` | the root group must be named `tma_hook`, not `tma` |
| `autoninja: command not found` | `export PATH="$HOME/depot_tools:$PATH"` |

## Files

```
CHROMIUM_VERSION    the Chromium tag TMA builds against
build.sh            the whole workflow, one command
Makefile            wrapper over build.sh
tma/BUILD.gn        GN target, lives at the root of a Chromium checkout
tma/tma_pak.gni     the trimmed resource-pack lists
tma/app/            process entry point, ContentMainDelegate
tma/browser/        browser client, main parts, platform delegate, frame
tma/resources/      the default application page
scripts/            deps, setup, build, run, package
build/              gitignored: the staged, stripped copy to ship
```

The window's Wayland `app_id` is `tma` (`tma/browser/tma_platform_delegate.cc`),
so a `.desktop` entry you write yourself should be named `tma.desktop`.

## License

GPL-3.0-or-later. See `LICENSE`.
