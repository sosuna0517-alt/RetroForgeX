#pragma once
// DeviceResources: owns the D3D11 device/context and the composition swap
// chain attached to the XAML SwapChainPanel via ISwapChainPanelNative.
//
// Threading:
//   - CreateDeviceAndSwapChain / AttachToPanel: UI thread (SetSwapChain must
//     be called on the XAML thread).
//   - SetPendingMetrics: UI thread (SizeChanged / CompositionScaleChanged).
//   - Everything else: render thread.

namespace rfx
{
    struct PanelMetrics
    {
        float dipWidth = 0.0f;
        float dipHeight = 0.0f;
        float compositionScaleX = 1.0f;
        float compositionScaleY = 1.0f;
        // Physical output mode if known (Xbox HDMI mode). 0 = derive from panel.
        uint32_t outputWidth = 0;
        uint32_t outputHeight = 0;
    };

    class DeviceResources
    {
    public:
        void CreateDeviceAndSwapChain(winrt::Windows::UI::Xaml::Controls::SwapChainPanel const& panel);

        void SetPendingMetrics(PanelMetrics const& metrics);
        // Render thread: applies a pending resize. Returns true if the back buffer changed.
        bool ApplyPendingResize();

        void Present();
        void Trim();

        ID3D11Device1* Device() const { return m_device.get(); }
        ID3D11DeviceContext1* Context() const { return m_context.get(); }
        ID3D11RenderTargetView* BackBufferRtv() const { return m_backBufferRtv.get(); }
        D3D_FEATURE_LEVEL FeatureLevel() const { return m_featureLevel; }
        uint32_t Width() const { return m_width; }
        uint32_t Height() const { return m_height; }

    private:
        void CreateBackBufferViews();
        void ApplyMatrixTransform();
        static void ComputeTargetSize(PanelMetrics const& m, uint32_t& w, uint32_t& h);

        winrt::com_ptr<ID3D11Device1> m_device;
        winrt::com_ptr<ID3D11DeviceContext1> m_context;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;
        winrt::com_ptr<ID3D11RenderTargetView> m_backBufferRtv;
        D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        PanelMetrics m_metrics{};

        std::mutex m_pendingMutex;
        std::optional<PanelMetrics> m_pending;
    };
}
