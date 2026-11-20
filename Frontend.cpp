#include "pch.h"
#include "Frontend.h"
#include "UI/BladeTheme.h"
#include "UI/imgui_impl_uwp.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"

using namespace winrt;
using namespace winrt::Windows::Gaming::Input;
using namespace winrt::Windows::Storage;

namespace rfx
{
    namespace
    {
        constexpr float kSurfSeconds = 1.2f;
        constexpr size_t kMaxChannels = 12;

        std::string Utf8(std::wstring_view w) { return winrt::to_string(w); }
        std::wstring Wide(std::string_view s) { return std::wstring{ winrt::to_hstring(s) }; }

        std::wstring Sanitize(std::wstring s)
        {
            for (auto& c : s)
            {
                if (wcschr(L"\\/:*?\"<>|", c) || c < 32) c = L'_';
            }
            return s;
        }

        bool ReadFileBytes(std::wstring const& path, std::vector<uint8_t>& out)
        {
            FILE* f = nullptr;
            if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            out.resize(size > 0 ? static_cast<size_t>(size) : 0);
            size_t read = out.empty() ? 0 : fread(out.data(), 1, out.size(), f);
            fclose(f);
            return read == out.size();
        }

        bool WriteFileBytes(std::wstring const& path, const void* data, size_t size)
        {
            std::wstring tmp = path + L".tmp";
            FILE* f = nullptr;
            if (_wfopen_s(&f, tmp.c_str(), L"wb") != 0 || !f) return false;
            size_t written = size ? fwrite(data, 1, size, f) : 0;
            fclose(f);
            if (written != size) { DeleteFileW(tmp.c_str()); return false; }
            return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        }

        std::vector<std::string> SplitTabs(std::string const& line)
        {
            std::vector<std::string> parts;
            std::string cur;
            for (char c : line)
            {
                if (c == '\t') { parts.push_back(cur); cur.clear(); }
                else if (c != '\r' && c != '\n') cur.push_back(c);
            }
            parts.push_back(cur);
            return parts;
        }

