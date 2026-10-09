// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#ifndef TMA_BROWSER_TMA_PLATFORM_DELEGATE_H_
#define TMA_BROWSER_TMA_PLATFORM_DELEGATE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/shell/browser/shell_platform_delegate.h"
#include "tma/browser/tma_fullscreen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"

class GURL;

namespace views {
class Widget;
class WidgetDelegate;
}  // namespace views

namespace tma {

// Owns the TMA top-level widgets and the process wide UI state (wm state,
// screen, views delegate), mirroring content::ShellPlatformDelegate but with a
// client-side frame drawn by tma::TmaFrameView.
//
// The base class keeps its per-Shell map private, so TMA carries its own map
// instead of trying to reuse the base one.
class TmaPlatformDelegate : public content::ShellPlatformDelegate {
 public:
  TmaPlatformDelegate();
  TmaPlatformDelegate(const TmaPlatformDelegate&) = delete;
  TmaPlatformDelegate& operator=(const TmaPlatformDelegate&) = delete;
  ~TmaPlatformDelegate() override;

  // content::ShellPlatformDelegate:
  void Initialize(const gfx::Size& default_window_size) override;
  void CreatePlatformWindow(content::Shell* shell,
                            const gfx::Size& initial_size) override;
  gfx::NativeWindow GetNativeWindow(content::Shell* shell) override;
  void CleanUp(content::Shell* shell) override;
  void SetContents(content::Shell* shell) override;
  void ResizeWebContent(content::Shell* shell,
                        const gfx::Size& content_size) override;
  void EnableUIControl(content::Shell* shell,
                       UIControl control,
                       bool is_enabled) override;
  void SetAddressBarURL(content::Shell* shell, const GURL& url) override;
  void SetIsLoading(content::Shell* shell, bool loading) override;
  void SetTitle(content::Shell* shell, const std::u16string& title) override;
  bool DestroyShell(content::Shell* shell) override;

 private:
  // Per-window state. TMA has one Shell per window, like content shell.
  struct TmaShellData {
    gfx::Size content_size;

    // Owned here rather than through WidgetDelegate::SetOwnedByWidget(),
    // whose pass key is restricted to friends of views::WidgetDelegate. The
    // only requirement is that the delegate outlives its Widget, and this
    // entry is dropped by CleanUp() while the window is being torn down.
    std::unique_ptr<views::WidgetDelegate> widget_delegate;

    // Self-owned Widget, destroyed through CloseNow().
    raw_ptr<views::Widget> window_widget = nullptr;

    // Mirrors the renderer's HTML5 fullscreen onto |window_widget|; see
    // tma_fullscreen.h. Dropped in DestroyShell() before CloseNow() so that
    // it never outlives the widget it points at.
    std::unique_ptr<TmaFullscreenObserver> fullscreen_observer;
  };

  // Process-wide state, created in Initialize().
  struct TmaPlatformData;

  TmaShellData& GetShellData(content::Shell* shell);

  base::flat_map<content::Shell*, TmaShellData> shell_data_map_;
  std::unique_ptr<TmaPlatformData> platform_;
};

}  // namespace tma

#endif  // TMA_BROWSER_TMA_PLATFORM_DELEGATE_H_
