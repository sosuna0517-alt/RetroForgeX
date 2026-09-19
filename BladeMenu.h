#pragma once
//
// BladeMenu.h
//
// A horizontal "blade bar" widget in the style of the 2005 Xbox 360 launch
// dashboard: a row of category blades, the active one centered and
// enlarged, side blades compressed/dimmed/bowed to suggest a curved
// surface wrapping away from the viewer. Selecting a blade reveals its
// vertical sub-menu underneath.
//
// This owns its own navigation state (which blade is active/animating,
// which sub-item is highlighted) but doesn't read input itself - call
// NavigateLeft/Right/Up/Down/Activate/Back from wherever RetroForgeX
// already handles input (wgi_input.cpp / InputManager), and call
// Update()/Render() once per frame from UIOverlay.

#include <string>
#include <vector>
#include <functional>

namespace RetroForgeX
{
    struct BladeSubItem
    {
        std::string label;
        std::function<void()> onActivate; // called when the player selects this item
    };

    struct Blade
    {
        std::string label; // e.g. "GAMES", "MEDIA", "SYSTEM"
        std::vector<BladeSubItem> items;
    };

    class BladeMenu
    {
    public:
        void AddBlade(Blade blade);

        // Call once per frame with delta time in seconds, regardless of
        // input - this drives the slide/ease animation even on frames
        // with no input.
        void Update(float deltaTimeSeconds);

        // Draws the blade bar, plus the active blade's sub-menu if open.
        // Call from UIOverlay's per-frame ImGui pass.
        void Render();

        // --- Navigation - call from your input handling code ---
        void NavigateLeft();
        void NavigateRight();
        void NavigateDown(); // enter/move down the active blade's sub-menu
        void NavigateUp();   // move up, or exit the sub-menu from the top item
        void Activate();     // select the highlighted sub-item
        void Back();         // leave the sub-menu, return focus to the blade bar

        bool IsInSubmenu() const { return m_inSubmenu; }

    private:
        std::vector<Blade> m_blades;
        int m_activeBladeIndex = 0;
        int m_targetBladeIndex = 0; // blade we're animating toward
        float m_transitionElapsed = 0.0f;
        bool m_transitioning = false;

        bool m_inSubmenu = false;
        int m_submenuIndex = 0;

        void RenderBladeBar();
        void RenderSubmenu();
        float GetBladeBlend() const; // 0 = at m_activeBladeIndex, 1 = fully at m_targetBladeIndex
    };
}
