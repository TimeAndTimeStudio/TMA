// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_
#define TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "content/shell/browser/shell_browser_main_parts.h"

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
  // startup from tma.conf, compiled in. There is no command-line argument and
  // no switch that can change it; a value that is not a usable URL opens an
  // inline data: URL saying so instead. See tma.conf.
  GURL GetStartupURL() const;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_BROWSER_MAIN_PARTS_H_
