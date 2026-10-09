// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tma/browser/tma_widget_delegate.h"

#include <memory>

#include "tma/browser/tma_frame_view.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace tma {

TmaWidgetDelegate::TmaWidgetDelegate() = default;

TmaWidgetDelegate::~TmaWidgetDelegate() = default;

std::unique_ptr<views::FrameView> TmaWidgetDelegate::CreateFrameView(
    views::Widget* widget) {
  // Widget::CreateFrameView() asks the delegate first and only falls back to
  // the native/default frame when this returns null, so returning a value here
  // is what keeps TMA's own frame in charge.
  return std::make_unique<TmaFrameView>(widget);
}

bool TmaWidgetDelegate::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  // The content window's non-client view is null while the widget is still
  // being built (and can legitimately be absent for a delegate-only widget);
  // there is no frame to consult yet, so let aura make its usual choice.
  views::NonClientView* non_client_view =
      GetWidget() ? GetWidget()->non_client_view() : nullptr;
  if (!non_client_view)
    return true;

  // Only the resize grips are claimed. HTCAPTION and the button strip stay
  // descended-into so that popup/child windows parked over the title bar keep
  // receiving input, and HTCLIENT keeps the page clickable right up to the
  // edge of the client area.
  return !ui::IsResizingComponent(non_client_view->NonClientHitTest(location));
}

}  // namespace tma
