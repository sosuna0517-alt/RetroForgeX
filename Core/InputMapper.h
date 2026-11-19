#pragma once
// InputMapper: polls Windows.Gaming.Input gamepads and maps them to the
// libretro RetroPad (RETRO_DEVICE_JOYPAD / RETRO_DEVICE_ANALOG).
//
// Xbox -> RetroPad face buttons follow libretro's SNES layout:
//   A -> B (bottom), B -> A (right), X -> Y (left), Y -> X (top).

#include "libretro.h"

namespace rfx
{
    struct PadState
    {
        bool connected = false;
        winrt::Windows::Gaming::Input::GamepadButtons buttons{};
        winrt::Windows::Gaming::Input::GamepadButtons previous{};
        double leftX = 0, leftY = 0, rightX = 0, rightY = 0;
        double leftTrigger = 0, rightTrigger = 0;
        uint16_t retroMask = 0;
    };

    class InputMapper
    {
    public:
        static constexpr size_t kMaxPads = 4;

        void Poll();

        int16_t State(unsigned port, unsigned device, unsigned index, unsigned id) const;
        bool Rumble(unsigned port, retro_rumble_effect effect, uint16_t strength);
        void StopRumble();

        // Frontend hotkeys/UI (pad 0, or any pad when anyPad = true).
        bool Held(winrt::Windows::Gaming::Input::GamepadButtons b, bool anyPad = true) const;
        bool Pressed(winrt::Windows::Gaming::Input::GamepadButtons b, bool anyPad = true) const;
        PadState const& Pad(size_t i) const { return m_pads[i]; }

        // While true, cores see a neutral pad (dashboard open / chords).
        void SetSuppressed(bool suppressed) { m_suppressed = suppressed; }

    private:
        std::array<PadState, kMaxPads> m_pads{};
        std::array<winrt::Windows::Gaming::Input::Gamepad, kMaxPads> m_gamepads{ nullptr, nullptr, nullptr, nullptr };
        std::array<winrt::Windows::Gaming::Input::GamepadVibration, kMaxPads> m_vibration{};
        bool m_suppressed = false;
    };
}
