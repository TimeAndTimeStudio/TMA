// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/app/content_main.h"
#include "tma/app/tma_main_delegate.h"

namespace {

using Args = std::span<const char* const>;

// Chromium launches every helper -- renderer, GPU, zygote, utility -- off this
// same binary and names it on the command line with --type=<kind>. That is the
// one thing a process may legitimately carry, so a command line without it is
// the browser process, and therefore the only one a human can have started.
bool LaunchedByChromium(Args args) {
  for (const char* const arg : args) {
    if (std::string_view(arg).starts_with("--type=")) {
      return true;
    }
  }
  return false;
}

// Checked on raw argv rather than on base::CommandLine so that this sees
// exactly what was typed: Chromium appends switches to its own command line
// as it starts up, and none of those are a human asking for something.
void RefuseArguments(Args args) {
  std::string joined;
  bool first = true;
  for (const char* const arg : args) {
    if (!first) {
      joined += ' ';
    }
    first = false;
    joined += arg;
  }
  std::fprintf(stderr,
               "error: TMA takes no arguments; refused '%s'.\n"
               "       What the window opens is startup in tma.conf and how\n"
               "       small it may get is min_width / min_height. Both are\n"
               "       baked in at build time: edit tma.conf and re-run\n"
               "       ./build.sh.\n",
               joined.c_str());
}

}  // namespace

int main(int argc, const char** argv) {
  // argv[0] is the program itself; everything after it is a request the shell
  // is not making. argc is always at least 1, so subspan(1) is in range.
  const Args all{argv, static_cast<size_t>(argc)};
  const Args tail = all.subspan(1);

  if (!tail.empty() && !LaunchedByChromium(tail)) {
    RefuseArguments(tail);
    return 2;
  }

  tma::TmaMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}
