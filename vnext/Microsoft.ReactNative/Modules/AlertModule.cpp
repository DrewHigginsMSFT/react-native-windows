// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "AlertModule.h"
#include "Unicode.h"

#include <UI.Xaml.Controls.Primitives.h>
#include <UI.Xaml.Controls.h>
#include <UI.Xaml.Media.h>
#include <UI.Xaml.Shapes.h>
#include <Utils/ValueUtils.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include "Utils/Helpers.h"

namespace Microsoft::ReactNative {

void Alert::showAlert(
    DialogOptions &&args,
    std::function<void(std::string)> /*error*/,
    std::function<void(std::string, int)> result) noexcept {
  m_context.UIDispatcher().Post([weakThis = weak_from_this(), args = std::move(args), result]() mutable {
    if (auto strongThis = weakThis.lock()) {
      strongThis->pendingAlerts.emplace(std::move(args), result);
      if (strongThis->pendingAlerts.size() == 1) {
        strongThis->ProcessPendingAlertRequests();
      }
    }
  });
}

void Alert::ProcessPendingAlertRequests() noexcept {
  if (pendingAlerts.empty())
    return;
  const auto &[args, result] = pendingAlerts.front();
  auto jsDispatcher = m_context.JSDispatcher();

  xaml::Controls::ContentDialog dialog{};
  dialog.Title(winrt::box_value(Microsoft::Common::Unicode::Utf8ToUtf16(args.title.value_or(std::string{}))));
  dialog.Content(winrt::box_value(Microsoft::Common::Unicode::Utf8ToUtf16(args.message.value_or(std::string{}))));
  dialog.PrimaryButtonText(Microsoft::Common::Unicode::Utf8ToUtf16(args.buttonPositive.value_or(std::string{})));
  dialog.SecondaryButtonText(Microsoft::Common::Unicode::Utf8ToUtf16(args.buttonNegative.value_or(std::string{})));
  dialog.CloseButtonText(Microsoft::Common::Unicode::Utf8ToUtf16(args.buttonNeutral.value_or(std::string{})));
  int defaultButton = args.defaultButton.value_or(0);
  if (defaultButton >= 0 && defaultButton <= 3) {
    dialog.DefaultButton(static_cast<xaml::Controls::ContentDialogButton>(defaultButton));
  }

  auto useXamlRootForThemeBugWorkaround = false;

  if (Is19H1OrHigher()) {
    // XamlRoot added in 19H1
    if (const auto xamlRoot = React::XamlUIService::GetXamlRoot(m_context.Properties().Handle())) {
      useXamlRootForThemeBugWorkaround = true;
      dialog.XamlRoot(xamlRoot);
      auto rootChangedToken = xamlRoot.Changed([=](auto &&, auto &&) {
        const auto rootSize = xamlRoot.Size();
        const auto popupRoot = xaml::Media::VisualTreeHelper::GetParent(dialog);
        const auto nChildren = xaml::Media::VisualTreeHelper::GetChildrenCount(popupRoot);
        if (nChildren == 2) {
          // The first is supposed to be the smoke screen (Rectangle), and the second the content dialog
          if (const auto smoke =
                  xaml::Media::VisualTreeHelper::GetChild(popupRoot, 0).try_as<xaml::Shapes::Rectangle>()) {
            const auto assertDialog =
                xaml::Media::VisualTreeHelper::GetChild(popupRoot, 1).try_as<xaml::Controls::ContentDialog>();
            if (assertDialog == dialog) {
              smoke.Width(rootSize.Width);
              smoke.Height(rootSize.Height);
              dialog.Width(rootSize.Width);
              dialog.Height(rootSize.Height);
            }
          }
        }
      });

      dialog.Closed([=](auto &&, auto &&) { xamlRoot.Changed(rootChangedToken); });
    }
  }

  // Workaround XAML bug with ContentDialog and dark theme:
  // https://github.com/microsoft/microsoft-ui-xaml/issues/2331
  dialog.Opened([useXamlRootForThemeBugWorkaround](winrt::IInspectable const &sender, auto &&) {
    auto contentDialog = sender.as<xaml::Controls::ContentDialog>();
    auto popups = xaml::Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(contentDialog.XamlRoot());

    auto contentAsFrameworkElement = useXamlRootForThemeBugWorkaround
        ? contentDialog.XamlRoot().Content().try_as<xaml::FrameworkElement>()
        : xaml::Window::Current().Content().try_as<xaml::FrameworkElement>();
    if (contentAsFrameworkElement) {
      for (uint32_t i = 0; i < popups.Size(); i++) {
        popups.GetAt(i).RequestedTheme(contentAsFrameworkElement.ActualTheme());
      }
    }
  });

  auto asyncOp = dialog.ShowAsync();
  asyncOp.Completed(
      [jsDispatcher, result, this](
          const winrt::IAsyncOperation<xaml::Controls::ContentDialogResult> &asyncOp, winrt::AsyncStatus status) {
        switch (asyncOp.GetResults()) {
          case xaml::Controls::ContentDialogResult::Primary:
            jsDispatcher.Post([result, this] { result(m_constants.buttonClicked, m_constants.buttonPositive); });
            break;
          case xaml::Controls::ContentDialogResult::Secondary:
            jsDispatcher.Post([result, this] { result(m_constants.buttonClicked, m_constants.buttonNegative); });
            break;
          case xaml::Controls::ContentDialogResult::None:
            jsDispatcher.Post([result, this] { result(m_constants.buttonClicked, m_constants.buttonNeutral); });
            break;
          default:
            break;
        }
        pendingAlerts.pop();
        ProcessPendingAlertRequests();
      });
}

Alert::Constants Alert::GetConstants() noexcept {
  return m_constants;
}

void Alert::Initialize(React::ReactContext const &reactContext) noexcept {
  m_context = reactContext;
  m_constants.buttonClicked = "buttonClicked";
  m_constants.dismissed = "dismissed";
  m_constants.buttonPositive = -1;
  m_constants.buttonNegative = -2;
  m_constants.buttonNeutral = -3;
}

} // namespace Microsoft::ReactNative
