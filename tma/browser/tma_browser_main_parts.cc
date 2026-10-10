// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_browser_main_parts.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "net/base/module/net_module.h"
#include "net/grit/net_resources.h"
#include "tma/browser/tma_dark_mode.h"
#include "tma/browser/tma_metrics.h"
#include "tma/browser/tma_platform_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace tma {

namespace {

// Percent-encoded fallback page, shown when the startup URL baked into this
// build is not usable. scripts/setup_chromium.sh rejects a tma.conf that does
// not state "url:<something with a scheme>", so reaching this means args.gn
// was edited by hand -- it says so rather than opening an empty window.
constexpr char kBadStartupUrl[] =
    "data:text/html;charset=utf-8,"
    "%3C!doctype%20html%3E%3Cmeta%20charset%3Dutf-8%3E"
    "%3Ctitle%3E" TMA_APP_NAME "%3C%2Ftitle%3E"
    "%3Cbody%20style%3D%22font%3A16px%20system-ui%2Csans-serif%3B"
    "margin%3A4rem%2Ccolor%3A%23444%22%3E"
    "%3Ch1%3E" TMA_APP_NAME "%3C%2Fh1%3E"
    "%3Cp%3EThe%20startup%20URL%20baked%20into%20this%20build%20is%20not%20"
    "a%20usable%20URL.%3C%2Fp%3E"
    "%3Cp%3ESet%20%22startup%20%3D%20url%3A...%22%20in%20tma.conf%20and%20"
    "re-run%20.%2Fbuild.sh.%3C%2Fp%3E"
    "%3C%2Fbody%3E";

// Content shell registers a resource provider so that net/ can look up
// IDR_DIR_HEADER_HTML (used to render directory listings). TMA re-registers
// the equivalent provider because it deliberately skips the DevTools HTTP
// handler that would otherwise be started alongside it.
scoped_refptr<base::RefCountedMemory> TmaPlatformResourceProvider(int key) {
  if (key == IDR_DIR_HEADER_HTML) {
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        IDR_DIR_HEADER_HTML);
  }
  return nullptr;
}

}  // namespace

TmaBrowserMainParts::TmaBrowserMainParts() = default;

TmaBrowserMainParts::~TmaBrowserMainParts() = default;

int TmaBrowserMainParts::PreMainMessageLoopRun() {
  // Content shell has no equivalent of
  // ChromeBrowserMainExtraPartsViewsLinux::dark_mode_manager_, and the one
  // upstream class that does exist cannot deliver the value in a non-GTK
  // build, so the desktop's color scheme never reaches Blink and
  // prefers-color-scheme is stuck on "light". See tma_dark_mode.h.
  InitDarkModeFromPortal();

  InitializeBrowserContexts();
  content::Shell::Initialize(CreateShellPlatformDelegate());
  net::NetModule::SetResourceProvider(TmaPlatformResourceProvider);

  // Deliberately not calling
  // content::ShellDevToolsManagerDelegate::StartHttpHandler() here: TMA is a
  // display shell and must not open a remote debugging port. The matching
  // StopHttpHandler() in ShellBrowserMainParts::PostMainMessageLoopRun() is
  // safe to call without a preceding Start.
  InitializeMessageLoopContext();
  return 0;
}

void TmaBrowserMainParts::InitializeMessageLoopContext() {
  // The window opens at the default size and nothing can change it: the
  // --window-size switch is deliberately gone, and the floor comes from
  // tma.conf's min_width / min_height (see TmaView::GetMinimumSize()).
  content::Shell::CreateNewWindow(
      browser_context(), GetStartupURL(), /*site_instance=*/nullptr,
      gfx::Size(kDefaultWindowWidth, kDefaultWindowHeight));
}

GURL TmaBrowserMainParts::GetStartupURL() const {
  // startup from tma.conf, compiled in by //tma/BUILD.gn. TMA takes no
  // command-line argument and no switch at all, so this is the only thing the
  // window will ever open -- see tma.conf.
  //
  // The format of that one key belongs to scripts/setup_chromium.sh, which
  // rejects anything that is not "url:<url>" and then rejects a URL with no
  // scheme, so nothing here interprets a string. The test stays because a
  // hand-edited args.gn is the one way to get a bad value in, and it should
  // open an explanation instead of an empty window.
  const GURL startup(TMA_STARTUP_URL);
  if (startup.is_valid() && startup.has_scheme()) {
    return startup;
  }
  LOG(WARNING) << "tma.conf startup is not a usable URL: " << TMA_STARTUP_URL;
  return GURL(kBadStartupUrl);
}

std::unique_ptr<content::ShellPlatformDelegate>
TmaBrowserMainParts::CreateShellPlatformDelegate() {
  // content::ShellBrowserMainParts::CreateShellPlatformDelegate() returns the
  // stock content::ShellPlatformDelegate, which would create a content-shell
  // window with a native frame. TMA needs its own.
  return std::make_unique<TmaPlatformDelegate>();
}

}  // namespace tma
