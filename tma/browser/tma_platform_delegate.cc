// Owner: Time And Time Studio
// Date: 2026-10-09 08:50 +0700
// License: GPL-3.0-or-later

#include "tma/browser/tma_platform_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "tma/browser/tma_view.h"
#include "tma/browser/tma_widget_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/wm_state.h"

namespace tma {

struct TmaPlatformDelegate::TmaPlatformData {
  std::unique_ptr<wm::WMState> wm_state;
  std::unique_ptr<display::Screen> screen;
  std::unique_ptr<views::ViewsDelegate> views_delegate;
};

namespace {

constexpr char kFullscreenSwitch[] = "fullscreen";

TmaView* TmaViewForWidget(views::Widget* widget) {
  return static_cast<TmaView*>(widget->widget_delegate()->GetContentsView());
}

}  // namespace

TmaPlatformDelegate::TmaPlatformDelegate() = default;

TmaPlatformDelegate::~TmaPlatformDelegate() = default;

void TmaPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  platform_ = std::make_unique<TmaPlatformData>();
  platform_->wm_state = std::make_unique<wm::WMState>();
  if (!display::Screen::HasScreen()) {
    platform_->screen = views::CreateDesktopScreen();
  }
  platform_->views_delegate =
      std::make_unique<views::DesktopTestViewsDelegate>();
}

void TmaPlatformDelegate::CreatePlatformWindow(
    content::Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!shell_data_map_.contains(shell));
  TmaShellData& shell_data = shell_data_map_[shell];
  shell_data.content_size = initial_size;

  auto widget_delegate = std::make_unique<TmaWidgetDelegate>();
  widget_delegate->SetContentsView(std::make_unique<TmaView>(shell));
  // Enables resize / maximize / minimize / fullscreen. NativeWidgetAura reads
  // these from the delegate during InitNativeWidget() and publishes them as the
  // window's kResizeBehavior bits, which is what makes WindowEventFilterLinux
  // honour the HTCAPTION / HTLEFT / ... results produced by TmaFrameView
  // (caption drag, border resize, double-click-to-maximize).
  widget_delegate->SetHasWindowSizeControls(true);
  widget_delegate->SetTitle(u"TMA");
  // Deliberately not calling SetOwnedByWidget(): that pass key is only
  // constructible by friends of views::WidgetDelegate. TmaShellData owns the
  // delegate instead.
  shell_data.widget_delegate = std::move(widget_delegate);

  shell_data.window_widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.bounds = gfx::Rect(initial_size);
  // TMA draws its own top bar, so the platform must not decorate the window.
  params.remove_standard_frame = true;
  params.delegate = shell_data.widget_delegate.get();
  params.wm_class_class = "tma";
  params.wm_class_name = params.wm_class_class;
  params.wayland_app_id = "tma";

  shell_data.window_widget->Init(std::move(params));
}

gfx::NativeWindow TmaPlatformDelegate::GetNativeWindow(content::Shell* shell) {
  return GetShellData(shell).window_widget->GetNativeWindow();
}

void TmaPlatformDelegate::CleanUp(content::Shell* shell) {
  // Called from ~Shell(), i.e. while CloseNow() is still unwinding; the entry
  // must be dropped here so that DestroyShell() cannot touch it afterwards.
  // Dropping it also releases the WidgetDelegate, which is safe at this point
  // because the Widget is already inside Widget::DestroyRootView() -- the same
  // point in teardown at which content shell's own delegate is gone.
  DCHECK(shell_data_map_.contains(shell));
  shell_data_map_.erase(shell);
}

void TmaPlatformDelegate::SetContents(content::Shell* shell) {
  TmaShellData& shell_data = GetShellData(shell);
  views::Widget* widget = shell_data.window_widget;

  TmaViewForWidget(widget)->SetWebContents(shell->web_contents(),
                                           shell_data.content_size);

  const SkColor background =
      widget->GetColorProvider()->GetColor(ui::kColorWindowBackground);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      shell->web_contents(), background);

  widget->GetNativeWindow()->GetHost()->Show();
  widget->Show();

  shell_data.fullscreen_observer =
      std::make_unique<TmaFullscreenObserver>(shell->web_contents(), widget);

  // views::Widget::Init() only honours kMaximized and kMinimized out of
  // params.show_state (ui/views/widget/widget.cc); kFullscreen is dropped on
  // the floor there, so the switch has to be requested explicitly once the
  // native window exists.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kFullscreenSwitch)) {
    SetNativeFullscreen(widget, true);
  }
}

void TmaPlatformDelegate::ResizeWebContent(content::Shell* shell,
                                           const gfx::Size& content_size) {
  shell->web_contents()->Resize(gfx::Rect(content_size));
}

void TmaPlatformDelegate::EnableUIControl(content::Shell* shell,
                                          UIControl control,
                                          bool is_enabled) {
  // TMA has no toolbar: there is nothing to enable or disable.
}

void TmaPlatformDelegate::SetAddressBarURL(content::Shell* shell,
                                           const GURL& url) {
  // TMA has no address bar.
}

void TmaPlatformDelegate::SetIsLoading(content::Shell* shell, bool loading) {}

void TmaPlatformDelegate::SetTitle(content::Shell* shell,
                                   const std::u16string& title) {
  // Updates the native window title (task switcher / Alt-Tab). The visible
  // caption text stays "TMA" by design; see TmaFrameView.
  GetShellData(shell).window_widget->widget_delegate()->SetTitle(title);
}

bool TmaPlatformDelegate::DestroyShell(content::Shell* shell) {
  TmaShellData& shell_data = GetShellData(shell);
  // CloseNow() destroys the widget, which destroys TmaView, which destroys the
  // Shell, which calls CleanUp() and erases |shell_data|. Do not touch
  // |shell_data| after this call. The observer holds a pointer to the widget,
  // so it goes first.
  shell_data.fullscreen_observer.reset();
  shell_data.window_widget->CloseNow();
  return true;
}

TmaPlatformDelegate::TmaShellData& TmaPlatformDelegate::GetShellData(
    content::Shell* shell) {
  DCHECK(shell_data_map_.contains(shell));
  return shell_data_map_.find(shell)->second;
}

}  // namespace tma
