#include "pch.h"
#include "BladeMenu.h"
#include "BladeTheme.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace RetroForgeX
{
    void BladeMenu::AddBlade(Blade blade)
    {
        m_blades.push_back(std::move(blade));
    }

    void BladeMenu::Update(float deltaTimeSeconds)
    {
        if (!m_transitioning)
        {
            return;
        }

        m_transitionElapsed += deltaTimeSeconds;
        if (m_transitionElapsed >= BladeTheme::BladeTransitionSeconds)
        {
            m_activeBladeIndex = m_targetBladeIndex;
            m_transitioning = false;
            m_transitionElapsed = 0.0f;
        }
    }

    float BladeMenu::GetBladeBlend() const
    {
        if (!m_transitioning)
        {
            return 0.0f;
        }
        float t = m_transitionElapsed / BladeTheme::BladeTransitionSeconds;
        return BladeTheme::EaseOutCubic(t);
    }

    void BladeMenu::NavigateLeft()
    {
        // NOTE: rapid double-tapping mid-transition will re-target from
        // whatever m_activeBladeIndex currently is, which can cause a small
        // visual snap rather than a smooth reversal. Fine for a first pass;
        // revisit if it feels janky in practice.
        if (m_inSubmenu || m_blades.empty())
        {
            return;
        }
        int newIndex = m_activeBladeIndex - 1;
        if (newIndex < 0)
        {
            newIndex = static_cast<int>(m_blades.size()) - 1;
        }
        m_targetBladeIndex = newIndex;
        m_transitioning = true;
        m_transitionElapsed = 0.0f;
    }

    void BladeMenu::NavigateRight()
    {
        if (m_inSubmenu || m_blades.empty())
        {
            return;
        }
        int newIndex = (m_activeBladeIndex + 1) % static_cast<int>(m_blades.size());
        m_targetBladeIndex = newIndex;
        m_transitioning = true;
        m_transitionElapsed = 0.0f;
    }

    void BladeMenu::NavigateDown()
    {
        if (m_blades.empty())
        {
            return;
        }
        m_inSubmenu = true;
        auto& items = m_blades[m_activeBladeIndex].items;
        if (!items.empty())
        {
            m_submenuIndex = std::min(m_submenuIndex + 1, static_cast<int>(items.size()) - 1);
        }
    }

    void BladeMenu::NavigateUp()
    {
        if (!m_inSubmenu)
        {
            return;
        }
        if (m_submenuIndex > 0)
        {
            m_submenuIndex--;
        }
        else
        {
            m_inSubmenu = false;
        }
    }

    void BladeMenu::Activate()
    {
        if (!m_inSubmenu || m_blades.empty())
        {
            return;
        }
        auto& items = m_blades[m_activeBladeIndex].items;
        if (m_submenuIndex >= 0 && m_submenuIndex < static_cast<int>(items.size()))
        {
            if (items[m_submenuIndex].onActivate)
            {
                items[m_submenuIndex].onActivate();
            }
        }
    }

    void BladeMenu::Back()
    {
        m_inSubmenu = false;
        m_submenuIndex = 0;
    }

    void BladeMenu::Render()
    {
        RenderBladeBar();
        if (m_inSubmenu)
        {
            RenderSubmenu();
        }
    }

    void BladeMenu::RenderBladeBar()
    {
        if (m_blades.empty())
        {
            return;
        }

        // Using the foreground draw list so blades render above other UI.
        // If RetroForgeX already has a dedicated fullscreen overlay window,
        // you may want that window's draw list instead - whichever keeps
        // this drawing in the right order relative to your other panels.
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        const float screenW = io.DisplaySize.x;
        const float centerX = screenW * 0.5f;
        const float barTopY = 40.0f; // top margin

        const float blend = GetBladeBlend();
        // Fractional "active position" - e.g. 2.4 means 40% of the way
        // from blade 2 to blade 3.
        const float activePos = static_cast<float>(m_activeBladeIndex) +
            blend * static_cast<float>(m_targetBladeIndex - m_activeBladeIndex);

        const int count = static_cast<int>(m_blades.size());
        for (int i = 0; i < count; ++i)
        {
            const float distance = static_cast<float>(i) - activePos; // signed distance from center
            const float absDistance = std::fabs(distance);

            // Blades further than ~2.5 slots away aren't worth drawing.
            if (absDistance > 2.5f)
            {
                continue;
            }

            // Interpolate width/alpha/vertical-bow by distance from center.
            const float t = std::min(absDistance / 2.5f, 1.0f);
            const float width = BladeTheme::ActiveBladeWidth +
                (BladeTheme::SideBladeWidth - BladeTheme::ActiveBladeWidth) * t;
            const float alpha = 1.0f - (1.0f - BladeTheme::SideBladeMinAlpha) * t;
            const float bowY = BladeTheme::MaxSideCurveOffsetY * t * t; // panels dip as they recede

            // Horizontal position: lay blades out around centerX, spaced by
            // the active blade's slot width.
            const float slotWidth = BladeTheme::ActiveBladeWidth + BladeTheme::BladeSpacing;
            const float x = centerX + distance * slotWidth - width * 0.5f;
            const float y = barTopY + bowY;
            const float height = BladeTheme::BladeBarHeight - bowY * 0.5f;

            const bool isActive = absDistance < 0.5f;
            const ImU32 panelColor = isActive ? BladeTheme::ColorBladeActive : BladeTheme::ColorBladeIdle;
            const ImU32 glowColor = isActive ? BladeTheme::ColorGlow : BladeTheme::ColorGlowDim;

            const ImVec2 topLeft(x, y);
            const ImVec2 bottomRight(x + width, y + height);

            const int glowR = (glowColor >> IM_COL32_R_SHIFT) & 0xFF;
            const int glowG = (glowColor >> IM_COL32_G_SHIFT) & 0xFF;
            const int glowB = (glowColor >> IM_COL32_B_SHIFT) & 0xFF;
            const int glowA = (glowColor >> IM_COL32_A_SHIFT) & 0xFF;

            // Fake glow: a few expanding, fading rect outlines behind the panel.
            for (int g = 3; g >= 1; --g)
            {
                const float expand = static_cast<float>(g) * 3.0f;
                const ImU32 glow = IM_COL32(glowR, glowG, glowB,
                    static_cast<int>(glowA * alpha * 0.25f));
                drawList->AddRect(
                    ImVec2(topLeft.x - expand, topLeft.y - expand),
                    ImVec2(bottomRight.x + expand, bottomRight.y + expand),
                    glow, 8.0f, 0, 2.0f);
            }

            // Panel body.
            const int panelR = (panelColor >> IM_COL32_R_SHIFT) & 0xFF;
            const int panelG = (panelColor >> IM_COL32_G_SHIFT) & 0xFF;
            const int panelB = (panelColor >> IM_COL32_B_SHIFT) & 0xFF;
            const int panelA = (panelColor >> IM_COL32_A_SHIFT) & 0xFF;
            const ImU32 fillColor = IM_COL32(panelR, panelG, panelB, static_cast<int>(panelA * alpha));
            drawList->AddRectFilled(topLeft, bottomRight, fillColor, 10.0f);

            // Crisp edge highlight.
            drawList->AddRect(topLeft, bottomRight, glowColor, 10.0f, 0, isActive ? 2.0f : 1.0f);

            // Label, centered near the bottom of the panel.
            const ImU32 textColor = isActive ? BladeTheme::ColorTextActive : BladeTheme::ColorTextIdle;
            const ImVec2 textSize = ImGui::CalcTextSize(m_blades[i].label.c_str());
            const ImVec2 textPos(x + (width - textSize.x) * 0.5f, y + height - textSize.y - 14.0f);
            drawList->AddText(textPos, textColor, m_blades[i].label.c_str());
        }
    }

    void BladeMenu::RenderSubmenu()
    {
        if (m_blades.empty())
        {
            return;
        }

        const auto& items = m_blades[m_activeBladeIndex].items;
        if (items.empty())
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const float panelWidth = 320.0f;
        const float x = (io.DisplaySize.x - panelWidth) * 0.5f;
        const float y = BladeTheme::BladeBarHeight + 60.0f;

        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(panelWidth, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(BladeTheme::ColorBladeActive));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(BladeTheme::ColorGlow));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("##BladeSubmenu", nullptr, flags);
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const bool selected = (i == m_submenuIndex);
            const ImVec4 color = selected
                ? ImGui::ColorConvertU32ToFloat4(BladeTheme::ColorSubmenuHighlight)
                : ImGui::ColorConvertU32ToFloat4(BladeTheme::ColorTextIdle);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (selected)
            {
                ImGui::Bullet();
                ImGui::SameLine();
            }
            else
            {
                ImGui::Indent(15.0f);
            }
            ImGui::TextUnformatted(items[i].label.c_str());
            if (!selected)
            {
                ImGui::Unindent(15.0f);
            }
            ImGui::PopStyleColor();
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}

