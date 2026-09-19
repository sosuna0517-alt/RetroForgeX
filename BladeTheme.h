#pragma once
//
// BladeTheme.h
//
// Visual constants for RetroForgeX's "Blades" dashboard theme, modeled on
// the original 2005 Xbox 360 launch dashboard: black background, glowing
// green blade panels, side blades receding/dimming/bowing away from a
// centered active blade.
//
// NOTE: colors here are an approximation from memory/common recreations,
// not extracted from original assets - eyeball them against reference
// screenshots and tweak to taste. Font is intentionally NOT the original
// Xbox system font (proprietary) - pick a free geometric sans-serif that
// reads similarly clean at a distance (Segoe UI, Roboto, etc. all work).
//
// NOTE: written without a compiler to check it against - the constants
// and IM_COL32 usage are standard Dear ImGui, but double check against
// whatever ImGui version RetroForgeX is pinned to.

#include "imgui.h"

namespace RetroForgeX::BladeTheme
{
    // --- Palette ---
    constexpr ImU32 ColorBackground       = IM_COL32(4, 8, 5, 255);       // near-black w/ faint green cast
    constexpr ImU32 ColorBladeIdle        = IM_COL32(14, 32, 18, 235);    // dim forest-green panel
    constexpr ImU32 ColorBladeActive      = IM_COL32(22, 58, 26, 245);    // brighter panel when centered
    constexpr ImU32 ColorGlow             = IM_COL32(90, 255, 90, 255);   // edge glow / selection highlight
    constexpr ImU32 ColorGlowDim          = IM_COL32(60, 160, 70, 120);   // glow on non-active blades
    constexpr ImU32 ColorTextIdle         = IM_COL32(170, 200, 175, 200);
    constexpr ImU32 ColorTextActive       = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 ColorSubmenuHighlight = IM_COL32(120, 255, 120, 255);

    // --- Layout ---
    constexpr float BladeBarHeight      = 140.0f; // pixels, header row of blades
    constexpr float ActiveBladeWidth    = 260.0f;
    constexpr float SideBladeWidth      = 160.0f;
    constexpr float BladeSpacing        = 18.0f;
    constexpr float MaxSideCurveOffsetY = 34.0f;  // how far side blades bow away vertically
    constexpr float SideBladeMinAlpha   = 0.35f;  // alpha of the furthest visible blade

    // --- Motion ---
    constexpr float BladeTransitionSeconds = 0.32f; // time to slide a new blade to center

    // Ease-out-cubic - snappy start, soft landing, close to the original
    // dashboard's blade-slide feel.
    inline float EaseOutCubic(float t)
    {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }
}
