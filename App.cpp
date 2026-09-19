// =============================================================================
// App.cpp
//
// RetroForgeX's IFrameworkView -- the actual UWP app entry point that owns
// the CoreWindow and wires every engine subsystem together: D3D12VideoDriver
// (rendering, including the ImGui overlay), XAudio2AudioDriver, WgiInputDriver,
// LibretroHost (core/game lifecycle), VfsStorage, MainLoop (frame pacing),
// and QuickMenu (the core/ROM-picking overlay).
//
// Threading: everything here runs on ONE thread -- the thread CoreApplication
// ::Run gives Run() below, which is also the thread that owns the CoreWindow
// and processes its dispatcher events. That's deliberate, not a simplification
// taken for granted: ImGui's context is not thread-safe (NewFrame/Render must
// happen on the same thread that later consumes the resulting ImDrawData in
// Present), and Windows::Storage::Pickers::FileOpenPicker needs to run on the
// thread associated with the app's view. Splitting emulation onto its own
// dedicated thread (MainLoop::Run, as opposed to the RunOneIteration() this
// file calls directly) is supported by MainLoop for exactly the frame-pacing-
// precision reasons described in its own header, but doing that would also
// require moving ImGui rendering onto that same thread and hopping the
// picker calls over to this one -- a real refactor, not a flag flip -- so
// it's left as a documented option rather than half-wired here.
// =============================================================================
#include "pch.h"

#include <stdexcept>
#include <thread>

#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.System.h>

#include "core/libretro_host.hpp"
#include "core/main_loop.hpp"
#include "video/d3d12_video.hpp"
#include "audio/xaudio2_audio.hpp"
#include "input/wgi_input.hpp"
#include "storage/vfs_storage.hpp"
#include "ui/imgui_uwp_backend.hpp"
#include "ui/quick_menu.hpp"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace rfx;

