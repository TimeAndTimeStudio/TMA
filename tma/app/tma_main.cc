// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
