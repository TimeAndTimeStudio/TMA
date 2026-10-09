// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tma/browser/tma_fullscreen.h"

#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace tma {

void SetNativeFullscreen(views::Widget* widget, bool fullscreen) {
  if (!widget || widget->IsFullscreen() == fullscreen) {
    return;
  }
  widget->SetFullscreen(fullscreen);
  // Fullscreen changes the frame geometry: border thickness, title strip
  // height and caption button visibility. Re-run that bookkeeping even when
  // the widget size happens not to change across the transition.
  if (views::NonClientView* non_client_view = widget->non_client_view()) {
    non_client_view->SizeConstraintsChanged();
    // views::View dropped SetNeedsLayout() in favour of InvalidateLayout(),
    // which is the documented way "to cause a view to be laid out".
    non_client_view->InvalidateLayout();
    non_client_view->SchedulePaint();
  }
}

TmaFullscreenObserver::TmaFullscreenObserver(content::WebContents* web_contents,
                                             views::Widget* widget)
    : content::WebContentsObserver(web_contents), widget_(widget) {}

TmaFullscreenObserver::~TmaFullscreenObserver() = default;

void TmaFullscreenObserver::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  SetNativeFullscreen(widget_, entered_fullscreen);
}

TmaFullscreenKeyHandler::TmaFullscreenKeyHandler(views::Widget* widget)
    : window_(widget->GetNativeWindow()), widget_(widget) {
  CHECK(window_);
  window_->AddObserver(this);
  window_->AddPreTargetHandler(this);
}

TmaFullscreenKeyHandler::~TmaFullscreenKeyHandler() {
  Unregister();
}

void TmaFullscreenKeyHandler::OnWindowDestroying(aura::Window* /*window*/) {
  Unregister();
}

void TmaFullscreenKeyHandler::Unregister() {
  if (!window_) {
    return;
  }
  window_->RemovePreTargetHandler(this);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void TmaFullscreenKeyHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed ||
      event->key_code() != ui::VKEY_F11) {
    return;
  }
  if (!widget_) {
    return;
  }
  SetNativeFullscreen(widget_.get(), !widget_->IsFullscreen());
  // Stop here so the renderer never sees the key either. Stopping in the
  // pre-target phase skips target dispatch entirely, which also means the
  // views accelerator path (FocusManagerEventHandler, registered on the
  // top-level window and therefore run before this handler) is the only other
  // place that can act on F11, and only when it has a focused view to work
  // with -- never both.
  event->StopPropagation();
}

}  // namespace tma
