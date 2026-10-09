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
startup =
```

`app_name` and `app_id` must start with a letter or digit and use only
letters, digits, `.` `_` and `-`.

`startup` is what the window opens with when the command line says nothing, and
its prefix says which kind of thing it is:

```ini
startup = url:https://example.com
startup = file:/path/to/page.html
```

Leave it empty for `<app_name>_resources/index.html` beside the binary. A
relative `file:` path is resolved against the binary's directory, not the
working directory.

There is deliberately no window title. The top bar carries the three caption
buttons and no text, so a page's `<title>` has nowhere to appear and is
ignored.

The file is read at *setup* time and baked into the binary, so re-run
`./build.sh` after editing it. A command-line argument always beats the
configured default:

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
| `build/<app_name>_resources/` | the default app page |

`$HOME/chromium/src/out/Default` keeps the unstripped build (273 MB). `build/`
is the shippable copy (146 MB) — copy the whole directory.

## Run

```bash
build/<app_name>
```

Needs a Wayland session. If your shell has no `WAYLAND_DISPLAY`, prefix
`env WAYLAND_DISPLAY=wayland-0`.

## Files

```
CHROMIUM_VERSION    the Chromium tag TMA builds against
build.sh            the whole workflow
tma.conf            application name, id and startup target
tma/browser/        the frame: hit-test, caption buttons, strip
tma/app/            process entry point
tma/resources/      the default application page
scripts/            deps, setup, build, run, package
```

## License

GPL-3.0-or-later. See `LICENSE`.
