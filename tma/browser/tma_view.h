// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_VIEW_H_
#define TMA_BROWSER_TMA_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace content {
class Shell;
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

namespace tma {

class TmaFullscreenKeyHandler;

// The contents view of a TMA window.
//
// It owns the content::Shell that drives the window (the Shell is self-owned
// and is destroyed through this view's destructor), embeds that Shell's
// WebContents in a views::WebView which fills the client area, and installs
// the F11 handler that toggles native fullscreen.
class TmaView : public views::View {
  METADATA_HEADER(TmaView, views::View)

 public:
  // Takes ownership of |shell|, mirroring content shell's ShellView.
  explicit TmaView(content::Shell* shell);
  TmaView(const TmaView&) = delete;
  TmaView& operator=(const TmaView&) = delete;
  ~TmaView() override;

  // Attaches |web_contents| (owned by the Shell) to a fresh WebView sized for
  // |content_size|. Replaces any WebView installed by an earlier call.
  void SetWebContents(content::WebContents* web_contents,
                       const gfx::Size& content_size);

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size GetMinimumSize() const override;

 private:
  // Declared first so that it is destroyed last among the members: ~Shell()
  // is what closes this window, so everything else here must still be alive
  // when it runs.
  const std::unique_ptr<content::Shell> shell_;

  // Owned by the view hierarchy as soon as it has been added.
  raw_ptr<views::WebView> web_view_ = nullptr;

  // Installed as a pre-target handler on the widget's native window for as
  // long as this view belongs to a widget; see TmaFullscreenKeyHandler for
  // why views::View::AddAccelerator() alone is not enough. The handler
  // unregisters itself if the window goes first, so declaration order here
  // does not matter.
  std::unique_ptr<TmaFullscreenKeyHandler> fullscreen_key_handler_;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_VIEW_H_
