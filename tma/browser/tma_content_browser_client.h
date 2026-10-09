// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_
#define TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "content/shell/browser/shell_content_browser_client.h"

namespace tma {

// TMA differs from content shell in two ways: which BrowserMainParts get
// created (so the window/URL/platform delegate can be replaced), and how the
// preferred color scheme is picked.
class TmaContentBrowserClient : public content::ShellContentBrowserClient {
 public:
  TmaContentBrowserClient();
  TmaContentBrowserClient(const TmaContentBrowserClient&) = delete;
  TmaContentBrowserClient& operator=(const TmaContentBrowserClient&) = delete;
  ~TmaContentBrowserClient() override;

  // content::ShellContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  void OverrideWebPreferences(content::WebContents* web_contents,
                              content::SiteInstance& main_frame_site,
                              blink::web_pref::WebPreferences* prefs) override;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_
