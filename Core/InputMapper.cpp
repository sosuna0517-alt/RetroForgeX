#include "pch.h"
#include "InputMapper.h"

using namespace winrt::Windows::Gaming::Input;

namespace rfx
{
    namespace
    {
        constexpr double kDeadzone = 0.18;

        inline bool Has(GamepadButtons set, GamepadButtons b)
        {
            return (set & b) == b;
        }

        double ApplyDeadzone(double v)
        {
            if (std::abs(v) < kDeadzone) return 0.0;
            const double sign = v < 0 ? -1.0 : 1.0;
            return sign * (std::abs(v) - kDeadzone) / (1.0 - kDeadzone);
        }

        int16_t ToAxis(double v)
        {
            return static_cast<int16_t>(std::clamp(v, -1.0, 1.0) * 32767.0);
        }
    }

    void InputMapper::Poll()
    {
        auto pads = Gamepad::Gamepads();
        const uint32_t count = std::min<uint32_t>(pads.Size(), static_cast<uint32_t>(kMaxPads));

        for (uint32_t i = 0; i < kMaxPads; ++i)
        {
            auto& p = m_pads[i];
            p.previous = p.buttons;

            if (i >= count)
            {
                m_gamepads[i] = nullptr;
                p = PadState{};
                continue;
            }

            m_gamepads[i] = pads.GetAt(i);
            const GamepadReading r = m_gamepads[i].GetCurrentReading();
            p.connected = true;
            p.buttons = r.Buttons;
            p.leftX = ApplyDeadzone(r.LeftThumbstickX);
            p.leftY = ApplyDeadzone(r.LeftThumbstickY);
            p.rightX = ApplyDeadzone(r.RightThumbstickX);
            p.rightY = ApplyDeadzone(r.RightThumbstickY);
            p.leftTrigger = r.LeftTrigger;
            p.rightTrigger = r.RightTrigger;

            uint16_t m = 0;
            auto set = [&](GamepadButtons b, unsigned id) { if (Has(r.Buttons, b)) m |= static_cast<uint16_t>(1u << id); };
            set(GamepadButtons::A, RETRO_DEVICE_ID_JOYPAD_B);
            set(GamepadButtons::B, RETRO_DEVICE_ID_JOYPAD_A);
            set(GamepadButtons::X, RETRO_DEVICE_ID_JOYPAD_Y);
            set(GamepadButtons::Y, RETRO_DEVICE_ID_JOYPAD_X);
            set(GamepadButtons::LeftShoulder, RETRO_DEVICE_ID_JOYPAD_L);
            set(GamepadButtons::RightShoulder, RETRO_DEVICE_ID_JOYPAD_R);
            set(GamepadButtons::LeftThumbstick, RETRO_DEVICE_ID_JOYPAD_L3);
            set(GamepadButtons::RightThumbstick, RETRO_DEVICE_ID_JOYPAD_R3);
            set(GamepadButtons::View, RETRO_DEVICE_ID_JOYPAD_SELECT);
            set(GamepadButtons::Menu, RETRO_DEVICE_ID_JOYPAD_START);
            set(GamepadButtons::DPadUp, RETRO_DEVICE_ID_JOYPAD_UP);
            set(GamepadButtons::DPadDown, RETRO_DEVICE_ID_JOYPAD_DOWN);
            set(GamepadButtons::DPadLeft, RETRO_DEVICE_ID_JOYPAD_LEFT);
            set(GamepadButtons::DPadRight, RETRO_DEVICE_ID_JOYPAD_RIGHT);
            if (r.LeftTrigger > 0.5) m |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
            if (r.RightTrigger > 0.5) m |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
            p.retroMask = m;
        }
    }

    int16_t InputMapper::State(unsigned port, unsigned device, unsigned index, unsigned id) const
    {
        if (m_suppressed || port >= kMaxPads)
        {
            return 0;
        }
        const auto& p = m_pads[port];
        if (!p.connected)
        {
            return 0;
        }

        switch (device & RETRO_DEVICE_MASK)
        {
        case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            {
                return static_cast<int16_t>(p.retroMask);
            }
            return (id < 16 && (p.retroMask & (1u << id))) ? 1 : 0;

        case RETRO_DEVICE_ANALOG:
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            {
                return id == RETRO_DEVICE_ID_ANALOG_X ? ToAxis(p.leftX) : ToAxis(-p.leftY);
            }
            if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            {
                return id == RETRO_DEVICE_ID_ANALOG_X ? ToAxis(p.rightX) : ToAxis(-p.rightY);
            }
            if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
            {
                if (id == RETRO_DEVICE_ID_JOYPAD_L2) return ToAxis(p.leftTrigger);
                if (id == RETRO_DEVICE_ID_JOYPAD_R2) return ToAxis(p.rightTrigger);
                return (id < 16 && (p.retroMask & (1u << id))) ? 32767 : 0;
            }
            return 0;

        default:
            return 0;
        }
    }

    bool InputMapper::Rumble(unsigned port, retro_rumble_effect effect, uint16_t strength)
    {
        if (port >= kMaxPads || !m_gamepads[port])
        {
            return false;
        }
        auto& v = m_vibration[port];
        const double amount = strength / 65535.0;
        if (effect == RETRO_RUMBLE_STRONG) v.LeftMotor = amount;
        else v.RightMotor = amount;
        m_gamepads[port].Vibration(v);
        return true;
    }

    void InputMapper::StopRumble()
    {
        for (size_t i = 0; i < kMaxPads; ++i)
        {
            m_vibration[i] = {};
            if (m_gamepads[i])
            {
                m_gamepads[i].Vibration(m_vibration[i]);
            }
        }
    }

    bool InputMapper::Held(GamepadButtons b, bool anyPad) const
    {
        const size_t n = anyPad ? kMaxPads : 1;
        for (size_t i = 0; i < n; ++i)
        {
            if (m_pads[i].connected && Has(m_pads[i].buttons, b)) return true;
        }
        return false;
    }

    bool InputMapper::Pressed(GamepadButtons b, bool anyPad) const
    {
        const size_t n = anyPad ? kMaxPads : 1;
        for (size_t i = 0; i < n; ++i)
        {
            const auto& p = m_pads[i];
            if (p.connected && Has(p.buttons, b) && !Has(p.previous, b)) return true;
        }
        return false;
    }
}
