#pragma once
// Frontend: owns the render thread and ties together the D3D11 device,
// post-processing, the libretro core, audio, input, the ROM library and the
// ImGui blade dashboard.
//
// Threads:
//   UI (XAML) thread : Initialize, OnPanelMetricsChanged, RequestToggleMenu,
//                      OnRomFolderPicked, Suspend, Resume, Shutdown
//   Render thread    : everything D3D11 / ImGui / libretro (cores are
//                      single-threaded from the frontend's point of view)
//   Worker threads   : library scans and ROM reads (blocking WinRT I/O)

#include "Core/LibretroCore.h"
#include "Core/AudioOutput.h"
#include "Core/InputMapper.h"
#include "Graphics/DeviceResources.h"
#include "Graphics/PostProcess.h"
#include "Storage/RomLibrary.h"
#include "UI/BladeMenu.h"

namespace rfx
{
    struct Channel
    {
        std::wstring corePath;   // package-relative core DLL
        std::wstring gamePath;
        std::string coreName;
        std::string gameName;
    };

    class Frontend final : public ICoreHost
    {
    public:
        Frontend();
        ~Frontend() override;

        // --- UI thread ---
        void SetFolderPickerRequestHandler(std::function<void()> handler) { m_requestFolderPicker = std::move(handler); }
        void Initialize(winrt::Windows::UI::Xaml::Controls::SwapChainPanel const& panel);
        void OnPanelMetricsChanged(PanelMetrics const& metrics);
        void Start();
        void Shutdown();
        void OnRomFolderPicked(winrt::Windows::Storage::StorageFolder const& folder);
        void Suspend();
        void Resume();
        void RequestToggleMenu() { m_toggleMenuRequested = true; }

        // --- ICoreHost (render thread) ---
        void OnSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch, retro_pixel_format format) override;
        void OnHardwareFrame(ID3D11ShaderResourceView* srv, unsigned width, unsigned height) override;
        void OnAudioFrames(const int16_t* interleavedStereo, size_t frames) override;
        void OnInputPoll() override;
        int16_t OnInputState(unsigned port, unsigned device, unsigned index, unsigned id) override;
        bool OnRumble(unsigned port, retro_rumble_effect effect, uint16_t strength) override;
        void OnCoreMessage(std::string const& text, unsigned frames) override;
        void OnAvInfoChanged() override;
        ID3D11Device* D3DDevice() override { return m_device.Device(); }
        ID3D11DeviceContext* D3DContext() override { return m_device.Context(); }
        D3D_FEATURE_LEVEL D3DFeatureLevel() override { return m_device.FeatureLevel(); }

    private:
        enum class Mode { Dashboard, Playing, Menu };

        struct PendingLaunch
        {
            Channel channel;
            bool resumeAutoState = false;
            std::atomic<bool> ready{ false }; // set by the worker after data/error are written
            bool failed = false;
            std::string error;
            std::wstring readablePath;
            std::vector<uint8_t> data;
        };

        struct Surf
        {
            bool active = false;
            float progress = 0.0f;
            bool launched = false;     // midpoint work done
            std::shared_ptr<PendingLaunch> launch; // null = surf back to dashboard
        };

        void RenderLoop();
        void InitImGui();
        void ApplyUiScale();
        void Frame(float dt);

        // Game lifecycle (render thread)
        void BeginLaunch(Channel const& channel, bool resumeAutoState);
        void BeginSurf(std::shared_ptr<PendingLaunch> launch); // null = back to dashboard
        void UpdateSurf(float dt);
        void StartLoadWorker(std::shared_ptr<PendingLaunch> const& launch);
        bool CompleteLaunch(PendingLaunch& launch);
        void CloseGame(bool writeAutoState);
        void SurfChannel(int direction);

        // Persistence
        std::wstring SaveBaseName() const;
        void WriteSram();
        void ReadSram();
        bool SaveState(std::wstring const& suffix);
        bool LoadState(std::wstring const& suffix);
        void LoadChannels();
        void SaveChannels();
        void AddChannelToFront(Channel const& ch);
        void LoadSettings();
        void SaveSettings();

        // UI
        void BuildMenu();
        void RefreshDynamicBlades();
        void HandleMenuInput(float dt);
        void DrawDashboardBackground();
        void DrawOverlay();
        void Toast(std::string text, float seconds = 3.0f);
        void StartScan(bool cores);
        void RunWorker(std::function<void()> fn);

        // Devices
        DeviceResources m_device;
        PostProcess m_post;
        AudioOutput m_audio;
        InputMapper m_input;
        RomLibrary m_library;
        std::unique_ptr<LibretroCore> m_core;
        CrtSettings m_crt;

        // Thread control
        std::thread m_renderThread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_toggleMenuRequested{ false };
        std::atomic<bool> m_rescanRequested{ false };
        std::atomic<bool> m_suspendRequested{ false };
        std::atomic<bool> m_suspended{ false };
        std::mutex m_suspendMutex;
        std::condition_variable m_suspendCv;
        std::atomic<float> m_pointerScale{ 1.0f };
        std::function<void()> m_requestFolderPicker;
        std::vector<std::thread> m_workers;
        std::mutex m_workerMutex;

        // State
        Mode m_mode = Mode::Dashboard;
        Surf m_surf;
        Channel m_current;
        std::vector<Channel> m_channels;
        int m_channelIndex = -1;
        double m_frameAccumulator = 0.0;
        double m_time = 0.0;
        uint64_t m_libraryRevisionSeen = ~0ull;
        std::vector<GameEntry> m_games;
        std::vector<CoreEntry> m_cores;
        std::map<std::wstring, size_t> m_coreChoice; // game path -> index into GameEntry::coreIndices
        uint32_t m_uiHeight = 0;
        float m_uiScale = 1.0f;

        // Paths (UTF-16)
        std::wstring m_localDir, m_savesDir, m_statesDir, m_systemDir;

        // UI
        BladeMenu m_menu;
        size_t m_bladeNowPlaying = 0, m_bladeGames = 0, m_bladeChannels = 0, m_bladeSystems = 0, m_bladeDisplay = 0, m_bladeSystem = 0;
        struct RepeatState { float held = 0.0f; float next = 0.0f; };
        std::array<RepeatState, 4> m_repeat{};
        std::string m_toast;
        float m_toastTime = 0.0f;
        std::string m_banner;
        float m_bannerTime = 0.0f;
    };
}
