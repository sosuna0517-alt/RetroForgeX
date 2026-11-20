#include "pch.h"
#include "BladeMenu.h"
#include "BladeTheme.h"
#include "imgui.h"

namespace rfx
{
    namespace
    {
        ImU32 WithAlpha(ImU32 color, float alphaScale)
        {
            const int r = (color >> IM_COL32_R_SHIFT) & 0xFF;
            const int g = (color >> IM_COL32_G_SHIFT) & 0xFF;
            const int b = (color >> IM_COL32_B_SHIFT) & 0xFF;
            const int a = (color >> IM_COL32_A_SHIFT) & 0xFF;
            return IM_COL32(r, g, b, static_cast<int>(static_cast<float>(a) * alphaScale));
        }
    }

    void BladeMenu::AddBlade(Blade blade)
    {
        m_blades.push_back(std::move(blade));
    }

    void BladeMenu::SetItems(size_t bladeIndex, std::vector<BladeSubItem> items)
    {
        if (bladeIndex >= m_blades.size())
        {
            return;
        }
        m_blades[bladeIndex].items = std::move(items);
        if (static_cast<int>(bladeIndex) == m_activeBladeIndex)
        {
            const int count = static_cast<int>(m_blades[bladeIndex].items.size());
            if (count == 0)
            {
                m_inSubmenu = false;
                m_submenuIndex = 0;
            }
            else
            {
                m_submenuIndex = std::clamp(m_submenuIndex, 0, count - 1);
            }
        }
    }

