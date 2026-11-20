#pragma once
// LibretroCore: loads one libretro core DLL from the app package
// (LoadPackagedLibrary), implements the environment callback, and exposes
// the retro_* lifecycle to the frontend. Hardware-rendered cores that request
// RETRO_HW_CONTEXT_D3D11 share the frontend's device and immediate context
// (retro_hw_render_interface_d3d11, version 1: the core runs on the render
// thread, so no context locking is needed).
//
// libretro callbacks carry no user pointer, so only one LibretroCore may be
// active at a time (s_active).

#include "libretro.h"
#include "libretro_d3d.h"

namespace rfx
{
    class ICoreHost
    {
    public:
        virtual ~ICoreHost() = default;
        virtual void OnSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch, retro_pixel_format format) = 0;
        virtual void OnHardwareFrame(ID3D11ShaderResourceView* srv, unsigned width, unsigned height) = 0;
        virtual void OnAudioFrames(const int16_t* interleavedStereo, size_t frames) = 0;
        virtual void OnInputPoll() = 0;
        virtual int16_t OnInputState(unsigned port, unsigned device, unsigned index, unsigned id) = 0;
        virtual bool OnRumble(unsigned port, retro_rumble_effect effect, uint16_t strength) = 0;
        virtual void OnCoreMessage(std::string const& text, unsigned frames) = 0;
        virtual void OnAvInfoChanged() = 0;
        virtual ID3D11Device* D3DDevice() = 0;
        virtual ID3D11DeviceContext* D3DContext() = 0;
        virtual D3D_FEATURE_LEVEL D3DFeatureLevel() = 0;
    };

    struct CoreDirectories
    {
        std::string system;   // BIOS files (UTF-8)
        std::string saves;    // SRAM / memory cards (UTF-8)
    };

    class LibretroCore
    {
    public:
        explicit LibretroCore(ICoreHost& host);
        ~LibretroCore();

        LibretroCore(LibretroCore const&) = delete;
        LibretroCore& operator=(LibretroCore const&) = delete;

        // packagePath: path relative to the package root, e.g. L"Cores\\snes9x_libretro.dll".
        bool Load(std::wstring const& packagePath, CoreDirectories const& dirs, std::string& error);
        void Unload();

        // data may be empty if the core needs a full path.
        bool LoadGame(std::string const& utf8Path, std::vector<uint8_t> data, std::string& error);
        void UnloadGame();

        void RunFrame();
        void Reset();

        size_t SerializeSize() const;
        bool Serialize(std::vector<uint8_t>& out) const;
        bool Unserialize(std::vector<uint8_t> const& in);

        // SRAM access (RETRO_MEMORY_SAVE_RAM).
        bool ReadSaveRam(std::vector<uint8_t>& out) const;
        bool WriteSaveRam(std::vector<uint8_t> const& in);

        bool IsLoaded() const { return m_module != nullptr; }
        bool IsGameLoaded() const { return m_gameLoaded; }
        bool NeedsFullPath() const { return m_systemInfo.need_fullpath; }
        bool IsHardwareRendered() const { return m_hwRender; }
        std::string const& LibraryName() const { return m_libraryName; }
        std::string const& LibraryVersion() const { return m_libraryVersion; }
        std::string const& ValidExtensions() const { return m_validExtensions; }
        retro_system_av_info const& AvInfo() const { return m_avInfo; }
        float AspectRatio() const;

        // Probes a core DLL for its system info without initializing it.
        struct ProbeResult
        {
            std::string libraryName;
            std::string libraryVersion;
            std::string validExtensions;
            bool needFullPath = false;
        };
        static bool Probe(std::wstring const& packagePath, ProbeResult& out);

    private:
        // retro_* entry points
        struct Api
        {
            void (*init)() = nullptr;
            void (*deinit)() = nullptr;
            unsigned (*api_version)() = nullptr;
            void (*get_system_info)(retro_system_info*) = nullptr;
            void (*get_system_av_info)(retro_system_av_info*) = nullptr;
            void (*set_environment)(retro_environment_t) = nullptr;
            void (*set_video_refresh)(retro_video_refresh_t) = nullptr;
            void (*set_audio_sample)(retro_audio_sample_t) = nullptr;
            void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
            void (*set_input_poll)(retro_input_poll_t) = nullptr;
            void (*set_input_state)(retro_input_state_t) = nullptr;
            void (*set_controller_port_device)(unsigned, unsigned) = nullptr;
            void (*reset)() = nullptr;
            void (*run)() = nullptr;
            size_t (*serialize_size)() = nullptr;
            bool (*serialize)(void*, size_t) = nullptr;
            bool (*unserialize)(const void*, size_t) = nullptr;
            bool (*load_game)(const retro_game_info*) = nullptr;
            void (*unload_game)() = nullptr;
            void* (*get_memory_data)(unsigned) = nullptr;
            size_t (*get_memory_size)(unsigned) = nullptr;
        };

        bool Environment(unsigned cmd, void* data);
        void StoreOptionDefault(const char* key, const char* value);

        static bool RETRO_CALLCONV EnvironmentThunk(unsigned cmd, void* data);
        static void RETRO_CALLCONV VideoRefreshThunk(const void* data, unsigned width, unsigned height, size_t pitch);
        static void RETRO_CALLCONV AudioSampleThunk(int16_t left, int16_t right);
        static size_t RETRO_CALLCONV AudioSampleBatchThunk(const int16_t* data, size_t frames);
        static void RETRO_CALLCONV InputPollThunk();
        static int16_t RETRO_CALLCONV InputStateThunk(unsigned port, unsigned device, unsigned index, unsigned id);
        static bool RETRO_CALLCONV RumbleThunk(unsigned port, retro_rumble_effect effect, uint16_t strength);
        static void RETRO_CALLCONV LogThunk(retro_log_level level, const char* fmt, ...);

        static LibretroCore* s_active;

        ICoreHost& m_host;
        HMODULE m_module = nullptr;
        Api m_api{};
        bool m_initialized = false;
        bool m_gameLoaded = false;

        retro_system_info m_systemInfo{};
        retro_system_av_info m_avInfo{};
        std::string m_libraryName;
        std::string m_libraryVersion;
        std::string m_validExtensions;
        retro_pixel_format m_pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;

        CoreDirectories m_dirs;
        std::string m_libretroPath;
        std::string m_gamePath;
        std::vector<uint8_t> m_gameData; // must stay alive while the game is loaded

        bool m_hwRender = false;
        retro_hw_render_callback m_hwCallback{};
        retro_hw_render_interface_d3d11 m_d3dInterface{};

        std::map<std::string, std::string> m_options;
        bool m_supportsNoGame = false;
    };
}
