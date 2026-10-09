// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_content_browser_client.h"

#include <utility>

#include "tma/browser/tma_browser_main_parts.h"

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

}  // namespace tma
