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

// TMA runs Wayland + Vulkan + WebGPU, and nothing else. Chromium reaches all
// of that through switches, TMA takes none of them from a human -- main()
// refuses every argument -- so TMA appends them itself. Which stack the shell
// runs on is therefore a property of the build, the same bargain tma.conf
// already makes for the startup URL.
//
// Wayland needs no switch at all: X11 is not compiled in (see the GN args), so
// there is no second platform for the shell to drift onto.
//
// Three switches reach Vulkan, one takes WebGL away. Switch 2 is the "only" in
// Wayland + Vulkan only -- native Vulkan is requested as a switch value the
// GPU blocklist may not rescind, rather than as a feature default it can.
void ConfigureTmaGraphics(base::CommandLine* command_line) {
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

  // 3. --use-angle=vulkan. WebGPU is the only rendering API TMA offers, but
  //    ANGLE is still linked in and still picks a backend of its own --
  //    gl/init/gl_display_initializer.cc reads this switch, and gl_factory.cc
  //    infers --use-gl=angle from its presence, so that one is not needed.
  //    Left unset, ANGLE defaults to Mesa's desktop GL, which would pull a
  //    second GPU API into a shell that claims one. Keeping it makes any ANGLE
  //    context that does get created Vulkan, never desktop GL.
  command_line->AppendSwitchASCII("use-angle", "vulkan");

  // 4. --disable-webgl. content/browser/web_contents/web_contents_impl.cc
  //    derives prefs.webgl1_enabled and prefs.webgl2_enabled from this single
  //    switch (along with kDisable3DAPIs, which would also catch WebGL2), so
  //    one switch retires both versions at once. WebGPU is gated somewhere
  //    else entirely -- the feature list -- which is why disabling WebGL does
  //    not touch it.
  //
  //    With the pref off Blink never creates the context, so
  //    canvas.getContext('webgl') answers null instead of handing back a
  //    wrapper around ANGLE. This is what makes the stack "WebGPU only":
  //    without it the shell offers two 3D APIs, and the second one reaches
  //    the GPU by a path the shell never asked for.
  command_line->AppendSwitch("disable-webgl");
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
  ConfigureTmaGraphics(command_line);
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
