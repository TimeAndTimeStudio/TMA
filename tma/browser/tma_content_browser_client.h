// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_
#define TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "content/shell/browser/shell_content_browser_client.h"

namespace tma {

// The only thing TMA changes about content shell's browser client is which
// BrowserMainParts get created, so that the window/URL/platform delegate can
// be replaced.
class TmaContentBrowserClient : public content::ShellContentBrowserClient {
 public:
  TmaContentBrowserClient();
  TmaContentBrowserClient(const TmaContentBrowserClient&) = delete;
  TmaContentBrowserClient& operator=(const TmaContentBrowserClient&) = delete;
  ~TmaContentBrowserClient() override;

  // content::ShellContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_CONTENT_BROWSER_CLIENT_H_
