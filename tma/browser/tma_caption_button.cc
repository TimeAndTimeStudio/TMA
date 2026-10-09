// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tma/browser/tma_caption_button.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace tma {

namespace {

// Outer size of the square glyphs (maximize / restore).
constexpr int kGlyphSize = 10;

// Stroke thickness of the square glyphs.
constexpr int kGlyphThickness = 2;

// Length of the minimize bar.
constexpr int kMinBarLength = 10;

// Length of each half of the close "X".
constexpr int kCloseHalfSize = 5;

// How far the back square of the restore glyph is offset, in DIP.
constexpr int kRestoreOffset = 3;

// Background blends, out of 255, applied toward white on dark frames and
// toward black on light frames.
constexpr uint8_t kHoverBlend = 30;
constexpr uint8_t kPressedBlend = 70;

// Approximate relative luminance: (0.2126*76 + 0.7152*171 + 0.0722*29)/256.
bool IsDark(SkColor color) {
  const int luminance = (54 * SkColorGetR(color) + 183 * SkColorGetG(color) +
                         19 * SkColorGetB(color)) >>
                        8;
  return luminance < 128;
}

uint8_t LerpU8(uint8_t from, uint8_t to, uint8_t amount) {
  const int interpolated = static_cast<int>(from) * (255 - amount) +
                           static_cast<int>(to) * amount;
  return static_cast<uint8_t>(interpolated / 255);
}

SkColor Blend(SkColor from, SkColor to, uint8_t amount) {
  return SkColorSetARGB(LerpU8(SkColorGetA(from), SkColorGetA(to), amount),
                        LerpU8(SkColorGetR(from), SkColorGetR(to), amount),
                        LerpU8(SkColorGetG(from), SkColorGetG(to), amount),
                        LerpU8(SkColorGetB(from), SkColorGetB(to), amount));
}

std::u16string_view LabelForType(TmaCaptionButton::Type type) {
  switch (type) {
    case TmaCaptionButton::Type::kMinimize:
      return u"Minimize";
    case TmaCaptionButton::Type::kMaximize:
      return u"Maximize";
    case TmaCaptionButton::Type::kRestore:
      return u"Restore";
    case TmaCaptionButton::Type::kClose:
      return u"Close";
  }
  // Unreachable: every enumerator is handled above. Kept so that -Wreturn-type
  // stays quiet whatever the compiler decides about exhaustive switches.
  return u"Button";
}

}  // namespace

TmaCaptionButton::TmaCaptionButton(Type type, base::RepeatingClosure callback)
    : type_(type), callback_(std::move(callback)) {
  // No SetTooltipText(): the hover bubble that spelled out "Minimize" /
  // "Maximize" / "Close" over the centre of the strip was pure noise for
  // universally known glyphs. The name is still published to accessibility so
  // screen readers keep announcing what the control does.
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(std::u16string(LabelForType(type_)));
}

TmaCaptionButton::~TmaCaptionButton() = default;

void TmaCaptionButton::SetCaptionColors(SkColor frame_color,
                                        SkColor glyph_color) {
  if (frame_color == frame_color_ && glyph_color == glyph_color_) {
    return;
  }
  frame_color_ = frame_color;
  glyph_color_ = glyph_color;
  const SkColor toward = IsDark(frame_color) ? SK_ColorWHITE : SK_ColorBLACK;
  hover_color_ = Blend(frame_color, toward, kHoverBlend);
  pressed_color_ = Blend(frame_color, toward, kPressedBlend);
  // The frame paints its children's colours from inside its own OnPaint(), so
  // without this the button keeps whatever pixels it last drew until something
  // else happens to repaint it -- i.e. the buttons show the previous frame
  // colour while the strip around them already shows the new one.
  SchedulePaint();
}

SkColor TmaCaptionButton::CurrentBackgroundColor() const {
  if (pressed_) {
    return pressed_color_;
  }
  if (hovered_) {
    return hover_color_;
  }
  return frame_color_;
}