        std::string Fmt(const char* fmt, double v)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), fmt, v);
            return buf;
        }

        float Step(float v, int dir, float step, float lo, float hi)
        {
            return std::clamp(std::round((v + dir * step) / step) * step, lo, hi);
        }
    }

    Frontend::Frontend() = default;

    Frontend::~Frontend()
    {
        Shutdown();
    }

    // ------------------------------------------------------------------ UI thread

    void Frontend::Initialize(winrt::Windows::UI::Xaml::Controls::SwapChainPanel const& panel)
    {
        m_device.CreateDeviceAndSwapChain(panel);

        m_localDir = std::wstring{ ApplicationData::Current().LocalFolder().Path() };
        m_savesDir = m_localDir + L"\\saves";
        m_statesDir = m_localDir + L"\\states";
        m_systemDir = m_localDir + L"\\system";
        for (auto const* dir : { &m_savesDir, &m_statesDir, &m_systemDir })
        {
            CreateDirectoryW(dir->c_str(), nullptr);
        }
        CreateDirectoryW((m_localDir + L"\\roms").c_str(), nullptr);
    }

    void Frontend::OnPanelMetricsChanged(PanelMetrics const& metrics)
    {
        if (metrics.dipWidth > 0.0f && metrics.outputWidth > 0)
        {
            m_pointerScale = static_cast<float>(metrics.outputWidth) / metrics.dipWidth;
        }
        m_device.SetPendingMetrics(metrics);
    }

    void Frontend::Start()
    {
        if (m_running.exchange(true))
        {
            return;
        }
        m_renderThread = std::thread([this]()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            RenderLoop();
            winrt::uninit_apartment();
        });
    }

    void Frontend::Shutdown()
    {
        if (m_running.exchange(false))
        {
            m_suspendRequested = false;
            m_suspendCv.notify_all();
            if (m_renderThread.joinable())
            {
                m_renderThread.join();
            }
        }
        std::vector<std::thread> workers;
        {
            std::lock_guard lock(m_workerMutex);
            workers.swap(m_workers);
        }
        for (auto& t : workers)
        {
            if (t.joinable()) t.join();
        }
    }

    void Frontend::OnRomFolderPicked(StorageFolder const& folder)
    {
        m_library.AddFolder(folder);
        m_rescanRequested = true;
    }

    void Frontend::Suspend()
    {
        if (!m_running)
        {
            return;
        }
        std::unique_lock lock(m_suspendMutex);
        m_suspendRequested = true;
        // Give the render thread a moment to flush SRAM and the auto state.
        m_suspendCv.wait_for(lock, std::chrono::seconds(3), [this] { return m_suspended.load() || !m_running; });
    }

    void Frontend::Resume()
    {
        {
            std::lock_guard lock(m_suspendMutex);
            m_suspendRequested = false;
        }
        m_suspendCv.notify_all();
    }

    void Frontend::RunWorker(std::function<void()> fn)
    {
        std::lock_guard lock(m_workerMutex);
        m_workers.emplace_back([fn = std::move(fn)]()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            try { fn(); } catch (...) {}
            winrt::uninit_apartment();
        });
    }

    // ------------------------------------------------------------------ Render thread

    void Frontend::RenderLoop()
    {
        m_device.ApplyPendingResize();
        m_post.Initialize(m_device.Device(), m_device.Context());
        m_post.Resize(m_device.Width(), m_device.Height());
        m_audio.Initialize();
        InitImGui();
        LoadSettings();
        LoadChannels();
        BuildMenu();
        StartScan(true);

        auto last = std::chrono::steady_clock::now();
        while (m_running)
        {
            if (m_suspendRequested)
            {
                if (!m_suspended)
                {
                    if (m_core && m_core->IsGameLoaded())
                    {
                        WriteSram();
                        SaveState(L"auto");
                    }
                    SaveChannels();
                    SaveSettings();
                    m_audio.SetPaused(true);
                    m_input.StopRumble();
                    m_device.Trim();
                    {
                        std::lock_guard lock(m_suspendMutex);
                        m_suspended = true;
                    }
                    m_suspendCv.notify_all();
                }
                std::unique_lock lock(m_suspendMutex);
                m_suspendCv.wait_for(lock, std::chrono::milliseconds(100), [this] { return !m_suspendRequested || !m_running; });
                last = std::chrono::steady_clock::now();
                continue;
            }
            if (m_suspended)
            {
                m_suspended = false;
                m_audio.SetPaused(m_mode != Mode::Playing);
            }

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            dt = std::clamp(dt, 0.0f, 0.25f);
            Frame(dt);
        }

        // Shutdown on the render thread: the core and ImGui own D3D objects.
        CloseGame(true);
        SaveChannels();
        SaveSettings();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplUwp_Shutdown();
        ImGui::DestroyContext();
        m_audio.Shutdown();
    }

    void Frontend::InitImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        // Segoe UI exists on Windows desktop; on Xbox the read may fail, so fall
        // back to ImGui's built-in font (scales fine thanks to dynamic fonts).
        const wchar_t* segoe = L"C:\\Windows\\Fonts\\segoeui.ttf";
        if (GetFileAttributesW(segoe) != INVALID_FILE_ATTRIBUTES)
        {
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f);
        }
        if (io.Fonts->Fonts.empty())
        {
            io.Fonts->AddFontDefault();
        }

        ImGui_ImplUwp_Init();
        ImGui_ImplDX11_Init(m_device.Device(), m_device.Context());
        ApplyUiScale();
    }

    void Frontend::ApplyUiScale()
    {
        const uint32_t h = std::max<uint32_t>(m_device.Height(), 1);
        if (h == m_uiHeight)
        {
            return;
        }
        m_uiHeight = h;
        m_uiScale = static_cast<float>(h) / 1080.0f;

        ImGuiStyle style;
        ImGui::StyleColorsDark(&style);
        style.ScaleAllSizes(m_uiScale);
        style.FontSizeBase = 22.0f * m_uiScale;
        style._NextFrameFontSizeBase = style.FontSizeBase;
        ImGui::GetStyle() = style;
        m_menu.SetScale(m_uiScale);
    }

    void Frontend::Frame(float dt)
    {
        m_time += dt;

        if (m_device.ApplyPendingResize())
        {
            m_post.Resize(m_device.Width(), m_device.Height());
            ApplyUiScale();
        }

        if (m_rescanRequested.exchange(false))
        {
            StartScan(false);
        }
        if (m_library.Revision() != m_libraryRevisionSeen)
        {
            m_libraryRevisionSeen = m_library.Revision();
            m_cores = m_library.Cores();
            m_games = m_library.Games();
            RefreshDynamicBlades();
        }

        m_input.Poll();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplUwp_NewFrame(static_cast<float>(m_device.Width()), static_cast<float>(m_device.Height()), m_pointerScale, false);
        ImGui::NewFrame();

        const bool gameLoaded = m_core && m_core->IsGameLoaded();

        // ---- Global hotkeys
        if (!m_surf.active)
        {
            const bool view = m_input.Held(GamepadButtons::View);
            const bool toggle = m_toggleMenuRequested.exchange(false) || (view && m_input.Pressed(GamepadButtons::Menu));
            if (toggle && gameLoaded)
            {
                m_mode = (m_mode == Mode::Playing) ? Mode::Menu : Mode::Playing;
                if (m_mode == Mode::Menu)
                {
                    m_menu.SetActiveBlade(static_cast<int>(m_bladeNowPlaying));
                    m_menu.NavigateDown();
                }
                m_audio.SetPaused(m_mode != Mode::Playing);
                m_input.SetSuppressed(m_mode != Mode::Playing);
                m_input.StopRumble();
            }

            const bool surfCombo = m_mode == Mode::Dashboard || view;
            if (surfCombo && m_input.Pressed(GamepadButtons::RightShoulder)) SurfChannel(+1);
            else if (surfCombo && m_input.Pressed(GamepadButtons::LeftShoulder)) SurfChannel(-1);
        }

        if (!m_surf.active && m_mode != Mode::Playing)
        {
            HandleMenuInput(dt);
        }

        // ---- Emulation
        if (gameLoaded && m_mode == Mode::Playing && !m_surf.active)
        {
            double fps = m_core->AvInfo().timing.fps;
            if (fps < 10.0 || fps > 240.0) fps = 60.0;
            const double frameTime = 1.0 / fps;
            m_frameAccumulator += dt;
            int runs = 0;
            while (m_frameAccumulator >= frameTime * 0.98 && runs < 3)
            {
                m_core->RunFrame();
                m_frameAccumulator -= frameTime;
                ++runs;
            }
            if (m_frameAccumulator > 0.1 || m_frameAccumulator < -0.1) m_frameAccumulator = 0.0;
            m_audio.EndFrame();
        }

        // ---- Composition
        m_post.RenderScene(m_core ? m_core->AspectRatio() : 0.0f, m_crt);
        UpdateSurf(dt);
        const bool useSnapshot = m_surf.active && m_surf.progress < 0.5f;
        m_post.RenderFinal(m_device.BackBufferRtv(), useSnapshot, m_surf.progress, static_cast<float>(m_time), m_surf.active);

        if (!m_surf.active && m_mode != Mode::Playing)
        {
            DrawDashboardBackground();
            m_menu.Update(dt);
            m_menu.Render();
        }
        DrawOverlay();

        ImGui::Render();
        ID3D11RenderTargetView* rtv = m_device.BackBufferRtv();
        m_device.Context()->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_device.Present();
    }

    // ------------------------------------------------------------------ Channel surf / launch

    void Frontend::BeginLaunch(Channel const& channel, bool resumeAutoState)
    {
        auto launch = std::make_shared<PendingLaunch>();
        launch->channel = channel;
        launch->resumeAutoState = resumeAutoState;
        StartLoadWorker(launch);
        BeginSurf(launch);
    }

    void Frontend::BeginSurf(std::shared_ptr<PendingLaunch> launch)
    {
        if (m_surf.active)
        {
            return;
        }
        m_post.CaptureSnapshot();
        m_surf = {};
        m_surf.active = true;
        m_surf.launch = std::move(launch);
        m_audio.SetPaused(true);
        m_input.StopRumble();
    }

    void Frontend::StartLoadWorker(std::shared_ptr<PendingLaunch> const& launch)
    {
        bool needFullPath = false;
        for (auto const& c : m_cores)
        {
            if (c.packagePath == launch->channel.corePath) needFullPath = c.needFullPath;
        }
        StorageFile known{ nullptr };
        for (auto const& g : m_games)
        {
            if (g.path == launch->channel.gamePath) { known = g.file; break; }
        }

        RunWorker([launch, needFullPath, known]()
        {
            StorageFile file = known;
            if (!file)
            {
                try { file = StorageFile::GetFileFromPathAsync(launch->channel.gamePath).get(); }
                catch (...) {}
            }
            if (!file)
            {
                launch->failed = true;
                launch->error = "Can't open " + Utf8(launch->channel.gamePath);
            }
            else if (needFullPath)
            {
                launch->readablePath = RomLibrary::EnsureDirectlyReadable(file);
            }
            else
            {
                launch->readablePath = launch->channel.gamePath;
                if (!RomLibrary::ReadAllBytes(file, launch->data))
                {
                    launch->failed = true;
                    launch->error = "Read failed: " + Utf8(launch->channel.gamePath);
                }
            }
            launch->ready = true;
        });
    }

    void Frontend::UpdateSurf(float dt)
    {
        if (!m_surf.active)
        {
            return;
        }
        m_surf.progress = std::min(m_surf.progress + dt / kSurfSeconds, 1.0f);

        if (!m_surf.launched && m_surf.progress >= 0.5f)
        {
            if (m_surf.launch && !m_surf.launch->ready)
            {
                m_surf.progress = 0.5f; // hold on static while the ROM streams in
                return;
            }

            CloseGame(true);
            m_surf.launched = true;
            if (!m_surf.launch)
            {
                m_mode = Mode::Dashboard;
            }
            else if (m_surf.launch->failed || !CompleteLaunch(*m_surf.launch))
            {
                if (m_surf.launch->failed) Toast(m_surf.launch->error, 5.0f);
                m_mode = Mode::Dashboard;
            }
            else
            {
                m_mode = Mode::Playing;
            }
            m_surf.launch.reset();
            RefreshDynamicBlades();
        }

        if (m_surf.progress >= 1.0f)
        {
            m_surf = {};
            m_audio.SetPaused(m_mode != Mode::Playing);
            m_input.SetSuppressed(m_mode != Mode::Playing);
            if (m_mode == Mode::Dashboard)
            {
                m_menu.SetActiveBlade(static_cast<int>(m_bladeGames));
            }
        }
    }

    bool Frontend::CompleteLaunch(PendingLaunch& launch)
    {
        m_core = std::make_unique<LibretroCore>(*this);
        CoreDirectories dirs{ Utf8(m_systemDir), Utf8(m_savesDir) };
        std::string error;
        if (!m_core->Load(launch.channel.corePath, dirs, error) ||
            !m_core->LoadGame(Utf8(launch.readablePath), std::move(launch.data), error))
        {
            Toast(error.empty() ? "Core failed to start" : error, 6.0f);
            m_core.reset();
            m_post.ClearFrame();
            return false;
        }

        m_current = launch.channel;
        if (m_current.coreName.empty()) m_current.coreName = m_core->LibraryName();
        ReadSram();
        m_audio.Configure(m_core->AvInfo().timing.sample_rate);
        if (launch.resumeAutoState && LoadState(L"auto"))
        {
            Toast("Resumed where you left off", 2.0f);
        }
        m_frameAccumulator = 0.0;

        int chNo = 0;
        for (size_t i = 0; i < m_channels.size(); ++i)
        {
            if (m_channels[i].gamePath == m_current.gamePath && m_channels[i].corePath == m_current.corePath)
            {
                chNo = static_cast<int>(i) + 1;
                m_channelIndex = static_cast<int>(i);
            }
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "CH %02d", chNo);
        m_banner = std::string(buf) + "   " + m_current.gameName + "   |   " + m_current.coreName;
        m_bannerTime = 4.0f;
        return true;
    }

    void Frontend::CloseGame(bool writeAutoState)
    {
        if (m_core && m_core->IsGameLoaded())
        {
            WriteSram();
            if (writeAutoState) SaveState(L"auto");
        }
        m_core.reset(); // retro_unload_game + retro_deinit + FreeLibrary
        m_post.ClearFrame();
        m_audio.Flush();
        m_input.StopRumble();
        m_current = {};
    }

    void Frontend::SurfChannel(int direction)
    {
        if (m_channels.empty())
        {
            Toast("No channels yet - launch a game from GAMES first");
            return;
        }
        const int n = static_cast<int>(m_channels.size());
        int idx = m_channelIndex < 0 ? (direction > 0 ? 0 : n - 1) : ((m_channelIndex + direction) % n + n) % n;
        if (n == 1 && m_core && m_core->IsGameLoaded())
        {
            Toast("Only one channel - launch another game to surf");
            return;
        }
        m_channelIndex = idx;
        BeginLaunch(m_channels[idx], true);
    }

    // ------------------------------------------------------------------ Persistence

    std::wstring Frontend::SaveBaseName() const
    {
        std::wstring core = m_current.corePath;
        auto slash = core.find_last_of(L"\\/");
        if (slash != std::wstring::npos) core = core.substr(slash + 1);
        if (core.size() > 4) core.resize(core.size() - 4);
        return Sanitize(Wide(m_current.gameName)) + L"." + Sanitize(core);
    }

    void Frontend::WriteSram()
    {
        std::vector<uint8_t> data;
        if (m_core && m_core->ReadSaveRam(data) && !data.empty())
        {
            WriteFileBytes(m_savesDir + L"\\" + SaveBaseName() + L".srm", data.data(), data.size());
        }
    }

    void Frontend::ReadSram()
    {
        std::vector<uint8_t> data;
        if (m_core && ReadFileBytes(m_savesDir + L"\\" + SaveBaseName() + L".srm", data) && !data.empty())
        {
            m_core->WriteSaveRam(data);
        }
    }

    bool Frontend::SaveState(std::wstring const& suffix)
    {
        std::vector<uint8_t> data;
        if (!m_core || !m_core->Serialize(data) || data.empty())
        {
            return false;
        }
        return WriteFileBytes(m_statesDir + L"\\" + SaveBaseName() + L"." + suffix + L".state", data.data(), data.size());
    }

    bool Frontend::LoadState(std::wstring const& suffix)
    {
        std::vector<uint8_t> data;
        if (!m_core || !ReadFileBytes(m_statesDir + L"\\" + SaveBaseName() + L"." + suffix + L".state", data) || data.empty())
        {
            return false;
        }
        return m_core->Unserialize(data);
    }

    void Frontend::LoadChannels()
    {
        std::vector<uint8_t> raw;
        if (!ReadFileBytes(m_localDir + L"\\channels.txt", raw)) return;
        std::string text(raw.begin(), raw.end());
        size_t pos = 0;
        while (pos < text.size() && m_channels.size() < kMaxChannels)
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            auto parts = SplitTabs(text.substr(pos, end - pos));
            pos = end + 1;
            if (parts.size() >= 4 && !parts[0].empty() && !parts[1].empty())
            {
                m_channels.push_back({ Wide(parts[0]), Wide(parts[1]), parts[2], parts[3] });
            }
        }
    }

    void Frontend::SaveChannels()
    {
        std::string text;
        for (auto const& ch : m_channels)
        {
            text += Utf8(ch.corePath) + "\t" + Utf8(ch.gamePath) + "\t" + ch.coreName + "\t" + ch.gameName + "\n";
        }
        WriteFileBytes(m_localDir + L"\\channels.txt", text.data(), text.size());
    }

    void Frontend::AddChannelToFront(Channel const& ch)
    {
        m_channels.erase(std::remove_if(m_channels.begin(), m_channels.end(), [&](Channel const& c)
        {
            return c.gamePath == ch.gamePath && c.corePath == ch.corePath;
        }), m_channels.end());
        m_channels.insert(m_channels.begin(), ch);
        if (m_channels.size() > kMaxChannels) m_channels.resize(kMaxChannels);
        m_channelIndex = 0;
        SaveChannels();
    }

    void Frontend::LoadSettings()
    {
        std::vector<uint8_t> raw;
        if (!ReadFileBytes(m_localDir + L"\\display.txt", raw)) return;
        std::string text(raw.begin(), raw.end());
        auto get = [&](const char* key, float& v)
        {
            auto p = text.find(std::string(key) + "=");
            if (p != std::string::npos) v = static_cast<float>(std::atof(text.c_str() + p + std::strlen(key) + 1));
        };
        float enabled = m_crt.enabled ? 1.0f : 0.0f, integer = m_crt.integerScale ? 1.0f : 0.0f;
        get("crt", enabled);
        get("scanlines", m_crt.scanlineStrength);
        get("mask", m_crt.maskStrength);
        get("bloom", m_crt.bloom);
        get("curvature", m_crt.curvature);
        get("maskpitch", m_crt.maskPitch);
        get("integer", integer);
        m_crt.enabled = enabled > 0.5f;
        m_crt.integerScale = integer > 0.5f;
    }

    void Frontend::SaveSettings()
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "crt=%d\nscanlines=%.3f\nmask=%.3f\nbloom=%.3f\ncurvature=%.4f\nmaskpitch=%.1f\ninteger=%d\n",
            m_crt.enabled ? 1 : 0, m_crt.scanlineStrength, m_crt.maskStrength, m_crt.bloom, m_crt.curvature, m_crt.maskPitch, m_crt.integerScale ? 1 : 0);
        WriteFileBytes(m_localDir + L"\\display.txt", buf, std::strlen(buf));
    }

    // ------------------------------------------------------------------ Dashboard

    void Frontend::StartScan(bool cores)
    {
        if (m_library.IsScanning())
        {
            return;
        }
        RunWorker([this, cores]()
        {
            if (cores) m_library.ScanCores();
            m_library.ScanGames();
        });
    }

    void Frontend::BuildMenu()
    {
        m_bladeNowPlaying = 0;
        m_menu.AddBlade(Blade{ "NOW PLAYING", {}, "Nothing is playing. Pick something from GAMES." });
        m_bladeGames = 1;
        m_menu.AddBlade(Blade{ "GAMES", {}, "Scanning..." });
        m_bladeChannels = 2;
        m_menu.AddBlade(Blade{ "CHANNELS", {}, "Recently played games appear here. LB/RB surfs between them." });
        m_bladeSystems = 3;
        m_menu.AddBlade(Blade{ "SYSTEMS", {}, "No cores found. Put libretro *.dll files in the package Cores\\ folder." });
        m_bladeDisplay = 4;
        m_menu.AddBlade(Blade{ "DISPLAY", {}, "" });
        m_bladeSystem = 5;
        m_menu.AddBlade(Blade{ "SYSTEM", {}, "" });
        m_menu.SetScale(m_uiScale);
        m_menu.SetActiveBlade(static_cast<int>(m_bladeGames));
        RefreshDynamicBlades();
    }

    void Frontend::RefreshDynamicBlades()
    {

        // NOW PLAYING
        std::vector<BladeSubItem> now;
        if (m_core && m_core->IsGameLoaded())
        {
            now.push_back({ "Resume  " + m_current.gameName, [this] { m_mode = Mode::Playing; m_audio.SetPaused(false); m_input.SetSuppressed(false); } });
            now.push_back({ "Save state", [this] { Toast(SaveState(L"slot0") ? "State saved" : "This core can't save states"); } });
            now.push_back({ "Load state", [this] { Toast(LoadState(L"slot0") ? "State loaded" : "No saved state"); } });
            now.push_back({ "Reset", [this] { m_core->Reset(); m_mode = Mode::Playing; m_audio.SetPaused(false); m_input.SetSuppressed(false); } });
            now.push_back({ "Close game", [this] { BeginSurf(nullptr); } });
            now.back().detail = "progress is auto-saved";
        }
        m_menu.SetItems(m_bladeNowPlaying, std::move(now));

        // GAMES
        std::vector<BladeSubItem> games;
        games.reserve(m_games.size());
        for (size_t i = 0; i < m_games.size(); ++i)
        {
            BladeSubItem item;
            item.label = m_games[i].displayName;
            item.value = [this, i]() -> std::string
            {
                if (i >= m_games.size() || m_games[i].coreIndices.empty()) return {};
                auto const& g = m_games[i];
                size_t choice = m_coreChoice.count(g.path) ? m_coreChoice[g.path] : 0;
                int ci = g.coreIndices[choice % g.coreIndices.size()];
                return ci < static_cast<int>(m_cores.size()) ? m_cores[ci].name : std::string{};
            };
            if (m_games[i].coreIndices.size() > 1)
            {
                item.onAdjust = [this, i](int dir)
                {
                    auto const& g = m_games[i];
                    size_t n = g.coreIndices.size();
                    size_t& choice = m_coreChoice[g.path];
                    choice = (choice + (dir > 0 ? 1 : n - 1)) % n;
                };
            }
            item.onActivate = [this, i]()
            {
                if (i >= m_games.size()) return;
                auto const& g = m_games[i];
                size_t choice = m_coreChoice.count(g.path) ? m_coreChoice[g.path] % g.coreIndices.size() : 0;
                auto const& core = m_cores[g.coreIndices[choice]];
                Channel ch{ core.packagePath, g.path, core.name, g.displayName };
                AddChannelToFront(ch);
                BeginLaunch(ch, false);
            };
            games.push_back(std::move(item));
        }
        m_menu.SetItems(m_bladeGames, std::move(games));

        // CHANNELS
        std::vector<BladeSubItem> channels;
        for (size_t i = 0; i < m_channels.size(); ++i)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "CH %02zu   ", i + 1);
            BladeSubItem item{ buf + m_channels[i].gameName, [this, i]()
            {
                if (i >= m_channels.size()) return;
                m_channelIndex = static_cast<int>(i);
                BeginLaunch(m_channels[i], true);
            } };
            item.detail = m_channels[i].coreName;
            channels.push_back(std::move(item));
        }
        if (!m_channels.empty())
        {
            channels.push_back({ "Clear channel list", [this] { m_channels.clear(); m_channelIndex = -1; SaveChannels(); RefreshDynamicBlades(); } });
        }
        m_menu.SetItems(m_bladeChannels, std::move(channels));

        // SYSTEMS
        std::vector<BladeSubItem> systems;
        for (size_t c = 0; c < m_cores.size(); ++c)
        {
            size_t count = 0;
            for (auto const& g : m_games)
                for (int ci : g.coreIndices)
                    if (ci == static_cast<int>(c)) ++count;
            BladeSubItem item;
            item.label = m_cores[c].name + "  " + m_cores[c].version;
            item.detail = std::to_string(count) + (count == 1 ? " game" : " games");
            systems.push_back(std::move(item));
        }
        m_menu.SetItems(m_bladeSystems, std::move(systems));

        // DISPLAY
        std::vector<BladeSubItem> display;
        auto onOff = [](bool b) { return std::string(b ? "On" : "Off"); };
        auto pct = [](float v) { return Fmt("%.0f%%", v * 100.0); };
        display.push_back({ "CRT aperture grille", [this] { m_crt.enabled = !m_crt.enabled; SaveSettings(); },
            [this, onOff] { return onOff(m_crt.enabled); }, [this](int) { m_crt.enabled = !m_crt.enabled; SaveSettings(); } });
        display.push_back({ "Scanlines", nullptr, [this, pct] { return pct(m_crt.scanlineStrength); },
            [this](int d) { m_crt.scanlineStrength = Step(m_crt.scanlineStrength, d, 0.05f, 0.0f, 1.0f); SaveSettings(); } });
        display.push_back({ "Phosphor mask", nullptr, [this, pct] { return pct(m_crt.maskStrength); },
            [this](int d) { m_crt.maskStrength = Step(m_crt.maskStrength, d, 0.05f, 0.0f, 1.0f); SaveSettings(); } });
        display.push_back({ "Mask pitch", nullptr, [this] { return Fmt("%.0f px", m_crt.maskPitch); },
            [this](int d) { m_crt.maskPitch = Step(m_crt.maskPitch, d, 1.0f, 2.0f, 6.0f); SaveSettings(); } });
        display.push_back({ "Bloom", nullptr, [this, pct] { return pct(m_crt.bloom); },
            [this](int d) { m_crt.bloom = Step(m_crt.bloom, d, 0.05f, 0.0f, 1.0f); SaveSettings(); } });
        display.push_back({ "Screen curvature", nullptr, [this] { return Fmt("%.3f", m_crt.curvature); },
            [this](int d) { m_crt.curvature = Step(m_crt.curvature, d, 0.005f, 0.0f, 0.1f); SaveSettings(); } });
        display.push_back({ "Integer scaling", [this] { m_crt.integerScale = !m_crt.integerScale; SaveSettings(); },
            [this, onOff] { return onOff(m_crt.integerScale); }, [this](int) { m_crt.integerScale = !m_crt.integerScale; SaveSettings(); } });
        display.push_back({ "Reset to defaults", [this] { m_crt = CrtSettings{}; SaveSettings(); } });
        display.push_back({ "Output", nullptr, [this] { return std::to_string(m_device.Width()) + " x " + std::to_string(m_device.Height()); } });
        m_menu.SetItems(m_bladeDisplay, std::move(display));

        // SYSTEM
        std::vector<BladeSubItem> sys;
        sys.push_back({ "Add ROM folder (USB drive)...", [this] { if (m_requestFolderPicker) m_requestFolderPicker(); } });
        sys.push_back({ "Rescan games", [this] { StartScan(false); Toast("Scanning for games..."); } });
        sys.push_back({ "Rescan cores", [this] { StartScan(true); Toast("Scanning cores..."); } });
        sys.push_back({ "Library", nullptr, [this]
        {
            return std::to_string(m_games.size()) + " games / " + std::to_string(m_cores.size()) + " cores" + (m_library.IsScanning() ? " (scanning)" : "");
        } });
        for (auto const& src : m_library.SourceFolders())
        {
            BladeSubItem item;
            item.label = "  " + Utf8(src);
            sys.push_back(std::move(item));
        }
        sys.push_back({ "Tip: E:\\roms etc. need File system access", nullptr });
        sys.back().detail = "Settings > Privacy";
        m_menu.SetItems(m_bladeSystem, std::move(sys));
    }

    void Frontend::HandleMenuInput(float dt)
    {
        auto anyPad = [this](auto pred)
        {
            for (size_t i = 0; i < InputMapper::kMaxPads; ++i)
                if (m_input.Pad(i).connected && pred(m_input.Pad(i))) return true;
            return false;
        };
        const bool dirs[4] = {
            m_input.Held(GamepadButtons::DPadUp) || anyPad([](PadState const& p) { return p.leftY > 0.6; }) || ImGui::IsKeyDown(ImGuiKey_UpArrow),
            m_input.Held(GamepadButtons::DPadDown) || anyPad([](PadState const& p) { return p.leftY < -0.6; }) || ImGui::IsKeyDown(ImGuiKey_DownArrow),
            m_input.Held(GamepadButtons::DPadLeft) || anyPad([](PadState const& p) { return p.leftX < -0.6; }) || ImGui::IsKeyDown(ImGuiKey_LeftArrow),
            m_input.Held(GamepadButtons::DPadRight) || anyPad([](PadState const& p) { return p.leftX > 0.6; }) || ImGui::IsKeyDown(ImGuiKey_RightArrow),
        };

        for (int d = 0; d < 4; ++d)
        {
            auto& r = m_repeat[d];
            bool fire = false;
            if (!dirs[d]) { r.held = 0.0f; r.next = 0.0f; continue; }
            if (r.held == 0.0f) { fire = true; r.next = 0.40f; }
            r.held += dt;
            if (r.held >= r.next) { fire = true; r.next = r.held + 0.07f; }
            if (!fire) continue;
            switch (d)
            {
            case 0: m_menu.NavigateUp(); break;
            case 1: m_menu.NavigateDown(); break;
            case 2: m_menu.NavigateLeft(); break;
            case 3: m_menu.NavigateRight(); break;
            }
        }

        static float pageCooldown = 0.0f;
        pageCooldown = std::max(0.0f, pageCooldown - dt);
        if (pageCooldown == 0.0f)
        {
            if (anyPad([](PadState const& p) { return p.rightTrigger > 0.6; }) || ImGui::IsKeyPressed(ImGuiKey_PageDown)) { m_menu.PageDown(); pageCooldown = 0.15f; }
            else if (anyPad([](PadState const& p) { return p.leftTrigger > 0.6; }) || ImGui::IsKeyPressed(ImGuiKey_PageUp)) { m_menu.PageUp(); pageCooldown = 0.15f; }
        }

        if (m_input.Pressed(GamepadButtons::A) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false))
        {
            m_menu.Activate();
        }
        else if (m_input.Pressed(GamepadButtons::B) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        {
            if (m_menu.IsInSubmenu())
            {
                m_menu.Back();
            }
            else if (m_mode == Mode::Menu)
            {
                m_mode = Mode::Playing;
                m_audio.SetPaused(false);
                m_input.SetSuppressed(false);
            }
        }
        else if (m_mode == Mode::Menu && !m_input.Held(GamepadButtons::View) && m_input.Pressed(GamepadButtons::Menu))
        {
            m_mode = Mode::Playing;
            m_audio.SetPaused(false);
            m_input.SetSuppressed(false);
        }
    }

    void Frontend::DrawDashboardBackground()
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        if (m_mode == Mode::Menu)
        {
            dl->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 8, 0, 190));
            return;
        }
        dl->AddRectFilledMultiColor(ImVec2(0, 0), size,
            IM_COL32(6, 22, 8, 255), IM_COL32(4, 14, 6, 255), IM_COL32(0, 4, 0, 255), IM_COL32(2, 10, 4, 255));
        // Soft horizontal "orb" glow behind the blade bar.
        const float s = m_uiScale;
        const float cy = (90.0f + BladeTheme::BladeBarHeight * 0.5f) * s;
        for (int i = 0; i < 12; ++i)
        {
            const float r = (900.0f - i * 60.0f) * s;
            dl->AddCircleFilled(ImVec2(size.x * 0.5f, cy), r, IM_COL32(40, 140, 40, 4 + i), 96);
        }
        const char* title = "RETROFORGE X";
        const float fs = ImGui::GetFontSize() * 0.9f;
        dl->AddText(ImGui::GetFont(), fs, ImVec2(48.0f * s, 30.0f * s), IM_COL32(120, 220, 120, 200), title);
    }

    void Frontend::DrawOverlay()
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        const float s = m_uiScale;
        const float dt = ImGui::GetIO().DeltaTime;

        // Channel banner (top-left, old-TV OSD green)
        if (m_bannerTime > 0.0f && !m_surf.active)
        {
            m_bannerTime -= dt;
            const float a = std::clamp(m_bannerTime, 0.0f, 1.0f);
            const float fs = ImGui::GetFontSize() * 1.6f;
            const ImVec2 pos(80.0f * s, 70.0f * s);
            dl->AddText(ImGui::GetFont(), fs, ImVec2(pos.x + 3 * s, pos.y + 3 * s), IM_COL32(0, 0, 0, static_cast<int>(200 * a)), m_banner.c_str());
            dl->AddText(ImGui::GetFont(), fs, pos, IM_COL32(110, 255, 110, static_cast<int>(255 * a)), m_banner.c_str());
        }
        if (m_surf.active && m_surf.progress > 0.36f && m_surf.progress < 0.64f)
        {
            const char* label = (m_surf.launch && !m_surf.launch->ready) ? "TUNING..." : "";
            const float fs = ImGui::GetFontSize() * 2.0f;
            dl->AddText(ImGui::GetFont(), fs, ImVec2(80.0f * s, 70.0f * s), IM_COL32(110, 255, 110, 230), label);
        }

        // Toast (bottom-center)
        if (m_toastTime > 0.0f)
        {
            m_toastTime -= dt;
            const float a = std::clamp(m_toastTime * 2.0f, 0.0f, 1.0f);
            const ImVec2 ts = ImGui::CalcTextSize(m_toast.c_str());
            const float pad = 18.0f * s;
            const ImVec2 p0((size.x - ts.x) * 0.5f - pad, size.y - 180.0f * s - pad);
            const ImVec2 p1((size.x + ts.x) * 0.5f + pad, size.y - 180.0f * s + ts.y + pad);
            dl->AddRectFilled(p0, p1, IM_COL32(8, 30, 10, static_cast<int>(230 * a)), 8.0f * s);
            dl->AddRect(p0, p1, IM_COL32(90, 220, 90, static_cast<int>(255 * a)), 8.0f * s, 0, 2.0f * s);
            dl->AddText(ImVec2(p0.x + pad, p0.y + pad), IM_COL32(230, 255, 230, static_cast<int>(255 * a)), m_toast.c_str());
        }

        // Button hints
        if (!m_surf.active && m_mode != Mode::Playing)
        {
            const char* hint = m_mode == Mode::Menu
                ? "(A) Select    (B) Back / Resume    View+LB/RB  Channel Surf"
                : "(A) Select    (B) Back    <> Adjust    LB/RB  Channel Surf    LT/RT  Page";
            const ImVec2 ts = ImGui::CalcTextSize(hint);
            dl->AddText(ImVec2((size.x - ts.x) * 0.5f, size.y - 80.0f * s), IM_COL32(150, 200, 150, 200), hint);
        }
    }

    void Frontend::Toast(std::string text, float seconds)
    {
        m_toast = std::move(text);
        m_toastTime = seconds;
    }

    // ------------------------------------------------------------------ ICoreHost

    void Frontend::OnSoftwareFrame(const void* data, unsigned width, unsigned height, size_t pitch, retro_pixel_format format)
    {
        if (data)
        {
            m_post.UploadSoftwareFrame(data, width, height, pitch, format);
        }
        // data == nullptr: dupe frame, keep the previous one.
    }

    void Frontend::OnHardwareFrame(ID3D11ShaderResourceView* srv, unsigned width, unsigned height)
    {
        m_post.SetHardwareFrame(srv, width, height);
    }

    void Frontend::OnAudioFrames(const int16_t* interleavedStereo, size_t frames)
    {
        m_audio.Push(interleavedStereo, frames);
    }

    void Frontend::OnInputPoll()
    {
        // Pads are polled once per host frame in Frame(); polling again here
        // would break button edge detection for the hotkeys.
    }

    int16_t Frontend::OnInputState(unsigned port, unsigned device, unsigned index, unsigned id)
    {
        return m_input.State(port, device, index, id);
    }

    bool Frontend::OnRumble(unsigned port, retro_rumble_effect effect, uint16_t strength)
    {
        return m_input.Rumble(port, effect, strength);
    }

    void Frontend::OnCoreMessage(std::string const& text, unsigned frames)
    {
        Toast(text, std::clamp(frames / 60.0f, 1.5f, 8.0f));
    }

    void Frontend::OnAvInfoChanged()
    {
        if (m_core)
        {
            m_audio.Configure(m_core->AvInfo().timing.sample_rate);
        }
    }
}
