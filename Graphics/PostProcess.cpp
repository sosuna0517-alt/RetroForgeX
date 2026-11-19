#include "pch.h"
#include "PostProcess.h"

// Generated at build time by FXC (see HeaderFileOutput in the .vcxproj).
#include "VS_Fullscreen.h"
#include "PS_CRT_ApertureGrille.h"
#include "PS_ChannelSurf.h"

namespace rfx
{
    namespace
    {
        constexpr DXGI_FORMAT kSceneFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

        inline uint32_t Expand565(uint16_t c)
        {
            uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }

        inline uint32_t Expand1555(uint16_t c)
        {
            uint32_t r = (c >> 10) & 0x1F, g = (c >> 5) & 0x1F, b = c & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);
            return 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    void PostProcess::Initialize(ID3D11Device1* device, ID3D11DeviceContext1* context)
    {
        m_device = device;
        m_context = context;

        winrt::check_hresult(m_device->CreateVertexShader(g_VS_Fullscreen, sizeof(g_VS_Fullscreen), nullptr, m_vsFullscreen.put()));
        winrt::check_hresult(m_device->CreatePixelShader(g_PS_CRT_ApertureGrille, sizeof(g_PS_CRT_ApertureGrille), nullptr, m_psCrt.put()));
        winrt::check_hresult(m_device->CreatePixelShader(g_PS_ChannelSurf, sizeof(g_PS_ChannelSurf), nullptr, m_psSurf.put()));

        D3D11_BUFFER_DESC cb{};
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cb.ByteWidth = sizeof(CrtConstants);
        winrt::check_hresult(m_device->CreateBuffer(&cb, nullptr, m_crtConstants.put()));
        cb.ByteWidth = sizeof(SurfConstants);
        winrt::check_hresult(m_device->CreateBuffer(&cb, nullptr, m_surfConstants.put()));

        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        winrt::check_hresult(m_device->CreateSamplerState(&sd, m_linearClamp.put()));

        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        winrt::check_hresult(m_device->CreateRasterizerState(&rd, m_rasterizer.put()));
    }

    void PostProcess::CreateTarget(uint32_t w, uint32_t h, winrt::com_ptr<ID3D11Texture2D>& tex,
        winrt::com_ptr<ID3D11RenderTargetView>& rtv, winrt::com_ptr<ID3D11ShaderResourceView>& srv)
    {
        tex = nullptr; rtv = nullptr; srv = nullptr;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = kSceneFormat;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        winrt::check_hresult(m_device->CreateTexture2D(&td, nullptr, tex.put()));
        winrt::check_hresult(m_device->CreateRenderTargetView(tex.get(), nullptr, rtv.put()));
        winrt::check_hresult(m_device->CreateShaderResourceView(tex.get(), nullptr, srv.put()));
        const float black[4] = { 0, 0, 0, 1 };
        m_context->ClearRenderTargetView(rtv.get(), black);
    }

    void PostProcess::Resize(uint32_t width, uint32_t height)
    {
        if (width == m_width && height == m_height && m_sceneTex)
        {
            return;
        }
        m_width = width;
        m_height = height;
        CreateTarget(width, height, m_sceneTex, m_sceneRtv, m_sceneSrv);
        CreateTarget(width, height, m_snapshotTex, m_snapshotRtv, m_snapshotSrv);
    }

    void PostProcess::EnsureUploadTexture(unsigned width, unsigned height)
    {
        if (m_uploadTex && width <= m_uploadW && height <= m_uploadH)
        {
            return;
        }
        // Grow in coarse steps to avoid recreating on small geometry changes.
        m_uploadW = std::max(width, std::max(m_uploadW, 256u));
        m_uploadH = std::max(height, std::max(m_uploadH, 256u));

        D3D11_TEXTURE2D_DESC td{};
        td.Width = m_uploadW;
        td.Height = m_uploadH;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DYNAMIC;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_uploadTex = nullptr;
        m_uploadSrv = nullptr;
        winrt::check_hresult(m_device->CreateTexture2D(&td, nullptr, m_uploadTex.put()));
        winrt::check_hresult(m_device->CreateShaderResourceView(m_uploadTex.get(), nullptr, m_uploadSrv.put()));
    }

