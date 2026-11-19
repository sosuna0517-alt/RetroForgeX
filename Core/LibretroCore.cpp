#include "pch.h"
#include "LibretroCore.h"

#include <cstdarg>
#include <cstdio>

namespace rfx
{
    LibretroCore* LibretroCore::s_active = nullptr;

    namespace
    {
        std::string Narrow(std::wstring const& w)
        {
            return winrt::to_string(w);
        }

        template <typename T>
        bool Bind(HMODULE module, const char* name, T& fn)
        {
            fn = reinterpret_cast<T>(GetProcAddress(module, name));
            return fn != nullptr;
        }

        // "Description; first|second|third" -> "first"
        std::string FirstChoice(const char* value)
        {
            if (!value) return {};
            std::string s(value);
            auto semi = s.find(';');
            if (semi != std::string::npos)
            {
                s = s.substr(semi + 1);
            }
            s.erase(0, s.find_first_not_of(' '));
            auto bar = s.find('|');
            return bar == std::string::npos ? s : s.substr(0, bar);
        }
    }

    LibretroCore::LibretroCore(ICoreHost& host) : m_host(host)
    {
    }

    LibretroCore::~LibretroCore()
    {
        Unload();
    }

    bool LibretroCore::Probe(std::wstring const& packagePath, ProbeResult& out)
    {
        HMODULE module = LoadPackagedLibrary(packagePath.c_str(), 0);
        if (!module)
        {
            return false;
        }
        void (*getInfo)(retro_system_info*) = nullptr;
        bool ok = Bind(module, "retro_get_system_info", getInfo);
        if (ok)
        {
            retro_system_info info{};
            getInfo(&info);
            out.libraryName = info.library_name ? info.library_name : "";
            out.libraryVersion = info.library_version ? info.library_version : "";
            out.validExtensions = info.valid_extensions ? info.valid_extensions : "";
            out.needFullPath = info.need_fullpath;
        }
        FreeLibrary(module);
        return ok;
    }

    bool LibretroCore::Load(std::wstring const& packagePath, CoreDirectories const& dirs, std::string& error)
    {
        Unload();

        m_module = LoadPackagedLibrary(packagePath.c_str(), 0);
        if (!m_module)
        {
            error = "LoadPackagedLibrary failed (" + std::to_string(GetLastError()) + ") for " + Narrow(packagePath);
            return false;
        }

        bool ok = true;
        ok &= Bind(m_module, "retro_init", m_api.init);
        ok &= Bind(m_module, "retro_deinit", m_api.deinit);
        ok &= Bind(m_module, "retro_api_version", m_api.api_version);
        ok &= Bind(m_module, "retro_get_system_info", m_api.get_system_info);
        ok &= Bind(m_module, "retro_get_system_av_info", m_api.get_system_av_info);
        ok &= Bind(m_module, "retro_set_environment", m_api.set_environment);
        ok &= Bind(m_module, "retro_set_video_refresh", m_api.set_video_refresh);
        ok &= Bind(m_module, "retro_set_audio_sample", m_api.set_audio_sample);
        ok &= Bind(m_module, "retro_set_audio_sample_batch", m_api.set_audio_sample_batch);
        ok &= Bind(m_module, "retro_set_input_poll", m_api.set_input_poll);
        ok &= Bind(m_module, "retro_set_input_state", m_api.set_input_state);
        ok &= Bind(m_module, "retro_set_controller_port_device", m_api.set_controller_port_device);
        ok &= Bind(m_module, "retro_reset", m_api.reset);
        ok &= Bind(m_module, "retro_run", m_api.run);
        ok &= Bind(m_module, "retro_serialize_size", m_api.serialize_size);
        ok &= Bind(m_module, "retro_serialize", m_api.serialize);
        ok &= Bind(m_module, "retro_unserialize", m_api.unserialize);
        ok &= Bind(m_module, "retro_load_game", m_api.load_game);
        ok &= Bind(m_module, "retro_unload_game", m_api.unload_game);
        Bind(m_module, "retro_get_memory_data", m_api.get_memory_data);
        Bind(m_module, "retro_get_memory_size", m_api.get_memory_size);

        if (!ok)
        {
            error = "Not a libretro core (missing retro_* exports)";
            Unload();
            return false;
        }
        if (m_api.api_version() != RETRO_API_VERSION)
        {
            error = "Unsupported libretro API version";
            Unload();
            return false;
        }

        m_dirs = dirs;
        m_libretroPath = Narrow(packagePath);
        m_options.clear();
        m_hwRender = false;
        m_hwCallback = {};
        m_pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;

        s_active = this;

        m_api.get_system_info(&m_systemInfo);
        m_libraryName = m_systemInfo.library_name ? m_systemInfo.library_name : "";
        m_libraryVersion = m_systemInfo.library_version ? m_systemInfo.library_version : "";
        m_validExtensions = m_systemInfo.valid_extensions ? m_systemInfo.valid_extensions : "";

        m_api.set_environment(&EnvironmentThunk);
        m_api.init();
        m_initialized = true;

        m_api.set_video_refresh(&VideoRefreshThunk);
        m_api.set_audio_sample(&AudioSampleThunk);
        m_api.set_audio_sample_batch(&AudioSampleBatchThunk);
        m_api.set_input_poll(&InputPollThunk);
        m_api.set_input_state(&InputStateThunk);
        return true;
    }

