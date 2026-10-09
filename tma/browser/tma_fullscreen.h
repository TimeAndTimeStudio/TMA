// Copyright 2026 The TMA Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TMA_BROWSER_TMA_FULLSCREEN_H_
#define TMA_BROWSER_TMA_FULLSCREEN_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

namespace tma {

// Puts |widget| into or out of native fullscreen and re-runs the frame
// bookkeeping that the geometry change requires. Shared by the F11 key
// handler and the observer below, which must behave identically.
void SetNativeFullscreen(views::Widget* widget, bool fullscreen);

// Maps content shell's renderer-initiated (HTML5) fullscreen onto a real
// native fullscreen of the TMA window.
//
// content::Shell::ToggleFullscreenModeForTab() only forwards to the platform
// delegate under IS_ANDROID || IS_IOS, so on Linux the renderer is told the
// page is fullscreen -- the requestFullscreen() promise resolves and
// document.fullscreenElement is set -- while the window keeps its old bounds
// and nothing on screen changes. Chrome supplies the missing half in
// chrome/browser/ui/exclusive_access; content shell has no equivalent, so TMA
// listens on the WebContents instead. WebContentsImpl::EnterFullscreenMode()
// and ExitFullscreenMode() notify DidToggleFullscreenModeForTab() on every
// platform after the delegate has run, which is exactly the hook needed here
// and needs no edit to content/ itself.
class TmaFullscreenObserver : public content::WebContentsObserver {
 public:
  TmaFullscreenObserver(content::WebContents* web_contents,
                        views::Widget* widget);
  TmaFullscreenObserver(const TmaFullscreenObserver&) = delete;
  TmaFullscreenObserver& operator=(const TmaFullscreenObserver&) = delete;
  ~TmaFullscreenObserver() override;

  // content::WebContentsObserver:
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

 private:
  // Owned by TmaPlatformDelegate::TmaShellData, which drops the observer
  // before destroying the widget in DestroyShell().
  raw_ptr<views::Widget> widget_;
};

// Toggles native fullscreen on F11.
//
// This handler exists because the two obvious hooks are unreliable on their
// own:
//
//   * views::View::AddAccelerator() is dispatched by FocusManagerEventHandler,
//     which returns early when FocusManager::GetFocusedView() is null. Nothing
//     in TMA is a focusable view -- clicking the page hands keyboard focus to
//     the renderer, which never updates views' focused view -- so after the
//     first click the accelerator stops firing.
//   * Content shell has no Linux implementation of
//     WebContentsDelegate::HandleKeyboardEvent() (it is guarded by IS_MAC), so
//     an F11 that reaches the renderer as unhandled is dropped there instead
//     of being handled in the browser.
//
// This handler registers as a pre-target handler on the widget's native
// window instead. ui::EventTarget::GetPreTargetHandlers() walks the parent
// chain, so it sees every key event dispatched anywhere in the window's aura
// subtree -- including events whose target is the renderer's own window --
// and can stop them before the renderer sees them. FocusManagerEventHandler is
// registered on the top-level window as well and runs first; when it does
// handle the key it stops propagation, so only one of the two ever acts on a
// given F11.
class TmaFullscreenKeyHandler : public ui::EventHandler,
                                public aura::WindowObserver {
 public:
  explicit TmaFullscreenKeyHandler(views::Widget* widget);
  TmaFullscreenKeyHandler(const TmaFullscreenKeyHandler&) = delete;
  TmaFullscreenKeyHandler& operator=(const TmaFullscreenKeyHandler&) = delete;
  ~TmaFullscreenKeyHandler() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Unregisters |this| from the window and forgets it. Called from the
  // destructor and from OnWindowDestroying(), whichever happens first, so
  // that neither side can ever reach the other after it is gone.
  void Unregister();

  // The window this handler is registered on. Cleared by OnWindowDestroying().
  // How long that is compared to ~TmaView() depends on which of the widget's
  // teardown paths run first, which is why the handler does not simply keep
  // the pointer for its whole lifetime.
  raw_ptr<aura::Window> window_;
  const raw_ptr<views::Widget> widget_;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_FULLSCREEN_H_
