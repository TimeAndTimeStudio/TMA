// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/app/tma_main_delegate.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "content/public/browser/content_browser_client.h"
#include "tma/browser/tma_content_browser_client.h"

namespace tma {

namespace {

// Chromium reaches its Vulkan stack through three switches, and TMA takes
// none of them from a human -- main() refuses every argument -- so TMA adds
// all three here. Which graphics stack the shell runs on is a property of the
// build rather than of whoever types at the binary, which is the same bargain
// tma.conf already makes for the startup URL.
//
// Wayland-only needs no switch at all: X11 is not compiled in (see the GN
// args), so there is no second platform for the shell to drift onto.
//
// 2 below is the "only" in Wayland + Vulkan only -- native Vulkan is requested
// as a switch value the GPU blocklist may not rescind, rather than as a
// feature default it can.
void RequireVulkanGraphics(base::CommandLine* command_line) {
  // 1. features::kVulkan. Disabled by default on every platform but Android
  //    (gpu/config/gpu_finch_features.cc), which is why Vulkan was never
  //    reached before: service_utils.cc derives both GpuPreferences::use_vulkan
  //    and the Skia GrContext type from this one feature, and
  //    webgpu_decoder_impl.cc only offers WebGPU a real backend when that
  //    context is Vulkan-backed. On Linux it hands out the Null backend
  //    otherwise -- the "Deliberately disable compat on linux" branch -- and
  //    requestAdapter() answers null.
  //
  //    Merged into any list already present rather than set: a helper launched
  //    by Chromium may carry --enable-features of its own, and the switch is a
  //    map entry, so overwriting would silently drop theirs.
  constexpr std::string_view kVulkan = "Vulkan";
  const std::string existing =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  bool already = false;
  for (const std::string& feature :
       base::SplitString(existing, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    if (feature == kVulkan) {
      already = true;
      break;
    }
  }
  if (!already) {
    command_line->AppendSwitchASCII(
        switches::kEnableFeatures,
        existing.empty() ? std::string(kVulkan) : existing + ",Vulkan");
  }

  // 2. --use-vulkan=native, which service_utils.cc maps to
  //    VulkanImplementationName::kForcedNative: Vulkan the GPU blocklist is
  //    not permitted to override.
  command_line->AppendSwitchASCII("use-vulkan", "native");

  // 3. --use-angle=vulkan, the same stack for WebGL. gl/init/
  //    gl_display_initializer.cc reads it to pick ANGLE's backend, and
  //    gl_factory.cc infers --use-gl=angle when --use-angle is present, so
  //    that switch is not needed. Left alone, ANGLE still reports OpenGL ES
  //    but translates to Mesa's desktop GL, and the shell would run WebGL and
  //    WebGPU on two different APIs while claiming one of them.
  command_line->AppendSwitchASCII("use-angle", "vulkan");
}

}  // namespace

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
  // this process with, plus the one switch TMA adds for it -- which is why
  // this runs in every process, browser and helper alike: the GPU process is
  // where service_utils.cc reads the feature back.
  //
  // BasicStartupComplete is called after base::CommandLine has been built, so
  // the switch is added late enough to see everything Chromium set and early
  // enough that nothing has read the feature yet.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  RequireVulkanGraphics(command_line);
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