void TmaCaptionButton::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), CurrentBackgroundColor());
  DrawGlyph(canvas);
}

void TmaCaptionButton::DrawGlyph(gfx::Canvas* canvas) const {
  const int w = width();
  const int h = height();
  const SkColor color = glyph_color_;

  const auto draw_square = [&](const gfx::Rect& rect) {
    canvas->FillRect(
        gfx::Rect(rect.x(), rect.y(), rect.width(), kGlyphThickness), color);
    canvas->FillRect(
        gfx::Rect(rect.x(), rect.bottom() - kGlyphThickness, rect.width(),
                  kGlyphThickness),
        color);
    canvas->FillRect(gfx::Rect(rect.x(), rect.y() + kGlyphThickness,
                               kGlyphThickness,
                               rect.height() - 2 * kGlyphThickness),
                     color);
    canvas->FillRect(
        gfx::Rect(rect.right() - kGlyphThickness, rect.y() + kGlyphThickness,
                  kGlyphThickness, rect.height() - 2 * kGlyphThickness),
        color);
  };

  switch (type_) {
    case Type::kMinimize: {
      canvas->FillRect(gfx::Rect((w - kMinBarLength) / 2,
                                 (h - kGlyphThickness) / 2, kMinBarLength,
                                 kGlyphThickness),
                       color);
      break;
    }
    case Type::kMaximize: {
      draw_square(gfx::Rect((w - kGlyphSize) / 2, (h - kGlyphSize) / 2,
                            kGlyphSize, kGlyphSize));
      break;
    }
    case Type::kRestore: {
      const int x = (w - kGlyphSize) / 2;
      const int y = (h - kGlyphSize) / 2;
      // Back square, up and to the right. Its bottom left corner is hidden
      // behind the front square, so knock that area out with the button
      // background before drawing the front square.
      draw_square(gfx::Rect(x + kRestoreOffset, y - kRestoreOffset, kGlyphSize,
                            kGlyphSize));
      canvas->FillRect(gfx::Rect(x, y, kGlyphSize, kGlyphSize),
                       CurrentBackgroundColor());
      draw_square(gfx::Rect(x, y, kGlyphSize, kGlyphSize));
      break;
    }
    case Type::kClose: {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(color);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      const float cx = w / 2.0f;
      const float cy = h / 2.0f;
      const float half = kCloseHalfSize;
      canvas->DrawLine(gfx::PointF(cx - half, cy - half),
                       gfx::PointF(cx + half, cy + half), flags);
      canvas->DrawLine(gfx::PointF(cx + half, cy - half),
                       gfx::PointF(cx - half, cy + half), flags);
      break;
    }
  }
}

void TmaCaptionButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (hovered_) {
    return;
  }
  hovered_ = true;
  SchedulePaint();
}

void TmaCaptionButton::OnMouseExited(const ui::MouseEvent& event) {
  if (!hovered_) {
    return;
  }
  hovered_ = false;
  SchedulePaint();
}

bool TmaCaptionButton::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return false;
  }
  pressed_ = true;
  if (views::Widget* widget = GetWidget()) {
    widget->SetCapture(this);
  }
  SchedulePaint();
  return true;
}

void TmaCaptionButton::OnMouseReleased(const ui::MouseEvent& event) {
  const bool was_pressed = pressed_;
  if (views::Widget* widget = GetWidget()) {
    if (widget->HasCapture()) {
      widget->ReleaseCapture();
    }
  }
  pressed_ = false;
  SchedulePaint();
  if (was_pressed && event.IsOnlyLeftMouseButton() &&
      GetLocalBounds().Contains(event.location())) {
    callback_.Run();
  }
}

void TmaCaptionButton::OnMouseCaptureLost() {
  if (!pressed_) {
    return;
  }
  pressed_ = false;
  SchedulePaint();
}

BEGIN_METADATA(TmaCaptionButton)
END_METADATA

}  // namespace tma
