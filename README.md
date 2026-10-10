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

`./tma.conf` decides the application's identity and what it opens with:

```ini
app_name = tma
app_id = tma
startup = file
```

`app_name` and `app_id` must start with a letter or digit and use only
letters, digits, `.` `_` and `-`.

`startup` is required and says what the window opens with when the command line
says nothing. There are exactly two values:

```ini
startup = file                      the packaged page:
                                     <app_name>_resources/index.html
startup = url:https://example.com   a URL
```

`file` takes **no name** on purpose: the entry point is fixed at `index.html`
and the config only chooses between that page and a URL. Everything else in
`tma/resources/` is copied beside it, so drop assets next to `index.html`, run
`./build.sh`, and they appear in `<app_name>_resources/`. Subdirectories are not
preserved — keep assets at the top level. An empty `startup` is an error, and
`./build.sh` reports it in under a second.

With `startup = url:...` nothing would ever read that directory, so it is left
out of the build entirely: no `<app_name>_resources/` is produced, and an
earlier one is deleted on the next build.

There is deliberately no window title. The top bar carries the three caption
buttons and no text, so a page's `<title>` has nowhere to appear and is
ignored.

The file is read at *setup* time and baked into the binary, so re-run
`./build.sh` after editing it. A command-line argument always beats the
configured value:

```bash
build/<app_name> --url=https://example.com
build/<app_name> https://example.com
build/<app_name> /path/to/page.html
```

## Where the output goes

| Path | What |
|---|---|
| `build/<app_name>` | the binary, stripped |
| `build/content_shell.pak` | resources |
| `build/<app_name>_resources/` | the app page and its assets |

`$HOME/chromium/src/out/Default` keeps the unstripped build (273 MB). `build/`
is the shippable copy (146 MB) — copy the whole directory.

## Run

```bash
build/<app_name>
```

Needs a Wayland session. If your shell has no `WAYLAND_DISPLAY`, prefix
`env WAYLAND_DISPLAY=wayland-0`.

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
URL. The packaged page ships no external resources and only calls `fetch()` when
you submit a URL in its box.

Two things do reach the network, and both are yours:

```ini
# opens that URL once, at startup
startup = url:https://example.com
```

```bash
build/<app_name> --url=https://example.com   # or a bare URL/path argument
```

`git push` to `github.com/TimeAndTimeStudio/TMA` is run by hand; nothing in
`./build.sh` talks to GitHub.

## Files

```
CHROMIUM_VERSION    the Chromium tag TMA builds against
build.sh            the whole workflow
tma.conf            application name, id and startup target
tma/browser/        the frame: hit-test, caption buttons, strip
tma/app/            process entry point
tma/resources/      the application page and its assets
scripts/            deps, setup, build, run, package
```

## License

GPL-3.0-or-later. See `LICENSE`.
