// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_METRICS_H_
#define TMA_BROWSER_TMA_METRICS_H_

// Geometry constants for the TMA client-side frame. Values are in DIP.
namespace tma {

// Height of the title strip itself. The frame has no visible border any more
// (see kFrameBorderThickness), so this is also the whole top chrome.
inline constexpr int kTitleBarHeight = 28;

// Thickness of the window border drawn around the client area. Always zero:
// TMA's frame is edge to edge, so no ring of frame colour shows between the
// title strip and the screen edge. Resize still works because
// kResizeHitThickness carves an invisible grip out of the window perimeter.
inline constexpr int kFrameBorderThickness = 0;

// Invisible resize grip. Points within this many DIP of the window edge report
// a resize component from TmaFrameView::NonClientHitTest() instead of letting
// the click fall through to the web content, which is what replaces the removed
// border as a way to resize the window. Wide enough that the grip can actually
// be found by mouse without a visual cue.
inline constexpr int kResizeHitThickness = 8;

// Size of the square resize hot zone in each window corner. A corner reports a
// corner component anywhere inside this square, not just inside the
// kResizeHitThickness ring, so the very corner of the window is grabbable.
inline constexpr int kResizeAreaCornerSize = 16;

// Caption button size. The three visible controls are centred as a group in
// the strip rather than flush with the right edge, so that the right edge and
// the top-right corner stay available to kResizeHitThickness.
inline constexpr int kCaptionButtonWidth = 34;

// Smallest allowed client (web content) area.
inline constexpr int kMinWindowWidth = 320;
inline constexpr int kMinWindowHeight = 200;

// Default window size, overridable with --window-size=<width>x<height>.
inline constexpr int kDefaultWindowWidth = 1280;
inline constexpr int kDefaultWindowHeight = 800;

}  // namespace tma

#endif  // TMA_BROWSER_TMA_METRICS_H_
