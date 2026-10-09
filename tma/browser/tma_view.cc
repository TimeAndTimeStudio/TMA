// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_view.h"

#include <memory>
#include <utility>

#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "tma/browser/tma_fullscreen.h"
#include "tma/browser/tma_metrics.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace tma {

TmaView::TmaView(content::Shell* shell) : shell_(shell) {
  // views::View has no layout manager by default, so the WebView would never
  // be laid out. The frame gives this view exactly the client rectangle.
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

TmaView::~TmaView() = default;

void TmaView::SetWebContents(content::WebContents* web_contents,
                              const gfx::Size& content_size) {
  if (web_view_) {
    // ExtractAsDangling clears this member and hands back a pointer that is
    // allowed to dangle; RemoveChildViewT returns ownership so the previous
    // WebView is destroyed here rather than leaking as an orphaned child.
    RemoveChildViewT(web_view_.ExtractAsDangling().get());
  }

  web_view_ = AddChildView(
      std::make_unique<views::WebView>(web_contents->GetBrowserContext()));
  web_view_->SetWebContents(web_contents);
  web_view_->SetPreferredSize(content_size);
  web_contents->Focus();
}

void TmaView::AddedToWidget() {
  views::View::AddedToWidget();
  // Content shell registers its own accelerators (F5, browser back/forward)
  // the same way; the focus manager dispatches them before the event reaches
  // the renderer for keys the page did not handle.
  AddAccelerator(ui::Accelerator(ui::VKEY_F11, ui::EF_NONE));
  // The accelerator above only runs when the focus manager has a focused
  // view, which TMA does not guarantee. This handler covers the rest.
  fullscreen_key_handler_ =
      std::make_unique<TmaFullscreenKeyHandler>(GetWidget());
}

void TmaView::RemovedFromWidget() {
  // Dropped here rather than in ~TmaView(): Widget::DestroyRootView() runs
  // this first, while the native window it is registered on still exists.
  fullscreen_key_handler_.reset();
  views::View::RemovedFromWidget();
}

bool TmaView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F11) {
    if (views::Widget* widget = GetWidget()) {
      SetNativeFullscreen(widget, !widget->IsFullscreen());
    }
    return true;
  }
  return views::View::AcceleratorPressed(accelerator);
}

gfx::Size TmaView::GetMinimumSize() const {
  return gfx::Size(kMinWindowWidth, kMinWindowHeight);
}

BEGIN_METADATA(TmaView)
END_METADATA

}  // namespace tma
