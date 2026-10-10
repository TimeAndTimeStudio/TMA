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

// The only two things a human may ask for, and both report the same number
// because TMA has no version of its own: it is a shell with no code of
// consequence outside //tma, so what identifies a build is the Chromium it
// embeds. --version names the product alongside it; --version-browser is the
// bare value, for anything that means to compare rather than read.
//
// Matched against raw argv rather than base::CommandLine so that this sees
// exactly what was typed: Chromium appends switches to its own command line as
// it starts up, and none of those are a request anyone made of TMA.
bool PrintVersion(Args args) {
  if (args.size() != 1) {
    return false;
  }
  const std::string_view arg(args.front());
  if (arg == "--version") {
    // Everything that names this build, in the order a bug report wants it:
    // the shell, its release, the id a compositor matches it by, and the
    // Chromium underneath.
    std::printf("%s %s (app_id %s, Chromium %s)\n", TMA_APP_NAME, TMA_VERSION,
                TMA_APP_ID, TMA_CHROMIUM_VERSION);
    return true;
  }
  if (arg == "--version-browser") {
    std::printf("Chromium %s\n", TMA_CHROMIUM_VERSION);
    return true;
  }
  return false;
}

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
               "error: TMA refused '%s'.\n"
               "       The only flags TMA accepts are --version and\n"
               "       --version-browser. TMA exits here rather than starting:\n"
               "       no window opens and this argument is never read.\n",
               joined.c_str());
}

}  // namespace

int main(int argc, const char** argv) {
  // argv[0] is the program itself; everything after it is a request the shell
  // is not making. argc is always at least 1, so subspan(1) is in range.
  const Args all{argv, static_cast<size_t>(argc)};
  const Args tail = all.subspan(1);

  if (!tail.empty() && !LaunchedByChromium(tail)) {
    if (PrintVersion(tail)) {
      return 0;
    }
    RefuseArguments(tail);
    return 2;
  }

  tma::TmaMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}
