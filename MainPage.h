#pragma once

#include "MainPage.g.h"
#include "Frontend.h"

namespace winrt::RetroForgeX::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        MainPage();
        ~MainPage();

        void OnSuspending();
        void OnResuming();

    private:
        void OnLoaded(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
        void OnPanelSizeChanged(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::SizeChangedEventArgs const&);
        void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const&, Windows::Foundation::IInspectable const&);
        void PushPanelSize();
        Windows::Foundation::IAsyncAction PickRomFolderAsync();

        std::unique_ptr<::rfx::Frontend> m_frontend;
        winrt::event_token m_backToken{};
        winrt::event_token m_keyToken{};
    };
}

namespace winrt::RetroForgeX::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
