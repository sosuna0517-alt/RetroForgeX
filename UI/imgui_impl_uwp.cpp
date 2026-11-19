#include "pch.h"
#include "imgui_impl_uwp.h"

using namespace winrt;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Gaming::Input;

namespace
{
    struct InputEvent
    {
        enum class Type { Key, Char, PointerPos, PointerButton, Wheel } type;
        ImGuiKey key = ImGuiKey_None;
        bool down = false;
        int button = 0;
        float x = 0, y = 0;
        uint16_t ch = 0;
    };

    struct BackendState
    {
        std::mutex mutex;
        std::vector<InputEvent> queue;
        CoreWindow window{ nullptr };
        event_token keyDown{}, keyUp{}, charReceived{}, pointerMoved{}, pointerPressed{}, pointerReleased{}, wheel{};
        std::chrono::steady_clock::time_point lastTime{};
        bool initialized = false;
    };

    BackendState& State()
    {
        static BackendState s;
        return s;
    }

    void Enqueue(InputEvent const& e)
    {
        auto& s = State();
        std::lock_guard lock(s.mutex);
        if (s.queue.size() < 4096)
        {
            s.queue.push_back(e);
        }
    }

    ImGuiKey MapVirtualKey(VirtualKey vk)
    {
        const int k = static_cast<int>(vk);
        if (k >= 'A' && k <= 'Z') return static_cast<ImGuiKey>(ImGuiKey_A + (k - 'A'));
        if (k >= '0' && k <= '9') return static_cast<ImGuiKey>(ImGuiKey_0 + (k - '0'));
        if (k >= static_cast<int>(VirtualKey::F1) && k <= static_cast<int>(VirtualKey::F12))
            return static_cast<ImGuiKey>(ImGuiKey_F1 + (k - static_cast<int>(VirtualKey::F1)));

        switch (vk)
        {
        case VirtualKey::Tab: return ImGuiKey_Tab;
        case VirtualKey::Left: return ImGuiKey_LeftArrow;
        case VirtualKey::Right: return ImGuiKey_RightArrow;
        case VirtualKey::Up: return ImGuiKey_UpArrow;
        case VirtualKey::Down: return ImGuiKey_DownArrow;
        case VirtualKey::PageUp: return ImGuiKey_PageUp;
        case VirtualKey::PageDown: return ImGuiKey_PageDown;
        case VirtualKey::Home: return ImGuiKey_Home;
        case VirtualKey::End: return ImGuiKey_End;
        case VirtualKey::Insert: return ImGuiKey_Insert;
        case VirtualKey::Delete: return ImGuiKey_Delete;
        case VirtualKey::Back: return ImGuiKey_Backspace;
        case VirtualKey::Space: return ImGuiKey_Space;
        case VirtualKey::Enter: return ImGuiKey_Enter;
        case VirtualKey::Escape: return ImGuiKey_Escape;
        case VirtualKey::Shift:
        case VirtualKey::LeftShift: return ImGuiKey_LeftShift;
        case VirtualKey::RightShift: return ImGuiKey_RightShift;
        case VirtualKey::Control:
        case VirtualKey::LeftControl: return ImGuiKey_LeftCtrl;
        case VirtualKey::RightControl: return ImGuiKey_RightCtrl;
        case VirtualKey::Menu:
        case VirtualKey::LeftMenu: return ImGuiKey_LeftAlt;
        case VirtualKey::RightMenu: return ImGuiKey_RightAlt;
        default: return ImGuiKey_None;
        }
    }

    void FeedGamepad(ImGuiIO& io)
    {
        auto pads = Gamepad::Gamepads();
        if (pads.Size() == 0)
        {
            return;
        }
        const GamepadReading r = pads.GetAt(0).GetCurrentReading();
        auto btn = [&](ImGuiKey key, GamepadButtons b) { io.AddKeyEvent(key, (r.Buttons & b) == b); };
        auto axis = [&](ImGuiKey key, double v, double lo, double hi)
        {
            float t = static_cast<float>((v - lo) / (hi - lo));
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            io.AddKeyAnalogEvent(key, t > 0.1f, t);
        };

        btn(ImGuiKey_GamepadStart, GamepadButtons::Menu);
        btn(ImGuiKey_GamepadBack, GamepadButtons::View);
        btn(ImGuiKey_GamepadFaceLeft, GamepadButtons::X);
        btn(ImGuiKey_GamepadFaceRight, GamepadButtons::B);
        btn(ImGuiKey_GamepadFaceUp, GamepadButtons::Y);
        btn(ImGuiKey_GamepadFaceDown, GamepadButtons::A);
        btn(ImGuiKey_GamepadDpadLeft, GamepadButtons::DPadLeft);
        btn(ImGuiKey_GamepadDpadRight, GamepadButtons::DPadRight);
        btn(ImGuiKey_GamepadDpadUp, GamepadButtons::DPadUp);
        btn(ImGuiKey_GamepadDpadDown, GamepadButtons::DPadDown);
        btn(ImGuiKey_GamepadL1, GamepadButtons::LeftShoulder);
        btn(ImGuiKey_GamepadR1, GamepadButtons::RightShoulder);
        btn(ImGuiKey_GamepadL3, GamepadButtons::LeftThumbstick);
        btn(ImGuiKey_GamepadR3, GamepadButtons::RightThumbstick);
        axis(ImGuiKey_GamepadL2, r.LeftTrigger, 0.1, 0.9);
        axis(ImGuiKey_GamepadR2, r.RightTrigger, 0.1, 0.9);
        axis(ImGuiKey_GamepadLStickLeft, r.LeftThumbstickX, -0.2, -0.9);
        axis(ImGuiKey_GamepadLStickRight, r.LeftThumbstickX, 0.2, 0.9);
        axis(ImGuiKey_GamepadLStickUp, r.LeftThumbstickY, 0.2, 0.9);
        axis(ImGuiKey_GamepadLStickDown, r.LeftThumbstickY, -0.2, -0.9);
        axis(ImGuiKey_GamepadRStickLeft, r.RightThumbstickX, -0.2, -0.9);
        axis(ImGuiKey_GamepadRStickRight, r.RightThumbstickX, 0.2, 0.9);
        axis(ImGuiKey_GamepadRStickUp, r.RightThumbstickY, 0.2, 0.9);
        axis(ImGuiKey_GamepadRStickDown, r.RightThumbstickY, -0.2, -0.9);
    }
}