// --- Example usage, e.g. in UIOverlay.cpp ---
//
// RetroForgeX::BladeMenu bladeMenu;
//
// Blade gamesBlade{ "GAMES" };
// gamesBlade.items.push_back({ "Game Library", []() { /* open library */ } });
// gamesBlade.items.push_back({ "Demos", []() { /* ... */ } });
// bladeMenu.AddBlade(std::move(gamesBlade));
//
// Blade mediaBlade{ "MEDIA" };
// mediaBlade.items.push_back({ "Music", []() { /* ... */ } });
// bladeMenu.AddBlade(std::move(mediaBlade));
//
// Blade systemBlade{ "SYSTEM" };
// systemBlade.items.push_back({ "Settings", []() { /* ... */ } });
// bladeMenu.AddBlade(std::move(systemBlade));
//
// // Per frame:
// bladeMenu.Update(deltaTime);
// bladeMenu.Render();
//
// // From input handling:
// if (pressedLeft)  bladeMenu.NavigateLeft();
// if (pressedRight) bladeMenu.NavigateRight();
// if (pressedDown)  bladeMenu.NavigateDown();
// if (pressedUp)    bladeMenu.NavigateUp();
// if (pressedA)     bladeMenu.Activate();
// if (pressedB)     bladeMenu.Back();