    void BladeMenu::SetActiveBlade(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_blades.size()))
        {
            return;
        }
        m_activeBladeIndex = m_targetBladeIndex = index;
        m_transitioning = false;
        m_inSubmenu = false;
        m_submenuIndex = 0;
        m_scrollTop = 0;
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

    void BladeMenu::BeginTransition(int target)
    {
        // Finish any in-flight slide first so rapid taps re-target from the
        // blade the user actually sees in the center.
        if (m_transitioning)
        {
            m_activeBladeIndex = m_targetBladeIndex;
        }
        m_targetBladeIndex = target;
        m_transitioning = target != m_activeBladeIndex;
        m_transitionElapsed = 0.0f;
        m_submenuIndex = 0;
        m_scrollTop = 0;
    }

    void BladeMenu::NavigateLeft()
    {
        if (m_blades.empty())
        {
            return;
        }
        if (m_inSubmenu)
        {
            auto& items = m_blades[m_activeBladeIndex].items;
            if (m_submenuIndex < static_cast<int>(items.size()) && items[m_submenuIndex].onAdjust)
            {
                items[m_submenuIndex].onAdjust(-1);
            }
            return;
        }
        int current = ActiveBlade();
        int newIndex = current - 1;
        if (newIndex < 0)
        {
            newIndex = static_cast<int>(m_blades.size()) - 1;
        }
        BeginTransition(newIndex);
    }

    void BladeMenu::NavigateRight()
    {
        if (m_blades.empty())
        {
            return;
        }
        if (m_inSubmenu)
        {
            auto& items = m_blades[m_activeBladeIndex].items;
            if (m_submenuIndex < static_cast<int>(items.size()) && items[m_submenuIndex].onAdjust)
            {
                items[m_submenuIndex].onAdjust(+1);
            }
            return;
        }
        BeginTransition((ActiveBlade() + 1) % static_cast<int>(m_blades.size()));
    }

    void BladeMenu::NavigateDown()
    {
        if (m_blades.empty() || m_transitioning)
        {
            return;
        }
        auto& items = m_blades[m_activeBladeIndex].items;
        if (items.empty())
        {
            return;
        }
        if (!m_inSubmenu)
        {
            // Entering the sub-menu lands on the first item.
            m_inSubmenu = true;
            m_submenuIndex = 0;
            m_scrollTop = 0;
            return;
        }
        m_submenuIndex = std::min(m_submenuIndex + 1, static_cast<int>(items.size()) - 1);
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

    void BladeMenu::PageDown()
    {
        if (!m_inSubmenu || m_blades.empty()) return;
        const int count = static_cast<int>(m_blades[m_activeBladeIndex].items.size());
        if (count > 0) m_submenuIndex = std::min(m_submenuIndex + VisibleRows(), count - 1);
    }

    void BladeMenu::PageUp()
    {
        if (!m_inSubmenu) return;
        m_submenuIndex = std::max(m_submenuIndex - VisibleRows(), 0);
    }

    void BladeMenu::Activate()
    {
        if (m_blades.empty() || m_transitioning)
        {
            return;
        }
        auto& items = m_blades[m_activeBladeIndex].items;
        if (!m_inSubmenu)
        {
            // A on a blade header opens its list.
            NavigateDown();
            return;
        }
        if (m_submenuIndex >= 0 && m_submenuIndex < static_cast<int>(items.size()))
        {
            // Copy: the callback may rebuild this blade's items.
            auto callback = items[m_submenuIndex].onActivate;
            if (callback)
            {
                callback();
            }
        }
    }

    void BladeMenu::Back()
    {
        m_inSubmenu = false;
    }

    void BladeMenu::Render()
    {
        RenderBladeBar();
        RenderSubmenu();
    }

    int BladeMenu::VisibleRows() const
    {
        const ImGuiIO& io = ImGui::GetIO();
        const float rowH = ImGui::GetFontSize() * 1.9f;
        const float top = (BladeTheme::BladeBarHeight + 110.0f) * m_scale;
        const float available = io.DisplaySize.y - top - 120.0f * m_scale;
        return std::max(3, static_cast<int>(available / rowH));
    }

    void BladeMenu::RenderBladeBar()
    {
        if (m_blades.empty())
        {
            return;
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        const float s = m_scale;
        const float screenW = io.DisplaySize.x;
        const float centerX = screenW * 0.5f;
        const float barTopY = 90.0f * s;

        const float blend = GetBladeBlend();
        const float activePos = static_cast<float>(m_activeBladeIndex) +
            blend * static_cast<float>(m_targetBladeIndex - m_activeBladeIndex);

        const float labelSize = ImGui::GetFontSize() * 1.35f;
        ImFont* font = ImGui::GetFont();

        const int count = static_cast<int>(m_blades.size());
        for (int i = 0; i < count; ++i)
        {
            const float distance = static_cast<float>(i) - activePos;
            const float absDistance = std::fabs(distance);
            if (absDistance > 2.5f)
            {
                continue;
            }

            const float t = std::min(absDistance / 2.5f, 1.0f);
            const float width = (BladeTheme::ActiveBladeWidth +
                (BladeTheme::SideBladeWidth - BladeTheme::ActiveBladeWidth) * t) * s;
            const float alpha = 1.0f - (1.0f - BladeTheme::SideBladeMinAlpha) * t;
            const float bowY = BladeTheme::MaxSideCurveOffsetY * t * t * s;

            const float slotWidth = (BladeTheme::ActiveBladeWidth + BladeTheme::BladeSpacing) * s;
            const float x = centerX + distance * slotWidth - width * 0.5f;
            const float y = barTopY + bowY;
            const float height = BladeTheme::BladeBarHeight * s - bowY * 0.5f;

            const bool isActive = absDistance < 0.5f;
            const ImU32 panelColor = isActive ? BladeTheme::ColorBladeActive : BladeTheme::ColorBladeIdle;
            const ImU32 glowColor = isActive ? BladeTheme::ColorGlow : BladeTheme::ColorGlowDim;

            const ImVec2 topLeft(x, y);
            const ImVec2 bottomRight(x + width, y + height);

            for (int g = 3; g >= 1; --g)
            {
                const float expand = static_cast<float>(g) * 3.0f * s;
                drawList->AddRect(
                    ImVec2(topLeft.x - expand, topLeft.y - expand),
                    ImVec2(bottomRight.x + expand, bottomRight.y + expand),
                    WithAlpha(glowColor, alpha * 0.25f), 8.0f * s, 0, 2.0f * s);
            }

            drawList->AddRectFilled(topLeft, bottomRight, WithAlpha(panelColor, alpha), 10.0f * s);
            drawList->AddRect(topLeft, bottomRight, glowColor, 10.0f * s, 0, (isActive ? 2.0f : 1.0f) * s);

            const ImU32 textColor = isActive ? BladeTheme::ColorTextActive : BladeTheme::ColorTextIdle;
            const char* label = m_blades[i].label.c_str();
            const ImVec2 textSize = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label);
            const ImVec2 textPos(x + (width - textSize.x) * 0.5f, y + height - textSize.y - 14.0f * s);
            drawList->AddText(font, labelSize, textPos, WithAlpha(textColor, alpha), label);
        }
    }

    void BladeMenu::RenderSubmenu()
    {
        if (m_blades.empty())
        {
            return;
        }

        const Blade& blade = m_blades[m_transitioning ? m_targetBladeIndex : m_activeBladeIndex];
        const auto& items = blade.items;

        ImGuiIO& io = ImGui::GetIO();
        const float s = m_scale;
        const float panelWidth = std::min(1100.0f * s, io.DisplaySize.x * 0.8f);
        const float x = (io.DisplaySize.x - panelWidth) * 0.5f;
        const float y = (BladeTheme::BladeBarHeight + 110.0f) * s;
        const float rowH = ImGui::GetFontSize() * 1.9f;
        const int rows = VisibleRows();

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float alpha = m_transitioning ? 0.35f + 0.65f * GetBladeBlend() : 1.0f;

        if (items.empty())
        {
            if (!blade.emptyText.empty())
            {
                ImVec2 sz = ImGui::CalcTextSize(blade.emptyText.c_str(), nullptr, false, panelWidth);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x + (panelWidth - sz.x) * 0.5f, y + rowH),
                    WithAlpha(BladeTheme::ColorTextIdle, alpha), blade.emptyText.c_str(), nullptr, panelWidth);
            }
            return;
        }

        // Keep the selection in view.
        const int count = static_cast<int>(items.size());
        if (m_submenuIndex < m_scrollTop) m_scrollTop = m_submenuIndex;
        if (m_submenuIndex >= m_scrollTop + rows) m_scrollTop = m_submenuIndex - rows + 1;
        m_scrollTop = std::clamp(m_scrollTop, 0, std::max(0, count - rows));

        const int visible = std::min(rows, count - m_scrollTop);
        const float panelH = visible * rowH + 24.0f * s;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + panelWidth, y + panelH), WithAlpha(BladeTheme::ColorBladeIdle, 0.85f * alpha), 10.0f * s);
        dl->AddRect(ImVec2(x, y), ImVec2(x + panelWidth, y + panelH), WithAlpha(BladeTheme::ColorGlowDim, alpha), 10.0f * s, 0, 2.0f * s);

        const float pad = 28.0f * s;
        for (int row = 0; row < visible; ++row)
        {
            const int i = m_scrollTop + row;
            const BladeSubItem& item = items[i];
            const float ry = y + 12.0f * s + row * rowH;
            const bool selected = m_inSubmenu && i == m_submenuIndex;

            if (selected)
            {
                dl->AddRectFilled(ImVec2(x + 8.0f * s, ry), ImVec2(x + panelWidth - 8.0f * s, ry + rowH),
                    WithAlpha(BladeTheme::ColorBladeActive, alpha), 6.0f * s);
                dl->AddRect(ImVec2(x + 8.0f * s, ry), ImVec2(x + panelWidth - 8.0f * s, ry + rowH),
                    WithAlpha(BladeTheme::ColorGlow, alpha), 6.0f * s, 0, 2.0f * s);
            }

            const ImU32 color = selected ? BladeTheme::ColorSubmenuHighlight : BladeTheme::ColorTextIdle;
            const float textY = ry + (rowH - ImGui::GetFontSize()) * 0.5f;
            dl->AddText(ImVec2(x + pad, textY), WithAlpha(color, alpha), item.label.c_str());

            std::string right = item.value ? item.value() : item.detail;
            if (!right.empty())
            {
                if (item.onAdjust && selected)
                {
                    right = "<  " + right + "  >";
                }
                const ImVec2 sz = ImGui::CalcTextSize(right.c_str());
                dl->AddText(ImVec2(x + panelWidth - pad - sz.x, textY),
                    WithAlpha(selected ? BladeTheme::ColorTextActive : BladeTheme::ColorGlowDim, alpha), right.c_str());
            }
        }

        // Scroll indicator.
        if (count > rows)
        {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%d / %d", m_submenuIndex + 1, count);
            const ImVec2 sz = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(x + panelWidth - sz.x, y + panelH + 8.0f * s), WithAlpha(BladeTheme::ColorGlowDim, alpha), buf);
        }
    }
}
