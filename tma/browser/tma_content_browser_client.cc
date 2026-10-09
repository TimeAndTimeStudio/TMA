// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_content_browser_client.h"

#include <utility>

#include "content/shell/browser/shell_content_browser_client.h"
#include "tma/browser/tma_browser_main_parts.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/native_theme/native_theme.h"

namespace tma {

TmaContentBrowserClient::TmaContentBrowserClient() = default;

TmaContentBrowserClient::~TmaContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
TmaContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto parts = std::make_unique<TmaBrowserMainParts>();
  // Required by ShellContentBrowserClient whenever CreateBrowserMainParts() is
  // overridden in a subclass.
  set_browser_main_parts(parts.get());
  return parts;
}

void TmaContentBrowserClient::OverrideWebPreferences(
    content::WebContents* web_contents,
    content::SiteInstance& main_frame_site,
    blink::web_pref::WebPreferences* prefs) {
  content::ShellContentBrowserClient::OverrideWebPreferences(
      web_contents, main_frame_site, prefs);

  // ShellContentBrowserClient::OverrideWebPreferences hardcodes
  // preferred_color_scheme: kDark only when --force-dark-mode is on the command
  // line, and kLight otherwise. It never consults the desktop, so every page
  // would be light no matter what the session says. Ask NativeTheme instead —
  // tma_dark_mode.cc keeps that in step with org.freedesktop.appearance.
  // blink::mojom::PreferredColorScheme has no "no preference" value, so an
  // undecided theme keeps whatever the base implementation chose.
  if (ui::NativeTheme::GetInstanceForWeb()->preferred_color_scheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  }
}

}  // namespace tma
