// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_dark_mode.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/variant.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/native_theme/native_theme.h"

namespace tma {

namespace {

// Same objects and interface ui::DarkModeManagerLinux talks to; see
// ui/linux/dark_mode_manager_linux.h for the upstream constants.
constexpr char kPortalService[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kSettingsInterface[] = "org.freedesktop.portal.Settings";
constexpr char kReadMethod[] = "Read";
constexpr char kChangedSignal[] = "SettingChanged";
constexpr char kNamespace[] = "org.freedesktop.appearance";
constexpr char kColorSchemeKey[] = "color-scheme";

// Values defined by the org.freedesktop.portal.Settings spec. Anything other
// than kDark/kLight (including kNoPreference = 0) clears the override.
constexpr uint32_t kDark = 1;
constexpr uint32_t kLight = 2;

void Apply(uint32_t color_scheme) {
  std::optional<ui::NativeTheme::PreferredColorScheme> scheme;
  if (color_scheme == kDark) {
    scheme = ui::NativeTheme::PreferredColorScheme::kDark;
  } else if (color_scheme == kLight) {
    scheme = ui::NativeTheme::PreferredColorScheme::kLight;
  }
  // No preference clears the override rather than forcing light, so a desktop
  // that stops answering keeps whatever Chromium would have chosen.
  ui::NativeTheme::SetPreferredColorSchemeOverride(scheme);
  // The override only reaches Blink through the native instance: it recomputes
  // preferred_color_scheme() on the first GetInstanceForNativeUi() and copies
  // the result to the web instance from UpdateWebInstance(). Touching both is
  // what makes the value land before the first frame rather than one
  // toolkit-settings change later.
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
}

// org.freedesktop.portal.Settings.Read returns (v), and the value inside that
// variant is itself a variant — the same two-level unwrap
// DarkModeManagerLinux::OnReadColorScheme does.
bool UnwrapColorScheme(dbus::Response* response, uint32_t* out) {
  if (!response) {
    return false;
  }
  dbus::MessageReader response_reader(response);
  dbus::MessageReader outer(nullptr);
  dbus::MessageReader inner(nullptr);
  return response_reader.PopVariant(&outer) && outer.PopVariant(&inner) &&
         inner.PopUint32(out);
}

void OnReadDone(base::OnceClosure quit,
                uint32_t* color_scheme_out,
                bool* parsed,
                dbus::Response* response) {
  *parsed = UnwrapColorScheme(response, color_scheme_out);
  std::move(quit).Run();
}

void OnSettingChanged(dbus_utils::ConnectToSignalResultSig<"ssv"> result) {
  if (!result.has_value()) {
    return;
  }
  auto& [ns, key, value] = result.value();
  if (ns != kNamespace || key != kColorSchemeKey) {
    return;
  }
  auto scheme = std::move(value).Take<uint32_t>();
  if (scheme.has_value()) {
    Apply(*scheme);
  }
}

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool connected) {}

// Blocking on purpose. ShellContentBrowserClient::OverrideWebPreferences reads
// the scheme while the first window is being created, a few milliseconds after
// PreMainMessageLoopRun returns; an async reply would land after that and the
// first frame would still be told "light". ObjectProxy::CallMethodAndBlock()
// refuses to run off the D-Bus thread, so spin a nested RunLoop instead: the
// reply is posted back to this (browser) thread and quits it.
bool ReadBlocking(dbus::ObjectProxy* proxy, uint32_t* color_scheme_out) {
  dbus::MethodCall method_call(kSettingsInterface, kReadMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kNamespace);
  writer.AppendString(kColorSchemeKey);

  base::RunLoop run_loop;
  bool parsed = false;
  proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnReadDone, run_loop.QuitClosure(), color_scheme_out,
                     &parsed));
  run_loop.Run();

  if (!parsed) {
    LOG(WARNING) << "Failed to read org.freedesktop.appearance.color-scheme "
                    "from the settings portal";
  }
  return parsed;
}

}  // namespace

void InitDarkModeFromPortal() {
  static bool started = false;
  if (started) {
    return;
  }
  started = true;

  scoped_refptr<dbus::Bus> bus = dbus_thread_linux::GetSharedSessionBus();
  dbus::ObjectProxy* proxy =
      bus->GetObjectProxy(kPortalService, dbus::ObjectPath(kPortalPath));

  uint32_t color_scheme = 0;
  if (ReadBlocking(proxy, &color_scheme)) {
    Apply(color_scheme);
  }

  // Stay in step with later theme switches.
  dbus_utils::ConnectToSignal<"ssv">(
      proxy, kSettingsInterface, kChangedSignal,
      base::BindRepeating(&OnSettingChanged),
      base::BindOnce(&OnSignalConnected));
}

}  // namespace tma
