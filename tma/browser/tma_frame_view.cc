// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_frame_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkColor.h"
#include "tma/browser/tma_metrics.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace tma {

namespace {

// The title strip is a single constant dark shade for both active and inactive
// windows. Deriving it from the colour provider's active/inactive pair made the
// strip change colour the moment the window (de)activated -- which happens on
// the first click on a caption button -- and left the buttons painted with the
// stale colour of the previous state, because SetCaptionColors() updated their
// members without scheduling a repaint.
constexpr SkColor kFrameColor = SkColorSetARGB(0xFF, 0x20, 0x21, 0x24);
constexpr SkColor kCaptionForeground = SkColorSetARGB(0xFF, 0xE8, 0xEA, 0xED);

}  // namespace

TmaFrameView::TmaFrameView(views::Widget* widget) : widget_(widget) {
  minimize_button_ = AddChildView(std::make_unique<TmaCaptionButton>(
      TmaCaptionButton::Type::kMinimize,
      base::BindRepeating(&views::Widget::Minimize, base::Unretained(widget))));
  maximize_button_ = AddChildView(std::make_unique<TmaCaptionButton>(
      TmaCaptionButton::Type::kMaximize,
      base::BindRepeating(&views::Widget::Maximize, base::Unretained(widget))));
  restore_button_ = AddChildView(std::make_unique<TmaCaptionButton>(
      TmaCaptionButton::Type::kRestore,
      base::BindRepeating(&views::Widget::Restore, base::Unretained(widget))));
  close_button_ = AddChildView(std::make_unique<TmaCaptionButton>(
      TmaCaptionButton::Type::kClose,
      base::BindRepeating(&views::Widget::Close, base::Unretained(widget))));
}

TmaFrameView::~TmaFrameView() = default;

int TmaFrameView::FrameBorderThickness() const {
  // Always zero: TMA draws no window border. The gap the border used to leave
  // between the title strip and the screen edge is now gone, and the resize
  // grip it provided is replaced by kResizeHitThickness in NonClientHitTest().
  return kFrameBorderThickness;
}

int TmaFrameView::TopBorderHeight() const {
  if (widget_->IsFullscreen()) {
    return 0;
  }
  return FrameBorderThickness() + kTitleBarHeight;
}

gfx::Rect TmaFrameView::GetBoundsForClientView() const {
  const int border = FrameBorderThickness();
  const int top = TopBorderHeight();
  return gfx::Rect(border, top, std::max(0, width() - 2 * border),
                   std::max(0, height() - top - border));
}

gfx::Rect TmaFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const int border = FrameBorderThickness();
  const int top = TopBorderHeight();
  return gfx::Rect(client_bounds.x() - border, client_bounds.y() - top,
                   client_bounds.width() + 2 * border,
                   client_bounds.height() + top + border);
}

int TmaFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!GetLocalBounds().Contains(point)) {
    return HTNOWHERE;
  }

  // Window controls. Checked first so that the buttons keep their full size,
  // matching views::DefaultFrameView; they also sit on top of the invisible
  // resize grip, which would otherwise swallow the outer DIP of each button.
  if (close_button_->GetVisible() && close_button_->bounds().Contains(point)) {
    return HTCLOSE;
  }
  if (restore_button_->GetVisible() && restore_button_->bounds().Contains(point)) {
    return HTMAXBUTTON;
  }
  if (maximize_button_->GetVisible() &&
      maximize_button_->bounds().Contains(point)) {
    return HTMAXBUTTON;
  }
  if (minimize_button_->GetVisible() &&
      minimize_button_->bounds().Contains(point)) {
    return HTMINBUTTON;
  }

  // Resize edges and corners. This has to be answered before the client view:
  // with kFrameBorderThickness == 0 the client view covers the whole window
  // perimeter, so asking it first would report HTCLIENT for every edge and
  // corner and leave the window unresizable. The grip is invisible -- the frame
  // no longer paints a border -- but it keeps the behaviour the border had.
  const int window_component = ResizeComponentAt(point);
  if (window_component != HTNOWHERE) {
    return window_component;
  }

  // Inside the web content area there is nothing non-client to do.
  const int client_component =
      widget_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE) {
    return client_component;
  }

  // Everything else in the title strip drags the window.
  return HTCAPTION;
}

