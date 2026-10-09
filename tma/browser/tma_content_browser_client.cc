// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