    void LibretroCore::Unload()
    {
        if (m_gameLoaded)
        {
            UnloadGame();
        }
        if (m_initialized && m_api.deinit)
        {
            s_active = this;
            m_api.deinit();
        }
        m_initialized = false;
        if (m_module)
        {
            FreeLibrary(m_module);
            m_module = nullptr;
        }
        m_api = {};
        if (s_active == this)
        {
            s_active = nullptr;
        }
    }

    bool LibretroCore::LoadGame(std::string const& utf8Path, std::vector<uint8_t> data, std::string& error)
    {
        if (!m_initialized)
        {
            error = "Core not loaded";
            return false;
        }
        s_active = this;

        m_gamePath = utf8Path;
        m_gameData = std::move(data);

        retro_game_info info{};
        info.path = m_gamePath.empty() ? nullptr : m_gamePath.c_str();
        if (!m_systemInfo.need_fullpath && !m_gameData.empty())
        {
            info.data = m_gameData.data();
            info.size = m_gameData.size();
        }

        if (!m_api.load_game(info.path || info.data ? &info : nullptr))
        {
            error = "retro_load_game failed";
            m_gameData.clear();
            return false;
        }
        m_gameLoaded = true;

        m_api.get_system_av_info(&m_avInfo);

        if (m_hwRender && m_hwCallback.context_reset)
        {
            m_hwCallback.context_reset();
        }

        for (unsigned port = 0; port < 4; ++port)
        {
            m_api.set_controller_port_device(port, RETRO_DEVICE_JOYPAD);
        }
        return true;
    }

    void LibretroCore::UnloadGame()
    {
        if (!m_gameLoaded)
        {
            return;
        }
        s_active = this;
        if (m_hwRender && m_hwCallback.context_destroy)
        {
            m_hwCallback.context_destroy();
        }
        m_api.unload_game();
        m_gameLoaded = false;
        m_gameData.clear();
        m_gameData.shrink_to_fit();
    }

    void LibretroCore::RunFrame()
    {
        if (!m_gameLoaded)
        {
            return;
        }
        s_active = this;
        m_api.run();
    }

    void LibretroCore::Reset()
    {
        if (m_gameLoaded)
        {
            s_active = this;
            m_api.reset();
        }
    }

    size_t LibretroCore::SerializeSize() const
    {
        return m_gameLoaded ? m_api.serialize_size() : 0;
    }

    bool LibretroCore::Serialize(std::vector<uint8_t>& out) const
    {
        const size_t size = SerializeSize();
        if (size == 0)
        {
            return false;
        }
        out.resize(size);
        return m_api.serialize(out.data(), out.size());
    }

    bool LibretroCore::Unserialize(std::vector<uint8_t> const& in)
    {
        if (!m_gameLoaded || in.empty())
        {
            return false;
        }
        s_active = this;
        return m_api.unserialize(in.data(), in.size());
    }

    bool LibretroCore::ReadSaveRam(std::vector<uint8_t>& out) const
    {
        if (!m_gameLoaded || !m_api.get_memory_data || !m_api.get_memory_size)
        {
            return false;
        }
        const size_t size = m_api.get_memory_size(RETRO_MEMORY_SAVE_RAM);
        const void* data = m_api.get_memory_data(RETRO_MEMORY_SAVE_RAM);
        if (!size || !data)
        {
            return false;
        }
        out.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        return true;
    }

    bool LibretroCore::WriteSaveRam(std::vector<uint8_t> const& in)
    {
        if (!m_gameLoaded || !m_api.get_memory_data || !m_api.get_memory_size)
        {
            return false;
        }
        const size_t size = m_api.get_memory_size(RETRO_MEMORY_SAVE_RAM);
        void* data = m_api.get_memory_data(RETRO_MEMORY_SAVE_RAM);
        if (!size || !data)
        {
            return false;
        }
        std::memcpy(data, in.data(), std::min(size, in.size()));
        return true;
    }