    void PostProcess::UploadSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch, retro_pixel_format format)
    {
        if (!data || width == 0 || height == 0)
        {
            return; // dupe frame: keep showing the previous one
        }

        EnsureUploadTexture(width, height);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(m_context->Map(m_uploadTex.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            return;
        }

        const auto* src = static_cast<const uint8_t*>(data);
        auto* dst = static_cast<uint8_t*>(mapped.pData);
        for (unsigned y = 0; y < height; ++y)
        {
            auto* out = reinterpret_cast<uint32_t*>(dst + y * mapped.RowPitch);
            const uint8_t* row = src + y * pitch;
            switch (format)
            {
            case RETRO_PIXEL_FORMAT_XRGB8888:
            {
                const auto* in = reinterpret_cast<const uint32_t*>(row);
                for (unsigned x = 0; x < width; ++x) out[x] = in[x] | 0xFF000000u;
                break;
            }
            case RETRO_PIXEL_FORMAT_RGB565:
            {
                const auto* in = reinterpret_cast<const uint16_t*>(row);
                for (unsigned x = 0; x < width; ++x) out[x] = Expand565(in[x]);
                break;
            }
            default: // RETRO_PIXEL_FORMAT_0RGB1555
            {
                const auto* in = reinterpret_cast<const uint16_t*>(row);
                for (unsigned x = 0; x < width; ++x) out[x] = Expand1555(in[x]);
                break;
            }
            }
        }
        m_context->Unmap(m_uploadTex.get(), 0);

        m_frameSrv = m_uploadSrv;
        m_frameW = width;
        m_frameH = height;
        m_frameUvScale[0] = static_cast<float>(width) / static_cast<float>(m_uploadW);
        m_frameUvScale[1] = static_cast<float>(height) / static_cast<float>(m_uploadH);
    }

    void PostProcess::SetHardwareFrame(ID3D11ShaderResourceView* srv, unsigned width, unsigned height)
    {
        if (!srv)
        {
            return;
        }
        m_frameSrv.copy_from(srv);
        m_frameW = width;
        m_frameH = height;
        m_frameUvScale[0] = m_frameUvScale[1] = 1.0f;

        winrt::com_ptr<ID3D11Resource> resource;
        srv->GetResource(resource.put());
        if (auto tex = resource.try_as<ID3D11Texture2D>())
        {
            D3D11_TEXTURE2D_DESC td{};
            tex->GetDesc(&td);
            if (td.Width > 0 && td.Height > 0)
            {
                m_frameUvScale[0] = static_cast<float>(width) / static_cast<float>(td.Width);
                m_frameUvScale[1] = static_cast<float>(height) / static_cast<float>(td.Height);
            }
        }
    }

    void PostProcess::ClearFrame()
    {
        m_frameSrv = nullptr;
        m_frameW = m_frameH = 0;
    }

    void PostProcess::BindCommonState()
    {
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        m_context->VSSetShader(m_vsFullscreen.get(), nullptr, 0);
        m_context->GSSetShader(nullptr, nullptr, 0);
        m_context->HSSetShader(nullptr, nullptr, 0);
        m_context->DSSetShader(nullptr, nullptr, 0);
        m_context->RSSetState(m_rasterizer.get());
        m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        m_context->OMSetDepthStencilState(nullptr, 0);
        ID3D11SamplerState* sampler = m_linearClamp.get();
        m_context->PSSetSamplers(0, 1, &sampler);
    }

    void PostProcess::RenderScene(float aspectRatio, CrtSettings const& s)
    {
        const float black[4] = { 0, 0, 0, 1 };
        m_context->ClearRenderTargetView(m_sceneRtv.get(), black);
        if (!m_frameSrv || m_frameW == 0 || m_frameH == 0)
        {
            return;
        }

        if (aspectRatio <= 0.0f)
        {
            aspectRatio = static_cast<float>(m_frameW) / static_cast<float>(m_frameH);
        }

        // Fit the game area into the target, preserving aspect ratio.
        float vw = static_cast<float>(m_width);
        float vh = vw / aspectRatio;
        if (vh > static_cast<float>(m_height))
        {
            vh = static_cast<float>(m_height);
            vw = vh * aspectRatio;
        }
        if (s.integerScale && m_frameH > 0)
        {
            float scale = std::floor(static_cast<float>(m_height) / static_cast<float>(m_frameH));
            if (scale >= 1.0f)
            {
                vh = scale * static_cast<float>(m_frameH);
                vw = vh * aspectRatio;
            }
        }
        vw = std::floor(vw);
        vh = std::floor(vh);
        const float vx = std::floor((static_cast<float>(m_width) - vw) * 0.5f);
        const float vy = std::floor((static_cast<float>(m_height) - vh) * 0.5f);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_crtConstants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            CrtConstants c{};
            c.sourceSize[0] = static_cast<float>(m_frameW);
            c.sourceSize[1] = static_cast<float>(m_frameH);
            c.sourceUvScale[0] = m_frameUvScale[0];
            c.sourceUvScale[1] = m_frameUvScale[1];
            c.outputSize[0] = vw;
            c.outputSize[1] = vh;
            c.viewportOrigin[0] = vx;
            c.viewportOrigin[1] = vy;
            c.scanlineStrength = s.scanlineStrength;
            c.maskStrength = s.maskStrength;
            c.bloom = s.bloom;
            c.curvature = s.curvature;
            c.gammaIn = s.gammaIn;
            c.gammaOut = s.gammaOut;
            c.maskPitch = s.maskPitch;
            c.enabled = s.enabled ? 1.0f : 0.0f;
            std::memcpy(mapped.pData, &c, sizeof(c));
            m_context->Unmap(m_crtConstants.get(), 0);
        }

        BindCommonState();
        ID3D11RenderTargetView* rtv = m_sceneRtv.get();
        m_context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{ vx, vy, vw, vh, 0.0f, 1.0f };
        m_context->RSSetViewports(1, &vp);
        m_context->PSSetShader(m_psCrt.get(), nullptr, 0);
        ID3D11Buffer* cbuf = m_crtConstants.get();
        m_context->PSSetConstantBuffers(0, 1, &cbuf);
        ID3D11ShaderResourceView* srv = m_frameSrv.get();
        m_context->PSSetShaderResources(0, 1, &srv);
        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSrv);
    }

    void PostProcess::CaptureSnapshot()
    {
        if (m_sceneTex && m_snapshotTex)
        {
            m_context->CopyResource(m_snapshotTex.get(), m_sceneTex.get());
        }
    }

    void PostProcess::RenderFinal(ID3D11RenderTargetView* backBuffer, bool useSnapshot, float progress, float timeSeconds, bool transitionActive)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_surfConstants.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            SurfConstants c{};
            c.progress = progress;
            c.time = timeSeconds;
            c.outputSize[0] = static_cast<float>(m_width);
            c.outputSize[1] = static_cast<float>(m_height);
            c.active = transitionActive ? 1.0f : 0.0f;
            std::memcpy(mapped.pData, &c, sizeof(c));
            m_context->Unmap(m_surfConstants.get(), 0);
        }

        BindCommonState();
        m_context->OMSetRenderTargets(1, &backBuffer, nullptr);
        D3D11_VIEWPORT vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        m_context->RSSetViewports(1, &vp);
        m_context->PSSetShader(m_psSurf.get(), nullptr, 0);
        ID3D11Buffer* cbuf = m_surfConstants.get();
        m_context->PSSetConstantBuffers(0, 1, &cbuf);
        ID3D11ShaderResourceView* srv = useSnapshot ? m_snapshotSrv.get() : m_sceneSrv.get();
        m_context->PSSetShaderResources(0, 1, &srv);
        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        m_context->PSSetShaderResources(0, 1, &nullSrv);
    }
}