namespace {

float CurrentDpi()
{
    if (auto display = DisplayInformation::GetForCurrentView())
        return display.LogicalDpi();
    return 96.0f;
}

} // namespace

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    // Construction order matters: LibretroHost holds references to the
    // three drivers, MainLoop and QuickMenu hold references to LibretroHost
    // (and MainLoop), so each dependency must already exist -- see
    // Initialize() below, which builds these in that order.
    std::unique_ptr<D3D12VideoDriver>   video_;
    std::unique_ptr<XAudio2AudioDriver> audio_;
    std::unique_ptr<WgiInputDriver>     input_;
    std::unique_ptr<LibretroHost>       host_;
    std::unique_ptr<VfsStorage>         storage_;
    std::unique_ptr<MainLoop>           main_loop_;
    std::unique_ptr<QuickMenu>          quick_menu_;

    CoreWindow window_{ nullptr };
    winrt::event_token window_key_down_token_{};
    std::atomic<bool> running_{ true };

    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const&)
    {
        video_   = std::make_unique<D3D12VideoDriver>();
        audio_   = std::make_unique<XAudio2AudioDriver>();
        input_   = std::make_unique<WgiInputDriver>();
        host_    = std::make_unique<LibretroHost>(*video_, *audio_, *input_);
        storage_ = std::make_unique<VfsStorage>(*host_);
        main_loop_  = std::make_unique<MainLoop>(*host_, *video_, *audio_);
        quick_menu_ = std::make_unique<QuickMenu>(*host_, *main_loop_, *input_, *storage_);

        input_->Initialize();

        // WgiInputDriver::PollInput (and therefore this callback) fires
        // from within LibretroHost::RunFrame, which -- per this file's own
        // threading note above -- runs on this same thread via
        // MainLoop::RunOneIteration in Run() below. That means it's already
        // safe to touch QuickMenu directly here; no cross-thread handoff
        // like MainLoop::QueueAction is needed the way it is for
        // LibretroHost's load/unload calls.
        input_->SetHotkeyCallback([this](Hotkey key)
        {
            if (key == Hotkey::QuickMenu)
                quick_menu_->Toggle();
        });

        AudioInitParams audio_params{};
        if (!audio_->Initialize(audio_params))
            throw std::runtime_error("XAudio2AudioDriver::Initialize failed");

        // VfsStorage's file I/O wrappers block on WinRT's async Storage APIs
        // via IAsyncOperation::get(), and C++/WinRT hard-asserts
        // (!is_sta_thread(), in Windows.Foundation.h) if that .get() runs on
        // an STA thread -- this app's CoreApplicationView/CoreWindow thread
        // (the one running Initialize() right now) IS an STA, since
        // CoreWindow requires it. So Initialize() has to actually run on a
        // separate, throwaway MTA thread; joined synchronously below since
        // nothing needs this thread responsive yet -- SetWindow/Run haven't
        // started pumping CoreWindow events at this point in the app's
        // lifecycle.
        bool storage_ok = false;
        std::thread storage_init_thread([this, &storage_ok]
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            storage_ok = storage_->Initialize();
            winrt::uninit_apartment();
        });
        storage_init_thread.join();
        if (!storage_ok)
            throw std::runtime_error("VfsStorage::Initialize failed");
    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
        running_.store(false, std::memory_order_relaxed);

        if (window_)
            window_.KeyDown(window_key_down_token_);

        quick_menu_.reset();

        if (video_)
            video_->Shutdown(); // also tears down the ImGui DX12 backend -- see ShutdownImGuiOverlay
        ImGui_ImplUwp_Shutdown();
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();

        if (audio_) audio_->Shutdown();
        if (input_) input_->Shutdown();
    }

    void Run()
    {
        window_.Activate();

        while (running_.load(std::memory_order_relaxed))
        {
            window_.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

            if (quick_menu_->IsOpen())
            {
                ImGui_ImplUwp_NewFrame();
                ImGui_ImplDX12_NewFrame();
                ImGui::NewFrame();
                quick_menu_->Draw();
                ImGui::Render();
                video_->SetImGuiOverlay(ImGui::GetDrawData());
            }
            else
            {
                video_->SetImGuiOverlay(nullptr);
            }

            // Steps retro_run() at the core's pace (a no-op while
            // QuickMenu is open -- Toggle() pauses it) and always presents,
            // so the overlay staged above actually reaches the screen.
            main_loop_->RunOneIteration();
        }
    }

    void SetWindow(CoreWindow const& window)
    {
        window_ = window;

        ImGui::CreateContext();
        ImGui_ImplUwp_Init(window);

        const float dpi = CurrentDpi();
        const float scale = dpi / 96.0f;
        const auto bounds = window.Bounds();

        // Kept alive only for this call: CreateSwapChainForCoreWindow
        // (inside D3D12VideoDriver::Initialize) uses the raw pointer
        // synchronously and doesn't need it to outlive this scope.
        auto window_unknown = window.as<::IUnknown>();

        VideoInitParams video_params{};
        video_params.window.core_window    = window_unknown.get();
        video_params.window.logical_width  = static_cast<uint32_t>(bounds.Width * scale);
        video_params.window.logical_height = static_cast<uint32_t>(bounds.Height * scale);
        video_params.window.dpi            = dpi;
        video_params.enable_vsync          = true;

        if (!video_->Initialize(video_params))
            throw std::runtime_error("D3D12VideoDriver::Initialize failed");
        if (!video_->InitializeImGuiOverlay())
            throw std::runtime_error("D3D12VideoDriver::InitializeImGuiOverlay failed");

        window.SizeChanged({ this, &App::OnWindowSizeChanged });

        // Keyboard fallback for Hotkey::QuickMenu's View+Menu gamepad combo
        // -- handy for testing on a dev PC with a keyboard but no controller
        // attached. F1 doesn't collide with any of ImGui's own navigation
        // keys (Tab/arrows/Enter/Escape), so it's safe to check on every
        // KeyDown regardless of whether ImGui currently wants keyboard
        // focus. This is a second, independent subscriber alongside
        // imgui_uwp_backend's own KeyDown handler -- CoreWindow events
        // support multiple listeners, so both fire normally.
        window_key_down_token_ = window.KeyDown({ this, &App::OnWindowKeyDown });
    }

    void OnWindowKeyDown(CoreWindow const&, KeyEventArgs const& args)
    {
        if (args.VirtualKey() == winrt::Windows::System::VirtualKey::F1)
            quick_menu_->Toggle();
    }

    void OnWindowSizeChanged(CoreWindow const& sender, WindowSizeChangedEventArgs const&)
    {
        const float dpi = CurrentDpi();
        const float scale = dpi / 96.0f;
        const auto bounds = sender.Bounds();
        video_->OnHostWindowResized(
            static_cast<uint32_t>(bounds.Width * scale),
            static_cast<uint32_t>(bounds.Height * scale),
            dpi);
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
}
