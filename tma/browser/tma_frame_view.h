// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_FRAME_VIEW_H_
#define TMA_BROWSER_TMA_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "tma/browser/tma_caption_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/frame_view.h"

namespace tma {

// The client-side window frame TMA draws around web content.
//
// It replaces views::DefaultFrameView and renders:
//   * a solid frame background,
//   * minimize / maximize-restore / close buttons, centred in the strip,
// and it answers NonClientHitTest() so that WindowEventFilterLinux turns the
// borders into resize zones and the caption into a drag region. There is no
// caption text: the strip is chrome for the three controls only, and TMA sets
// no window title either (see TmaPlatformDelegate), so there is nothing for
// the compositor to show on this window's behalf.
class TmaFrameView : public views::FrameView {
  METADATA_HEADER(TmaFrameView, views::FrameView)

 public:
  explicit TmaFrameView(views::Widget* widget);
  TmaFrameView(const TmaFrameView&) = delete;
  TmaFrameView& operator=(const TmaFrameView&) = delete;
  ~TmaFrameView() override;

  // views::FrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void SizeConstraintsChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 private:
  // Thickness of the restored window border; zero while maximized/fullscreen.
  int FrameBorderThickness() const;

  // Height of the whole top strip (border + title bar). Zero when fullscreen.
  int TopBorderHeight() const;

  // Returns a HT* resize code for |point| when it falls inside one of the
  // window's resize grips, HTNOWHERE otherwise. Answered after the caption
  // buttons -- the controls are full title-bar height, so the top-edge grip
  // would otherwise swallow their top few DIP -- but before the client view:
  // with kFrameBorderThickness == 0 the client view covers the whole window
  // perimeter, so asking it first would report HTCLIENT for every edge and
  // corner and leave the window unresizable.
  int ResizeComponentAt(const gfx::Point& point) const;

  void LayoutCaptionButtons();

  SkColor FrameColor() const;
  SkColor CaptionForegroundColor() const;

  void UpdateCursorForLocation(const gfx::Point& point);

  const raw_ptr<views::Widget> widget_;

  raw_ptr<TmaCaptionButton> minimize_button_ = nullptr;
  raw_ptr<TmaCaptionButton> maximize_button_ = nullptr;
  raw_ptr<TmaCaptionButton> restore_button_ = nullptr;
  raw_ptr<TmaCaptionButton> close_button_ = nullptr;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_FRAME_VIEW_H_
