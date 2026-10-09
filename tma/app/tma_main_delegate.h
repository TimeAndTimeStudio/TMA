// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_APP_TMA_MAIN_DELEGATE_H_
#define TMA_APP_TMA_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "content/public/app/content_main_delegate.h"
#include "content/shell/app/shell_main_delegate.h"

namespace tma {

// Process-level entry point for every TMA process (browser, renderer, gpu, ...).
//
// Everything TMA does not need to change is inherited from Chromium's
// ShellMainDelegate, so TMA stays a thin shell on top of official Chromium
// source: no fork, no CEF, no Electron.
class TmaMainDelegate : public content::ShellMainDelegate {
 public:
  TmaMainDelegate();
  TmaMainDelegate(const TmaMainDelegate&) = delete;
  TmaMainDelegate& operator=(const TmaMainDelegate&) = delete;
  ~TmaMainDelegate() override;

  // content::ContentMainDelegate:
  std::optional<int> BasicStartupComplete() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
};

}  // namespace tma

#endif  // TMA_APP_TMA_MAIN_DELEGATE_H_
