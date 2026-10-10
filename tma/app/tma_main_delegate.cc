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

// Chromium keeps its Vulkan graphics backend behind base::Feature "Vulkan",
// which is disabled by default on every platform except Android
// (gpu/config/gpu_finch_features.cc). On Linux that gate decides far more than
// compositing:
//
//   * gpu/command_buffer/service/service_utils.cc derives both
//     GpuPreferences::use_vulkan and the Skia GrContext type from it, so with
//     the feature off Skia runs on GL;
//   * gpu/command_buffer/service/webgpu_decoder_impl.cc only offers WebGPU a
//     real backend when that Skia context is Vulkan-backed. On Linux it
//     otherwise hands out the Null backend -- see the "Deliberately disable
//     compat on linux" branch -- and requestAdapter() answers null, which the
//     page sees as "No available adapters."
//
// So this one feature is the whole difference between WebGPU existing and not,
// and no compile-time argument reaches it: features are parsed from the
// command line. TMA takes no switch from a human (main() refuses every one),
// so TMA appends this switch itself. The graphics stack a shell runs on is
// therefore decided by the build, not by whoever types at the binary.
//
// Merged into an existing --enable-features rather than set, because a helper
// launched by Chromium may already carry a list of its own and overwriting the
// map entry would silently drop it.
void EnableVulkanGraphics(base::CommandLine* command_line) {
  constexpr std::string_view kVulkan = "Vulkan";
  const std::string existing =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  for (const std::string& feature :
       base::SplitString(existing, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    if (feature == kVulkan) {
      return;
    }
  }
  command_line->AppendSwitchASCII(
      switches::kEnableFeatures,
      existing.empty() ? std::string(kVulkan) : existing + ",Vulkan");
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
  EnableVulkanGraphics(command_line);
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
