// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_
#define TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "content/shell/browser/shell_browser_main_parts.h"
#include "ui/gfx/geometry/size.h"

class GURL;

namespace tma {

// Replaces content shell's window bootstrap:
//  * never starts the remote DevTools HTTP server,
//  * creates the window through TmaPlatformDelegate (client-side frame with a
//    custom TMA top bar),
//  * loads the app URL instead of falling back to https://www.google.com/.
class TmaBrowserMainParts : public content::ShellBrowserMainParts {
 public:
  TmaBrowserMainParts();
  TmaBrowserMainParts(const TmaBrowserMainParts&) = delete;
  TmaBrowserMainParts& operator=(const TmaBrowserMainParts&) = delete;
  ~TmaBrowserMainParts() override;

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;

 protected:
  // content::ShellBrowserMainParts:
  void InitializeMessageLoopContext() override;
  std::unique_ptr<content::ShellPlatformDelegate>
  CreateShellPlatformDelegate() override;

 private:
  // URL to open in the first window. Resolution order:
  //   1. first non-switch command line argument (URL or file path)
  //   2. startup from tma.conf when it is a URL (startup = url:...)
  //   3. file://<exe dir>/<app_name>_resources/index.html, which is what
  //      startup = file means
  //   4. an inline data: URL describing the missing resource
  GURL GetStartupURL() const;

  // Initial window size from --window-size=<width>x<height>, else the default.
  gfx::Size GetWindowSize() const;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_
