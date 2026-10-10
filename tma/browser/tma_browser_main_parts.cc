// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_browser_main_parts.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "net/base/filename_util.h"
#include "net/base/module/net_module.h"
#include "net/grit/net_resources.h"
#include "tma/browser/tma_dark_mode.h"
#include "tma/browser/tma_metrics.h"
#include "tma/browser/tma_platform_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace tma {

namespace {

constexpr char kWindowSizeSwitch[] = "window-size";

// Spliced together by string-literal concatenation from TMA_APP_NAME, so that
// renaming the app in tma.conf moves the directory TMA looks in as well;
// //tma/BUILD.gn copies the HTML into a directory named the same way. tma.conf
// restricts app_name to ASCII characters that need no percent-encoding, which
// is what makes it safe to splice into the fallback URL below too.
constexpr char kResourcesDirName[] = TMA_APP_NAME "_resources";
// The packaged entry point. tma.conf's `startup = file` deliberately names no
// file, so this is the one place that decides what the app page is called.
constexpr char kEntryFileName[] = "index.html";

// Percent-encoded fallback page, shown when the packaged entry point is not on
// disk. There is nothing else to fall back to: tma.conf requires startup, and
// it only ever chooses between this page and a URL.
constexpr char kMissingResourcesUrl[] =
    "data:text/html;charset=utf-8,"
    "%3C!doctype%20html%3E%3Cmeta%20charset%3Dutf-8%3E"
    "%3Ctitle%3E" TMA_APP_NAME "%3C%2Ftitle%3E"
    "%3Cbody%20style%3D%22font%3A16px%20system-ui%2Csans-serif%3B"
    "margin%3A4rem%2Ccolor%3A%23444%22%3E"
    "%3Ch1%3E" TMA_APP_NAME "%3C%2Fh1%3E"
    "%3Cp%3ENo%20resources%20found.%20Expected%20"
    "%22" TMA_APP_NAME
    "_resources%2Findex.html%22%20next%20to%20the%20executable%2C%20or%20"
    "pass%20a%20URL%20on%20the%20command%20line.%3C%2Fp%3E"
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
  content::Shell::CreateNewWindow(browser_context(), GetStartupURL(),
                                  /*site_instance=*/nullptr, GetWindowSize());
}

GURL TmaBrowserMainParts::GetStartupURL() const {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // A bare command line argument wins over tma.conf. There is deliberately no
  // --url= switch as well: two spellings of one idea means two things to
  // document and two things to get wrong, and a bare argument covers
  // everything a switch would -- both a URL and a path have a scheme or a
  // leading slash, and neither needs a flag in front of it.
  const base::CommandLine::StringVector& args = command_line->GetArgs();
  if (!args.empty()) {
    const std::string& first = args[0];
    GURL url(first);
    if (url.is_valid() && url.has_scheme()) {
      return url;
    }
    const base::FilePath path =
        base::MakeAbsoluteFilePath(base::FilePath(first));
    if (!path.empty()) {
      return net::FilePathToFileURL(path);
    }
  }

  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);

  // startup from tma.conf, compiled in by //tma/BUILD.gn. It sits below the
  // command line on purpose: an argument on a given launch is a more specific
  // statement of intent than a value recorded once at build time.
  //
  // TMA_STARTUP_URL is empty when tma.conf says `startup = file`, so what
  // follows is either that URL or the packaged page, never both. The format of
  // the single `startup` key belongs to setup_chromium.sh, which rejects
  // anything that is neither "file" nor "url:<url>", so nothing here
  // interprets a prefix (which also keeps clang from constant-folding the
  // branch away and failing the build on -Wunreachable-code).
  if (TMA_STARTUP_URL[0] != '\0') {
    const GURL url(TMA_STARTUP_URL);
    if (url.is_valid() && url.has_scheme()) {
      return url;
    }
    LOG(WARNING) << "tma.conf startup is not a usable URL: " << TMA_STARTUP_URL;
  }

  if (!exe_dir.empty()) {
    const base::FilePath entry =
        exe_dir.AppendASCII(kResourcesDirName).AppendASCII(kEntryFileName);
    if (base::PathExists(entry)) {
      return net::FilePathToFileURL(entry);
    }
    LOG(WARNING) << "TMA entry point not found at " << entry
                 << "; falling back to an inline placeholder page.";
  }

  return GURL(kMissingResourcesUrl);
}

std::unique_ptr<content::ShellPlatformDelegate>
TmaBrowserMainParts::CreateShellPlatformDelegate() {
  // content::ShellBrowserMainParts::CreateShellPlatformDelegate() returns the
  // stock content::ShellPlatformDelegate, which would create a content-shell
  // window with a native frame. TMA needs its own.
  return std::make_unique<TmaPlatformDelegate>();
}

gfx::Size TmaBrowserMainParts::GetWindowSize() const {
  gfx::Size size(kDefaultWindowWidth, kDefaultWindowHeight);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kWindowSizeSwitch)) {
    return size;
  }

  const std::string value =
      command_line->GetSwitchValueASCII(kWindowSizeSwitch);
  const auto parts = base::SplitStringPiece(value, "x", base::TRIM_WHITESPACE,
                                            base::SPLIT_WANT_NONEMPTY);
  int width = 0;
  int height = 0;
  if (parts.size() == 2 && base::StringToInt(parts[0], &width) &&
      base::StringToInt(parts[1], &height) && width > 0 && height > 0) {
    size.SetSize(width, height);
  } else {
    LOG(WARNING) << "Invalid --" << kWindowSizeSwitch << "=" << value
                 << "; expected <width>x<height>.";
  }
  return size;
}

}  // namespace tma