    float LibretroCore::AspectRatio() const
    {
        const auto& g = m_avInfo.geometry;
        if (g.aspect_ratio > 0.0f)
        {
            return g.aspect_ratio;
        }
        if (g.base_height > 0)
        {
            return static_cast<float>(g.base_width) / static_cast<float>(g.base_height);
        }
        return 4.0f / 3.0f;
    }

    void LibretroCore::StoreOptionDefault(const char* key, const char* value)
    {
        if (key && value && m_options.find(key) == m_options.end())
        {
            m_options[key] = value;
        }
    }

    bool LibretroCore::Environment(unsigned cmd, void* data)
    {
        switch (cmd)
        {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (data) *static_cast<bool*>(data) = true;
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        {
            auto fmt = *static_cast<const retro_pixel_format*>(data);
            if (fmt == RETRO_PIXEL_FORMAT_0RGB1555 || fmt == RETRO_PIXEL_FORMAT_XRGB8888 || fmt == RETRO_PIXEL_FORMAT_RGB565)
            {
                m_pixelFormat = fmt;
                return true;
            }
            return false;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
            *static_cast<const char**>(data) = m_dirs.system.c_str();
            return true;

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = m_dirs.saves.c_str();
            return true;

        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
            *static_cast<const char**>(data) = m_libretroPath.c_str();
            return true;

        case RETRO_ENVIRONMENT_GET_USERNAME:
            *static_cast<const char**>(data) = "Player";
            return true;

        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            *static_cast<unsigned*>(data) = RETRO_LANGUAGE_ENGLISH;
            return true;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            static_cast<retro_log_callback*>(data)->log = &LogThunk;
            return true;

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
            static_cast<retro_rumble_interface*>(data)->set_rumble_state = &RumbleThunk;
            return true;

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true;

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            if (data) *static_cast<int*>(data) = 3; // video | audio
            return true;

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
            if (data) *static_cast<bool*>(data) = false;
            return true;

        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            m_supportsNoGame = *static_cast<const bool*>(data);
            return true;

        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            m_avInfo.geometry = *static_cast<const retro_game_geometry*>(data);
            m_host.OnAvInfoChanged();
            return true;

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
            m_avInfo = *static_cast<const retro_system_av_info*>(data);
            m_host.OnAvInfoChanged();
            return true;

        case RETRO_ENVIRONMENT_SET_MESSAGE:
        {
            auto msg = static_cast<const retro_message*>(data);
            if (msg && msg->msg) m_host.OnCoreMessage(msg->msg, msg->frames);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
        {
            auto msg = static_cast<const retro_message_ext*>(data);
            if (msg && msg->msg) m_host.OnCoreMessage(msg->msg, msg->duration / 16);
            return true;
        }

        // ---- Core options ------------------------------------------------
        // Returning false from GET_CORE_OPTIONS_VERSION makes most cores fall
        // back to the legacy SET_VARIABLES path; v1/v2 definitions are still
        // accepted below for cores that send them unconditionally.
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            return false;

        case RETRO_ENVIRONMENT_SET_VARIABLES:
            for (auto var = static_cast<const retro_variable*>(data); var && var->key; ++var)
            {
                StoreOptionDefault(var->key, FirstChoice(var->value).c_str());
            }
            return true;

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
            for (auto def = static_cast<const retro_core_option_definition*>(data); def && def->key; ++def)
            {
                StoreOptionDefault(def->key, def->default_value ? def->default_value : def->values[0].value);
            }
            return true;

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        {
            auto intl = static_cast<const retro_core_options_intl*>(data);
            for (auto def = intl ? intl->us : nullptr; def && def->key; ++def)
            {
                StoreOptionDefault(def->key, def->default_value ? def->default_value : def->values[0].value);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        {
            auto opts = static_cast<const retro_core_options_v2*>(data);
            for (auto def = opts ? opts->definitions : nullptr; def && def->key; ++def)
            {
                StoreOptionDefault(def->key, def->default_value ? def->default_value : def->values[0].value);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        {
            auto intl = static_cast<const retro_core_options_v2_intl*>(data);
            auto opts = intl ? intl->us : nullptr;
            for (auto def = opts ? opts->definitions : nullptr; def && def->key; ++def)
            {
                StoreOptionDefault(def->key, def->default_value ? def->default_value : def->values[0].value);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            auto var = static_cast<retro_variable*>(data);
            if (!var || !var->key) return false;
            auto it = m_options.find(var->key);
            if (it == m_options.end()) return false;
            var->value = it->second.c_str();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (data) *static_cast<bool*>(data) = false;
            return true;

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
            return true;

        // ---- Hardware rendering (D3D11 only) -------------------------------
        case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
            *static_cast<unsigned*>(data) = RETRO_HW_CONTEXT_D3D11;
            return true;

        case RETRO_ENVIRONMENT_SET_HW_RENDER:
        {
            auto cb = static_cast<retro_hw_render_callback*>(data);
            if (!cb || cb->context_type != RETRO_HW_CONTEXT_D3D11)
            {
                return false; // OpenGL / Vulkan cores are not supported by this frontend
            }
            cb->get_current_framebuffer = nullptr;
            cb->get_proc_address = nullptr;
            m_hwCallback = *cb;
            m_hwRender = true;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
        {
            if (!m_hwRender)
            {
                return false;
            }
            m_d3dInterface = {};
            m_d3dInterface.interface_type = RETRO_HW_RENDER_INTERFACE_D3D11;
            m_d3dInterface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION;
            m_d3dInterface.handle = this;
            m_d3dInterface.device = m_host.D3DDevice();
            m_d3dInterface.context = m_host.D3DContext();
            m_d3dInterface.featureLevel = m_host.D3DFeatureLevel();
            m_d3dInterface.D3DCompile = &::D3DCompile;
            *static_cast<const retro_hw_render_interface**>(data) =
                reinterpret_cast<const retro_hw_render_interface*>(&m_d3dInterface);
            return true;
        }

        // ---- Accepted, nothing to do --------------------------------------
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        case RETRO_ENVIRONMENT_SET_ROTATION:
            return true;

        default:
            return false;
        }
    }

    // ---- Static thunks ------------------------------------------------------

    bool RETRO_CALLCONV LibretroCore::EnvironmentThunk(unsigned cmd, void* data)
    {
        return s_active ? s_active->Environment(cmd, data) : false;
    }

    void RETRO_CALLCONV LibretroCore::VideoRefreshThunk(const void* data, unsigned width, unsigned height, size_t pitch)
    {
        auto self = s_active;
        if (!self)
        {
            return;
        }
        if (data == RETRO_HW_FRAME_BUFFER_VALID)
        {
            // Version 1 convention: the frame is the SRV bound at PS slot 0.
            ID3D11ShaderResourceView* srv = nullptr;
            self->m_host.D3DContext()->PSGetShaderResources(0, 1, &srv);
            if (srv)
            {
                self->m_host.OnHardwareFrame(srv, width, height);
                srv->Release();
            }
            return;
        }
        self->m_host.OnSoftwareFrame(data, width, height, pitch, self->m_pixelFormat);
    }

    void RETRO_CALLCONV LibretroCore::AudioSampleThunk(int16_t left, int16_t right)
    {
        if (s_active)
        {
            const int16_t frame[2] = { left, right };
            s_active->m_host.OnAudioFrames(frame, 1);
        }
    }

    size_t RETRO_CALLCONV LibretroCore::AudioSampleBatchThunk(const int16_t* data, size_t frames)
    {
        if (s_active)
        {
            s_active->m_host.OnAudioFrames(data, frames);
        }
        return frames;
    }

    void RETRO_CALLCONV LibretroCore::InputPollThunk()
    {
        if (s_active)
        {
            s_active->m_host.OnInputPoll();
        }
    }

    int16_t RETRO_CALLCONV LibretroCore::InputStateThunk(unsigned port, unsigned device, unsigned index, unsigned id)
    {
        return s_active ? s_active->m_host.OnInputState(port, device, index, id) : 0;
    }

    bool RETRO_CALLCONV LibretroCore::RumbleThunk(unsigned port, retro_rumble_effect effect, uint16_t strength)
    {
        return s_active ? s_active->m_host.OnRumble(port, effect, strength) : false;
    }

    void RETRO_CALLCONV LibretroCore::LogThunk(retro_log_level level, const char* fmt, ...)
    {
        char buffer[1024];
        static const char* prefixes[] = { "[core:debug] ", "[core:info] ", "[core:warn] ", "[core:error] " };
        const char* prefix = (level >= RETRO_LOG_DEBUG && level <= RETRO_LOG_ERROR) ? prefixes[level] : "[core] ";
        va_list args;
        va_start(args, fmt);
        int n = std::snprintf(buffer, sizeof(buffer), "%s", prefix);
        std::vsnprintf(buffer + n, sizeof(buffer) - static_cast<size_t>(n), fmt, args);
        va_end(args);
        OutputDebugStringA(buffer);
    }
}
