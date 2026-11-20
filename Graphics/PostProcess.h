#pragma once
// PostProcess: core frame -> CRT pass (4K scene target) -> Channel Surf pass
// (back buffer). Also owns the upload texture for software-rendered cores and
// accepts the shader resource view handed over by RETRO_HW_CONTEXT_D3D11 cores.
//
// All methods: render thread only.

#include "libretro.h"

namespace rfx
{
    struct CrtSettings
    {
        bool  enabled = true;
        float scanlineStrength = 0.55f;
        float maskStrength = 0.45f;
        float bloom = 0.35f;
        float curvature = 0.03f;
        float gammaIn = 2.4f;
        float gammaOut = 2.2f;
        float maskPitch = 3.0f;   // output pixels per RGB triad
        bool  integerScale = false;
    };

    class PostProcess
    {
    public:
        void Initialize(ID3D11Device1* device, ID3D11DeviceContext1* context);
        void Resize(uint32_t width, uint32_t height);

        // Core frame input.
        void UploadSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch, retro_pixel_format format);
        void SetHardwareFrame(ID3D11ShaderResourceView* srv, unsigned width, unsigned height);
        void ClearFrame();
        bool HasFrame() const { return m_frameSrv != nullptr; }

        // Renders the current core frame through the CRT pass into the scene target.
        void RenderScene(float aspectRatio, CrtSettings const& settings);
        // Copies the last scene into the snapshot (the "old channel" for transitions).
        void CaptureSnapshot();
        // Final pass into the back buffer.
        void RenderFinal(ID3D11RenderTargetView* backBuffer, bool useSnapshot, float progress, float timeSeconds, bool transitionActive);

    private:
        struct CrtConstants
        {
            float sourceSize[2];
            float sourceUvScale[2];
            float outputSize[2];
            float viewportOrigin[2];
            float scanlineStrength;
            float maskStrength;
            float bloom;
            float curvature;
            float gammaIn;
            float gammaOut;
            float maskPitch;
            float enabled;
        };
        static_assert(sizeof(CrtConstants) % 16 == 0, "constant buffer must be 16-byte aligned");

        struct SurfConstants
        {
            float progress;
            float time;
            float outputSize[2];
            float active;
            float pad[3];
        };
        static_assert(sizeof(SurfConstants) % 16 == 0, "constant buffer must be 16-byte aligned");

        void CreateTarget(uint32_t w, uint32_t h, winrt::com_ptr<ID3D11Texture2D>& tex,
            winrt::com_ptr<ID3D11RenderTargetView>& rtv, winrt::com_ptr<ID3D11ShaderResourceView>& srv);
        void EnsureUploadTexture(unsigned width, unsigned height);
        void BindCommonState();

        ID3D11Device1* m_device = nullptr;
        ID3D11DeviceContext1* m_context = nullptr;

        winrt::com_ptr<ID3D11VertexShader> m_vsFullscreen;
        winrt::com_ptr<ID3D11PixelShader> m_psCrt;
        winrt::com_ptr<ID3D11PixelShader> m_psSurf;
        winrt::com_ptr<ID3D11Buffer> m_crtConstants;
        winrt::com_ptr<ID3D11Buffer> m_surfConstants;
        winrt::com_ptr<ID3D11SamplerState> m_linearClamp;
        winrt::com_ptr<ID3D11RasterizerState> m_rasterizer;

        // Scene (CRT output) and snapshot targets at back-buffer size.
        winrt::com_ptr<ID3D11Texture2D> m_sceneTex;
        winrt::com_ptr<ID3D11RenderTargetView> m_sceneRtv;
        winrt::com_ptr<ID3D11ShaderResourceView> m_sceneSrv;
        winrt::com_ptr<ID3D11Texture2D> m_snapshotTex;
        winrt::com_ptr<ID3D11RenderTargetView> m_snapshotRtv;
        winrt::com_ptr<ID3D11ShaderResourceView> m_snapshotSrv;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // Software frame upload.
        winrt::com_ptr<ID3D11Texture2D> m_uploadTex;
        winrt::com_ptr<ID3D11ShaderResourceView> m_uploadSrv;
        unsigned m_uploadW = 0;
        unsigned m_uploadH = 0;

        // Current frame (either m_uploadSrv or the core's HW SRV).
        winrt::com_ptr<ID3D11ShaderResourceView> m_frameSrv;
        unsigned m_frameW = 0;
        unsigned m_frameH = 0;
        float m_frameUvScale[2] = { 1.0f, 1.0f };
    };
}
