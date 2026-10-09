// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_DARK_MODE_H_
#define TMA_BROWSER_TMA_DARK_MODE_H_

namespace tma {

// Reads org.freedesktop.appearance / color-scheme from the session bus and
// pushes it into the web NativeTheme as PreferredColorSchemeOverride, which is
// what Blink turns into `prefers-color-scheme`.
//
// This exists because two upstream paths are both unavailable here:
//   * ui::DarkModeManagerLinux delivers the value to LinuxUiTheme, but the only
//     LinuxUiTheme a non-GTK build has is FallbackLinuxUi, whose
//     SetColorScheme() is an empty function (ui/linux/fallback_linux_ui.cc).
//   * ui::NativeTheme::OsSettingsProvider returns kNoPreference without a
//     toolkit to ask.
// The override is the same hook --force-dark-mode uses, so it is a supported
// entry point rather than a fork of Blink's preference plumbing.
//
// Call once from the browser main loop before the first window is created.
// Safe to call more than once; later calls are ignored.
void InitDarkModeFromPortal();

}  // namespace tma

#endif  // TMA_BROWSER_TMA_DARK_MODE_H_
