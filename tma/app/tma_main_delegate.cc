// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/app/tma_main_delegate.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/content_browser_client.h"
#include "tma/browser/tma_content_browser_client.h"

namespace tma {

TmaMainDelegate::TmaMainDelegate() = default;

TmaMainDelegate::~TmaMainDelegate() = default;

std::optional<int> TmaMainDelegate::BasicStartupComplete() {
  // Content shell starts a remote DevTools HTTP server on 127.0.0.1 unless it
  // is explicitly asked not to. TMA is a display shell, so the server is never
  // started (see TmaBrowserMainParts::PreMainMessageLoopRun()).
  //
  // Arguments from a human are already gone: main() refuses anything that is
  // not Chromium's own --type=, before ContentMain and therefore before this
  // runs. What is logged below is therefore what Chromium itself is running
  // this process with, which for a helper means its --type and nothing else.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  VLOG(1) << "TMA starting with command line: "
          << command_line->GetCommandLineString();

  return content::ShellMainDelegate::BasicStartupComplete();
}

content::ContentBrowserClient* TmaMainDelegate::CreateContentBrowserClient() {
  if (!browser_client_) {
    browser_client_ = std::make_unique<TmaContentBrowserClient>();
  }
  return browser_client_.get();
}

}  // namespace tma
