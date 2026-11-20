#include "pch.h"
#include "DeviceResources.h"

namespace rfx
{
    namespace
    {
        constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        constexpr UINT kBufferCount = 2;

        bool SdkLayersAvailable()
        {
            HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_NULL, nullptr,
                D3D11_CREATE_DEVICE_DEBUG, nullptr, 0, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
            return SUCCEEDED(hr);
        }
    }

    void DeviceResources::ComputeTargetSize(PanelMetrics const& m, uint32_t& w, uint32_t& h)
    {
        if (m.outputWidth > 0 && m.outputHeight > 0)
        {
            w = m.outputWidth;
            h = m.outputHeight;
        }
        else
        {
            w = static_cast<uint32_t>(std::lround(m.dipWidth * m.compositionScaleX));
            h = static_cast<uint32_t>(std::lround(m.dipHeight * m.compositionScaleY));
        }
        w = std::max<uint32_t>(w, 64);
        h = std::max<uint32_t>(h, 64);
    }

    void DeviceResources::CreateDeviceAndSwapChain(winrt::Windows::UI::Xaml::Controls::SwapChainPanel const& panel)
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        if (SdkLayersAvailable())
        {
            flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
#endif
        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, device.put(), &m_featureLevel, context.put()));

        m_device = device.as<ID3D11Device1>();
        m_context = context.as<ID3D11DeviceContext1>();

        // Initial size: 1920x1080 until the first SizeChanged arrives.
        m_width = 1920;
        m_height = 1080;

        auto dxgiDevice = m_device.as<IDXGIDevice3>();
        winrt::com_ptr<IDXGIAdapter> adapter;
        winrt::check_hresult(dxgiDevice->GetAdapter(adapter.put()));
        winrt::com_ptr<IDXGIFactory2> factory;
        winrt::check_hresult(adapter->GetParent(IID_PPV_ARGS(factory.put())));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.Format = kBackBufferFormat;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = kBufferCount;
        desc.Scaling = DXGI_SCALING_STRETCH;           // required for composition swap chains
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        winrt::check_hresult(factory->CreateSwapChainForComposition(m_device.get(), &desc, nullptr, m_swapChain.put()));

        // Must be called on the XAML (UI) thread.
        auto panelNative = panel.as<ISwapChainPanelNative>();
        winrt::check_hresult(panelNative->SetSwapChain(m_swapChain.get()));

        // Keep latency low: one queued frame.
        dxgiDevice->SetMaximumFrameLatency(1);

        CreateBackBufferViews();
    }

    void DeviceResources::SetPendingMetrics(PanelMetrics const& metrics)
    {
        std::lock_guard lock(m_pendingMutex);
        m_pending = metrics;
    }

    bool DeviceResources::ApplyPendingResize()
    {
        PanelMetrics metrics{};
        {
            std::lock_guard lock(m_pendingMutex);
            if (!m_pending)
            {
                return false;
            }
            metrics = *m_pending;
            m_pending.reset();
        }

        if (metrics.dipWidth <= 0.0f || metrics.dipHeight <= 0.0f)
        {
            return false;
        }

        m_metrics = metrics;
        uint32_t w = 0, h = 0;
        ComputeTargetSize(metrics, w, h);

        bool changed = false;
        if (w != m_width || h != m_height)
        {
            m_backBufferRtv = nullptr;
            m_context->OMSetRenderTargets(0, nullptr, nullptr);
            m_context->Flush();
            winrt::check_hresult(m_swapChain->ResizeBuffers(kBufferCount, w, h, kBackBufferFormat, 0));
            m_width = w;
            m_height = h;
            CreateBackBufferViews();
            changed = true;
        }

        ApplyMatrixTransform();
        return changed;
    }

    void DeviceResources::ApplyMatrixTransform()
    {
        // The panel displays one swap-chain pixel per DIP by default. Scale the
        // (e.g. 3840x2160) swap chain down into the panel's DIP size so XAML's
        // layout scale (200% on a 4K Xbox) maps it 1:1 to physical pixels.
        auto swapChain2 = m_swapChain.try_as<IDXGISwapChain2>();
        if (!swapChain2 || m_width == 0 || m_height == 0)
        {
            return;
        }
        DXGI_MATRIX_3X2_F m{};
        m._11 = m_metrics.dipWidth / static_cast<float>(m_width);
        m._22 = m_metrics.dipHeight / static_cast<float>(m_height);
        swapChain2->SetMatrixTransform(&m);
    }

    void DeviceResources::CreateBackBufferViews()
    {
        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        winrt::check_hresult(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));
        winrt::check_hresult(m_device->CreateRenderTargetView(backBuffer.get(), nullptr, m_backBufferRtv.put()));
    }

    void DeviceResources::Present()
    {
        HRESULT hr = m_swapChain->Present(1, 0);
        // Flip model discards the back buffer contents after Present.
        m_context->DiscardView(m_backBufferRtv.get());
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // Device loss is rare on console; surface it so it is visible in the debugger.
            winrt::check_hresult(hr);
        }
    }

    void DeviceResources::Trim()
    {
        if (m_device)
        {
            m_context->ClearState();
            m_device.as<IDXGIDevice3>()->Trim();
        }
    }
}
