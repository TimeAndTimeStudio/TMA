// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_WIDGET_DELEGATE_H_
#define TMA_BROWSER_TMA_WIDGET_DELEGATE_H_

#include <memory>

#include "ui/views/widget/widget_delegate.h"

namespace views {
class FrameView;
class Widget;
}  // namespace views

namespace tma {

// The views::WidgetDelegate for a TMA window. Its only job is to install the
// custom client-side frame; everything else about the window is configured by
// TmaPlatformDelegate::CreatePlatformWindow().
//
// Lifetime note: TMA does *not* call SetOwnedByWidget(), because that pass key
// is only constructible by friends of views::WidgetDelegate and TMA is not one
// of them. TmaPlatformDelegate keeps the delegate alive in its per-Shell data
// instead, which gives the same guarantee (the delegate outlives the Widget)
// without touching Chromium's friend list.
class TmaWidgetDelegate : public views::WidgetDelegate {
 public:
  TmaWidgetDelegate();
  TmaWidgetDelegate(const TmaWidgetDelegate&) = delete;
  TmaWidgetDelegate& operator=(const TmaWidgetDelegate&) = delete;
  ~TmaWidgetDelegate() override;

  // views::WidgetDelegate:
  std::unique_ptr<views::FrameView> CreateFrameView(
      views::Widget* widget) override;

  // Called by aura while picking a target window, once per child of this
  // widget's content window, with |location| already in the content window's
  // coordinates (which are the RootView's coordinates). Returning false keeps
  // the event on the content window instead of letting it fall through to the
  // renderer's surface window.
  //
  // TMA needs that on purpose: aura::wm::CompoundEventFilter reads the resize
  // cursor from target->delegate()->GetNonClientComponent(), and the renderer
  // window's delegate answers HTCLIENT unconditionally, so without this the
  // frame's HTLEFT/HTRIGHT/... never reaches a delegate that knows about it
  // and the cursor stays a plain arrow over the resize grips. The renderer
  // should not receive those events anyway; TmaFrameView treats them as
  // non-client areas and HandleLocatedEventWithHitTest starts a native resize.
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_WIDGET_DELEGATE_H_