void ImGui_ImplUwp_AttachWindow(CoreWindow const& window)
{
    auto& s = State();
    ImGui_ImplUwp_DetachWindow();
    s.window = window;

    s.keyDown = window.KeyDown([](CoreWindow const&, KeyEventArgs const& args)
    {
        InputEvent e{ InputEvent::Type::Key };
        e.key = MapVirtualKey(args.VirtualKey());
        e.down = true;
        if (e.key != ImGuiKey_None) Enqueue(e);
    });
    s.keyUp = window.KeyUp([](CoreWindow const&, KeyEventArgs const& args)
    {
        InputEvent e{ InputEvent::Type::Key };
        e.key = MapVirtualKey(args.VirtualKey());
        e.down = false;
        if (e.key != ImGuiKey_None) Enqueue(e);
    });
    s.charReceived = window.CharacterReceived([](CoreWindow const&, CharacterReceivedEventArgs const& args)
    {
        InputEvent e{ InputEvent::Type::Char };
        e.ch = static_cast<uint16_t>(args.KeyCode());
        Enqueue(e);
    });

    auto onPointer = [](PointerEventArgs const& args, bool pressedEvent, bool releasedEvent)
    {
        auto point = args.CurrentPoint();
        auto pos = point.Position();
        InputEvent move{ InputEvent::Type::PointerPos };
        move.x = pos.X;
        move.y = pos.Y;
        Enqueue(move);

        if (pressedEvent || releasedEvent)
        {
            auto props = point.Properties();
            InputEvent b{ InputEvent::Type::PointerButton };
            b.down = pressedEvent;
            if (pressedEvent)
            {
                b.button = props.IsRightButtonPressed() ? 1 : (props.IsMiddleButtonPressed() ? 2 : 0);
            }
            else
            {
                using winrt::Windows::UI::Input::PointerUpdateKind;
                auto kind = props.PointerUpdateKind();
                b.button = kind == PointerUpdateKind::RightButtonReleased ? 1 : (kind == PointerUpdateKind::MiddleButtonReleased ? 2 : 0);
            }
            Enqueue(b);
        }
    };

    s.pointerMoved = window.PointerMoved([onPointer](CoreWindow const&, PointerEventArgs const& a) { onPointer(a, false, false); });
    s.pointerPressed = window.PointerPressed([onPointer](CoreWindow const&, PointerEventArgs const& a) { onPointer(a, true, false); });
    s.pointerReleased = window.PointerReleased([onPointer](CoreWindow const&, PointerEventArgs const& a) { onPointer(a, false, true); });
    s.wheel = window.PointerWheelChanged([](CoreWindow const&, PointerEventArgs const& args)
    {
        InputEvent e{ InputEvent::Type::Wheel };
        e.y = static_cast<float>(args.CurrentPoint().Properties().MouseWheelDelta()) / 120.0f;
        Enqueue(e);
    });
}

void ImGui_ImplUwp_DetachWindow()
{
    auto& s = State();
    if (!s.window)
    {
        return;
    }
    s.window.KeyDown(s.keyDown);
    s.window.KeyUp(s.keyUp);
    s.window.CharacterReceived(s.charReceived);
    s.window.PointerMoved(s.pointerMoved);
    s.window.PointerPressed(s.pointerPressed);
    s.window.PointerReleased(s.pointerReleased);
    s.window.PointerWheelChanged(s.wheel);
    s.window = nullptr;
}

bool ImGui_ImplUwp_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
    io.BackendPlatformName = "imgui_impl_uwp";
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformUserData = &State();
    State().lastTime = std::chrono::steady_clock::now();
    State().initialized = true;
    return true;
}

void ImGui_ImplUwp_Shutdown()
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    State().initialized = false;
}

void ImGui_ImplUwp_NewFrame(float displayWidth, float displayHeight, float pointerScale, bool feedGamepad)
{
    auto& s = State();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(displayWidth, displayHeight);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - s.lastTime).count();
    s.lastTime = now;
    io.DeltaTime = dt > 0.0f ? (dt < 0.25f ? dt : 0.25f) : 1.0f / 60.0f;

    std::vector<InputEvent> events;
    {
        std::lock_guard lock(s.mutex);
        events.swap(s.queue);
    }
    for (auto const& e : events)
    {
        switch (e.type)
        {
        case InputEvent::Type::Key: io.AddKeyEvent(e.key, e.down); break;
        case InputEvent::Type::Char: io.AddInputCharacterUTF16(e.ch); break;
        case InputEvent::Type::PointerPos: io.AddMousePosEvent(e.x * pointerScale, e.y * pointerScale); break;
        case InputEvent::Type::PointerButton: io.AddMouseButtonEvent(e.button, e.down); break;
        case InputEvent::Type::Wheel: io.AddMouseWheelEvent(0.0f, e.y); break;
        }
    }

    if (feedGamepad)
    {
        FeedGamepad(io);
    }
}