int TmaFrameView::ResizeComponentAt(const gfx::Point& point) const {
  const int w = width();
  const int h = height();

  // Each corner is a full kResizeAreaCornerSize square rather than the
  // edge band views::FrameView::GetHTComponentForFrame() produced. That helper
  // rejects the point unless it is already within kResizeHitThickness of the
  // perimeter and only then widens it to the corner size, so the corners were
  // never more than kResizeHitThickness deep and kResizeAreaCornerSize did
  // almost nothing. Checking the squares first is what makes the very corner
  // of the window grabbable.
  const bool near_left = point.x() < kResizeAreaCornerSize;
  const bool near_right = point.x() >= w - kResizeAreaCornerSize;
  const bool near_top = point.y() < kResizeAreaCornerSize;
  const bool near_bottom = point.y() >= h - kResizeAreaCornerSize;
  if ((near_left || near_right) && (near_top || near_bottom)) {
    if (!widget_->widget_delegate()->CanResize()) {
      // A window that cannot be resized still reports a border strip around
      // the client area, matching GetHTComponentForFrame(). HTBORDER is not a
      // resizing component, so it never reaches the WM's resize handler.
      return HTBORDER;
    }
    if (near_left) {
      return near_top ? HTTOPLEFT : HTBOTTOMLEFT;
    }
    return near_top ? HTTOPRIGHT : HTBOTTOMRIGHT;
  }

  // Edges: a kResizeHitThickness strip measured from the window perimeter.
  const bool on_left = point.x() < kResizeHitThickness;
  const bool on_right = point.x() >= w - kResizeHitThickness;
  const bool on_top = point.y() < kResizeHitThickness;
  const bool on_bottom = point.y() >= h - kResizeHitThickness;
  if (on_left || on_right || on_top || on_bottom) {
    if (!widget_->widget_delegate()->CanResize()) {
      return HTBORDER;
    }
    if (on_top) {
      if (on_left) {
        return HTTOPLEFT;
      }
      return on_right ? HTTOPRIGHT : HTTOP;
    }
    if (on_bottom) {
      if (on_left) {
        return HTBOTTOMLEFT;
      }
      return on_right ? HTBOTTOMRIGHT : HTBOTTOM;
    }
    if (on_left) {
      return HTLEFT;
    }
    return HTRIGHT;
  }

  return HTNOWHERE;
}

void TmaFrameView::SizeConstraintsChanged() {
  LayoutCaptionButtons();
  SchedulePaint();
}

void TmaFrameView::OnPaint(gfx::Canvas* canvas) {
  const SkColor frame_color = FrameColor();
  canvas->FillRect(GetLocalBounds(), frame_color);

  // The controls are laid out in physical coordinates (centred, no RTL
  // mirroring), which is also how NonClientHitTest() reads them back.
  const SkColor foreground = CaptionForegroundColor();
  minimize_button_->SetCaptionColors(frame_color, foreground);
  maximize_button_->SetCaptionColors(frame_color, foreground);
  restore_button_->SetCaptionColors(frame_color, foreground);
  close_button_->SetCaptionColors(frame_color, foreground);
}

void TmaFrameView::OnMouseMoved(const ui::MouseEvent& event) {
  // RootView deliberately does not consult View::GetCursor() for events that
  // carry EF_IS_NON_CLIENT, so the resize cursors are applied here instead.
  UpdateCursorForLocation(event.location());
}

void TmaFrameView::OnMouseExited(const ui::MouseEvent& event) {
  widget_->SetCursor(ui::Cursor());
}

