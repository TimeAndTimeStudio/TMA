// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_CAPTION_BUTTON_H_
#define TMA_BROWSER_TMA_CAPTION_BUTTON_H_

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace tma {

// One of the three window controls (minimize, maximize/restore, close) hosted
// by tma::TmaFrameView.
//
// This is deliberately a plain views::View rather than a views::Button: the
// control rectangles are reported by TmaFrameView::NonClientHitTest() as
// HTMINBUTTON / HTMAXBUTTON / HTCLOSE, so they already receive normal views
// mouse events, and a views::Button would pull in ink drop / background
// machinery that TMA does not use.
//
// The button paints its own background and glyph so that it always matches the
// current frame colors; TmaFrameView pushes those colors down right before the
// children are painted.
class TmaCaptionButton : public views::View {
  METADATA_HEADER(TmaCaptionButton, views::View)

 public:
  enum class Type {
    kMinimize,
    kMaximize,
    kRestore,
    kClose,
  };

  TmaCaptionButton(Type type, base::RepeatingClosure callback);
  TmaCaptionButton(const TmaCaptionButton&) = delete;
  TmaCaptionButton& operator=(const TmaCaptionButton&) = delete;
  ~TmaCaptionButton() override;

  Type type() const { return type_; }

  // Updates the colors derived from the current frame color. Does not schedule
  // a paint: TmaFrameView calls this from OnPaint() before children are drawn.
  void SetCaptionColors(SkColor frame_color, SkColor glyph_color);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;

 private:
  SkColor CurrentBackgroundColor() const;
  void DrawGlyph(gfx::Canvas* canvas) const;

  const Type type_;
  base::RepeatingClosure callback_;

  SkColor frame_color_ = SK_ColorTRANSPARENT;
  SkColor glyph_color_ = SK_ColorWHITE;
  SkColor hover_color_ = SK_ColorTRANSPARENT;
  SkColor pressed_color_ = SK_ColorTRANSPARENT;

  bool hovered_ = false;
  bool pressed_ = false;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_CAPTION_BUTTON_H_
