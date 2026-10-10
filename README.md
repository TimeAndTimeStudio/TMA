# TMA — Time Mini App

A **Linux display shell written in C++** that embeds **official Chromium**
directly. Chromium renders the page; TMA adds only a 28 px dark strip along the
top with minimise / maximise / close buttons, no border, and an invisible
resize ring.

No CEF, no Electron, no Chromium fork, no backend — the page's own JavaScript
does every HTTP request and WebSocket itself. Linux and Wayland only.

## Build

```bash
./build.sh
```

Installs the missing Fedora packages, fetches the pinned Chromium source,
grafts `tma/` into the checkout, compiles, and packages.

| Command | What it does |
|---|---|
| `./build.sh` | everything above |
| `./build.sh build` | incremental build only (~40–50 s) |
| `./build.sh package` | re-stage `build/`; add `--xz` to compress |
| `./build.sh run` | build, then launch |
| `./build.sh clean` | delete `out/Default` and `build/` |
| `./build.sh distclean` | also delete the Chromium checkout and depot_tools |

The first run is heavy. After that every step is idempotent.

**After editing `./tma/` or `./tma.conf`, run `./build.sh`.** The tree is
*copied* into the Chromium checkout, so a bare `autoninja` sees stale files.

## Configure

`./tma.conf` decides the application's identity, what the window opens with,
and the floor it cannot shrink past:

```ini
app_name = tma
version = 0.1.0
app_id = tma
startup = url:https://example.com
min_width = 320
min_height = 200
```

`app_name` must be lowercase — it is the binary's name on disk, and a name that
changes with Shift held is one people mistype. `app_id` and `version` allow
upper case too; all three start with a letter or digit and then use only
letters, digits, `.` `_` `+` and `-`. `min_width` / `min_height` must be positive
integers: they are the client (web content) area, and the strip along the top
adds to the height on top of `min_height`.

`startup` is required and has exactly one form, `url:<url>` — anything with a
scheme (`http://`, `https://`, `data:`, ...). There is no packaged page: this
URL is the only thing the window will ever open. An empty value, a `file`
keyword left over from an older config, or a URL with no scheme are each
rejected by `./build.sh` in under a second.

**TMA takes no arguments except `--version` and `--version-browser`** (see
[Run](#run)), and refuses to start if you pass anything else. No `--url`, no
positional argument, no `--window-size`, no `--fullscreen`, and no file path
either — `main()` exits with an error naming what you passed, before Chromium
is initialised, rather than quietly ignoring it. What a binary does follows
from `tma.conf` alone and nothing about how it was invoked; what the window
opens and how small it may get are decided before the link.

There is deliberately no window title. The top bar carries the three caption
buttons and no text, so a page's `<title>` has nowhere to appear and is
ignored.

The file is read at *setup* time and baked into the binary, so re-run
`./build.sh` after editing it.

## Where the output goes

| Path | What |
|---|---|
| `build/<app_name>` | the binary, stripped |
| `build/content_shell.pak` | resources |

`$HOME/chromium/src/out/Default` keeps the unstripped build (273 MB). `build/`
is the shippable copy (146 MB) — copy the whole directory.

## Run

```bash
build/<app_name>
```

Needs a Wayland session. If your shell has no `WAYLAND_DISPLAY`, prefix
`env WAYLAND_DISPLAY=wayland-0`.

Two flags are accepted, and between them they name the whole build:

```bash
build/<app_name> --version           # tma 0.1.0
build/<app_name> --version-browser   # Chromium 155.0.8059.39
```

`--version` is the shell's own identity — the `app_id` and `version` from
`tma.conf`, nothing more. `--version-browser` is the Chromium underneath, bare,
for scripts that mean to compare it.

## Network

Every host TMA or its build reaches. No telemetry, no update check, no backend.

### `./build.sh deps`

Whatever `/etc/yum.repos.d/` is configured for — on stock Fedora that starts at
`https://mirrors.fedoraproject.org/metalink?...` and then the mirror it resolves
to. The script carries no URL of its own; it only runs `sudo dnf install`.

### `./build.sh setup`

| Host | What |
|---|---|
| `chromium.googlesource.com/chromium/tools/depot_tools.git` | cloned once |
| `chromium.googlesource.com/chromium/src.git` | the tag in `CHROMIUM_VERSION` |
| `*.googlesource.com` — webrtc, skia, dawn, pdfium, quiche, boringssl, swiftshader, aomedia, android | the git deps listed in `src/DEPS` |
| `chrome-infra-packages.appspot.com` | CIPD packages |
| `storage.googleapis.com/<bucket>/...` | GCS blobs: the clang toolchain (`chromium-browser-clang`, 427 MB), the Debian sysroot (`chrome-linux-sysroot`), node, clang-format, ... |

`--no-history` makes every git fetch shallow; `--jobs=4` caps parallel fetches
because googlesource answers HTTP 429 above a shared quota. `cat
$HOME/chromium/.gclient_entries` lists exactly what landed on your machine.

**Remote Build Execution is not used.** autoninja runs `siso ninja --offline`
because `use_remoteexec` is unset — that is what the `offline mode` line in the
build output means. Every step is compiled here; no RBE or remote cache is
contacted.

### `./build.sh build`, `package`, `run`

Nothing. The build is local, packaging is `cp` / `strip` / `xz`.

### Running TMA

Nothing either. The only IPC is the Wayland socket and, over D-Bus,
`org.freedesktop.appearance` (prefers-color-scheme) and
`org.freedesktop.portal.FileChooser` (the file dialog) — both local, neither a
URL.

One thing reaches the network, and it is yours — the URL this opens once, at
startup:

```ini
startup = url:https://example.com
```

The page it opens makes whatever requests it likes, which is its business and
not TMA's; TMA itself issues none.

`git push` to `github.com/TimeAndTimeStudio/TMA` is run by hand; nothing in
`./build.sh` talks to GitHub.

## Files

```
CHROMIUM_VERSION    the Chromium tag TMA builds against; --version reports it
                    together with `version` from tma.conf
build.sh            the whole workflow
tma.conf            application name, id, startup URL and window floor
tma/browser/        the frame: hit-test, caption buttons, strip
tma/app/            process entry point
scripts/            deps, setup, build, run, package
```

## License

GPL-3.0-or-later. See `LICENSE`.