void TmaFrameView::UpdateCursorForLocation(const gfx::Point& point) {
  using ui::mojom::CursorType;
  CursorType cursor = CursorType::kNull;
  switch (NonClientHitTest(point)) {
    case HTLEFT:
      cursor = CursorType::kWestResize;
      break;
    case HTRIGHT:
      cursor = CursorType::kEastResize;
      break;
    case HTTOP:
      cursor = CursorType::kNorthResize;
      break;
    case HTBOTTOM:
      cursor = CursorType::kSouthResize;
      break;
    case HTTOPLEFT:
      cursor = CursorType::kNorthWestResize;
      break;
    case HTTOPRIGHT:
      cursor = CursorType::kNorthEastResize;
      break;
    case HTBOTTOMLEFT:
      cursor = CursorType::kSouthWestResize;
      break;
    case HTBOTTOMRIGHT:
      cursor = CursorType::kSouthEastResize;
      break;
    default:
      break;
  }
  widget_->SetCursor(ui::Cursor(cursor));
}

void TmaFrameView::Layout(PassKey) {
  LayoutCaptionButtons();
  LayoutSuperclass<views::FrameView>(this);
}

gfx::Size TmaFrameView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetWindowBoundsForClientBounds(
                                gfx::Rect(widget_->client_view()
                                              ->GetPreferredSize(available_size)))
      .size();
}

gfx::Size TmaFrameView::GetMinimumSize() const {
  return GetWindowBoundsForClientBounds(
             gfx::Rect(widget_->client_view()->GetMinimumSize()))
      .size();
}

gfx::Size TmaFrameView::GetMaximumSize() const {
  const gfx::Size client_max = widget_->client_view()->GetMaximumSize();
  const gfx::Size converted =
      GetWindowBoundsForClientBounds(gfx::Rect(client_max)).size();
  return gfx::Size(client_max.width() == 0 ? 0 : converted.width(),
                   client_max.height() == 0 ? 0 : converted.height());
}

void TmaFrameView::LayoutCaptionButtons() {
  const bool show_controls = !widget_->IsFullscreen();
  const bool maximized = widget_->IsMaximized();

  minimize_button_->SetVisible(show_controls);
  maximize_button_->SetVisible(show_controls && !maximized);
  restore_button_->SetVisible(show_controls && maximized);
  close_button_->SetVisible(show_controls);

  if (!show_controls || bounds().IsEmpty()) {
    return;
  }

  const int y = FrameBorderThickness();

  // Centred, right to left: close, maximize/restore, minimize. Exactly three
  // controls are visible at any moment (maximize and restore share a slot).
  //
  // They used to sit flush against the right edge, which made the whole
  // top-right corner unusable for resizing: NonClientHitTest() tests the
  // buttons before ResizeComponentAt(), so the 8px right-edge grip and the
  // 16px corner square inside the close button both answered HTCLOSE. Keeping
  // the group in the middle of the strip leaves both edges and all four
  // corners to the resize grip.
  const int group_width = 3 * kCaptionButtonWidth;
  const int left = std::max(0, (width() - group_width) / 2);

  minimize_button_->SetBounds(left, y, kCaptionButtonWidth, kTitleBarHeight);
  const int toggle_x = left + kCaptionButtonWidth;
  maximize_button_->SetBounds(toggle_x, y, kCaptionButtonWidth,
                              kTitleBarHeight);
  restore_button_->SetBounds(toggle_x, y, kCaptionButtonWidth, kTitleBarHeight);
  close_button_->SetBounds(left + 2 * kCaptionButtonWidth, y,
                           kCaptionButtonWidth, kTitleBarHeight);
}

SkColor TmaFrameView::FrameColor() const {
  return kFrameColor;
}

SkColor TmaFrameView::CaptionForegroundColor() const {
  return kCaptionForeground;
}

BEGIN_METADATA(TmaFrameView)
END_METADATA

}  // namespace tma
