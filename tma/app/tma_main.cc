// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include <utility>

#include "content/public/app/content_main.h"
#include "tma/app/tma_main_delegate.h"

int main(int argc, const char** argv) {
  tma::TmaMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}
