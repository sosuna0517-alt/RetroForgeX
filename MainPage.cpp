#include "pch.h"
#include "MainPage.h"
#if __has_include("MainPage.g.cpp")
#include "MainPage.g.cpp"
#endif

#include "UI/imgui_impl_uwp.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Display::Core;

namespace winrt::RetroForgeX::implementation
{
    MainPage::MainPage()
    {
        // XAML objects must not touch XAML properties before InitializeComponent;
        // the generated base class calls it for us, so hook events in Loaded.
        Loaded({ this, &MainPage::OnLoaded });
    }

    MainPage::~MainPage()
    {
        if (m_frontend)
        {
            m_frontend->Shutdown();
        }
    }

    void MainPage::OnLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_frontend)
        {
            return;
        }

        // The B button on Xbox raises BackRequested; swallow it so it never
        // navigates/exits the app. The dashboard handles B itself.
        m_backToken = SystemNavigationManager::GetForCurrentView().BackRequested(
            [](IInspectable const&, BackRequestedEventArgs const& args) { args.Handled(true); });

        auto window = CoreWindow::GetForCurrentThread();
        ImGui_ImplUwp_AttachWindow(window);

        // Keyboard fallback for testing on PC: F1 or Escape toggles the dashboard.
        m_keyToken = window.KeyDown([this](CoreWindow const&, KeyEventArgs const& args)
        {
            const auto key = args.VirtualKey();
            if ((key == Windows::System::VirtualKey::F1 || key == Windows::System::VirtualKey::Escape) && m_frontend)
            {
                m_frontend->RequestToggleMenu();
            }
        });

        m_frontend = std::make_unique<::rfx::Frontend>();
        m_frontend->SetFolderPickerRequestHandler([weak = get_weak()]()
        {
            if (auto self = weak.get())
            {
                self->Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [weak]()
                {
                    if (auto inner = weak.get())
                    {
                        inner->PickRomFolderAsync();
                    }
                });
            }
        });

        m_frontend->Initialize(swapChainPanel());
        PushPanelSize();

        swapChainPanel().SizeChanged({ this, &MainPage::OnPanelSizeChanged });
        swapChainPanel().CompositionScaleChanged({ this, &MainPage::OnCompositionScaleChanged });

        m_frontend->Start();
    }

    void MainPage::PushPanelSize()
    {
        if (!m_frontend)
        {
            return;
        }

        auto panel = swapChainPanel();
        ::rfx::PanelMetrics metrics{};
        metrics.dipWidth = static_cast<float>(panel.ActualWidth());
        metrics.dipHeight = static_cast<float>(panel.ActualHeight());
        metrics.compositionScaleX = panel.CompositionScaleX();
        metrics.compositionScaleY = panel.CompositionScaleY();

        // On Xbox the HDMI mode is the real output resolution (3840x2160 on a 4K TV),
        // independent of XAML's layout scale. Render the swap chain at that size.
        if (Windows::System::Profile::AnalyticsInfo::VersionInfo().DeviceFamily() == L"Windows.Xbox")
        {
            try
            {
                if (auto hdmi = HdmiDisplayInformation::GetForCurrentView())
                {
                    auto mode = hdmi.GetCurrentDisplayMode();
                    metrics.outputWidth = mode.ResolutionWidthInRawPixels();
                    metrics.outputHeight = mode.ResolutionHeightInRawPixels();
                }
            }
            catch (...)
            {
            }
        }

        m_frontend->OnPanelMetricsChanged(metrics);
    }

    void MainPage::OnPanelSizeChanged(IInspectable const&, SizeChangedEventArgs const&)
    {
        PushPanelSize();
    }

    void MainPage::OnCompositionScaleChanged(SwapChainPanel const&, IInspectable const&)
    {
        PushPanelSize();
    }

    IAsyncAction MainPage::PickRomFolderAsync()
    {
        auto strongThis = get_strong();
        Windows::Storage::Pickers::FolderPicker picker;
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().Append(L"*");

        Windows::Storage::StorageFolder folder{ nullptr };
        try
        {
            folder = co_await picker.PickSingleFolderAsync();
        }
        catch (...)
        {
        }

        if (folder && m_frontend)
        {
            m_frontend->OnRomFolderPicked(folder);
        }
    }

    void MainPage::OnSuspending()
    {
        if (m_frontend)
        {
            m_frontend->Suspend();
        }
    }

    void MainPage::OnResuming()
    {
        if (m_frontend)
        {
            m_frontend->Resume();
        }
    }
}
