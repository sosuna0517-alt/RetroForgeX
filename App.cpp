#include "pch.h"

#include "App.h"
#include "MainPage.h"

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace RetroForgeX;
using namespace RetroForgeX::implementation;

App::App()
{
    // Xbox: keep XAML from showing the mouse cursor / focus visuals.
    // Input goes straight to Windows.Gaming.Input and Dear ImGui.
    RequiresPointerMode(ApplicationRequiresPointerMode::WhenRequested);
    FocusVisualKind(FocusVisualKind::Reveal);

    Suspending({ this, &App::OnSuspending });
    Resuming({ this, &App::OnResuming });

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
    UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
    {
        if (IsDebuggerPresent())
        {
            auto errorMessage = e.Message();
            __debugbreak();
        }
    });
#endif
}

void App::OnLaunched(LaunchActivatedEventArgs const& e)
{
    // Draw edge-to-edge (no TV-safe-area padding); the UI applies its own safe margins.
    Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().SetDesiredBoundsMode(
        Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);

    Frame rootFrame{ nullptr };
    if (auto content = Window::Current().Content())
    {
        rootFrame = content.try_as<Frame>();
    }

    if (rootFrame == nullptr)
    {
        rootFrame = Frame();
        rootFrame.NavigationFailed({ this, &App::OnNavigationFailed });
        Window::Current().Content(rootFrame);
    }

    if (e.PrelaunchActivated() == false)
    {
        if (rootFrame.Content() == nullptr)
        {
            rootFrame.Navigate(xaml_typename<RetroForgeX::MainPage>(), box_value(e.Arguments()));
        }
        Window::Current().Activate();
    }
}

void App::OnSuspending([[maybe_unused]] IInspectable const& sender, SuspendingEventArgs const& e)
{
    auto deferral = e.SuspendingOperation().GetDeferral();
    if (auto frame = Window::Current().Content().try_as<Frame>())
    {
        if (auto page = frame.Content().try_as<RetroForgeX::MainPage>())
        {
            get_self<implementation::MainPage>(page)->OnSuspending();
        }
    }
    deferral.Complete();
}

void App::OnResuming(IInspectable const&, IInspectable const&)
{
    if (auto frame = Window::Current().Content().try_as<Frame>())
    {
        if (auto page = frame.Content().try_as<RetroForgeX::MainPage>())
        {
            get_self<implementation::MainPage>(page)->OnResuming();
        }
    }
}

void App::OnNavigationFailed(IInspectable const&, NavigationFailedEventArgs const& e)
{
    throw hresult_error(E_FAIL, hstring(L"Failed to load Page ") + e.SourcePageType().Name);
}
