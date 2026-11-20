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
// Owns navigation state but doesn't read input itself: the dashboard calls
// NavigateLeft/Right/Up/Down/Activate/Back from gamepad edges, and
// Update()/Render() once per frame inside the ImGui pass.
//
// 4K: all theme sizes are authored for 1080p and multiplied by SetScale()
// (output height / 1080, so 2.0 at 3840x2160).

#include <string>
#include <vector>
#include <functional>

namespace rfx
{
    struct BladeSubItem
    {
        std::string label;
        std::function<void()> onActivate;          // A button
        std::function<std::string()> value;        // optional right-aligned value text
        std::function<void(int direction)> onAdjust; // optional d-pad left/right (-1 / +1)
        std::string detail;                        // optional second line (dim)
    };

    struct Blade
    {
        std::string label; // e.g. "GAMES", "MEDIA", "SYSTEM"
        std::vector<BladeSubItem> items;
        std::string emptyText; // shown when items is empty
    };

    class BladeMenu
    {
    public:
        void AddBlade(Blade blade);
        void SetItems(size_t bladeIndex, std::vector<BladeSubItem> items);
        void SetScale(float scale) { m_scale = scale; }
        void SetActiveBlade(int index);
        int ActiveBlade() const { return m_transitioning ? m_targetBladeIndex : m_activeBladeIndex; }
        size_t BladeCount() const { return m_blades.size(); }

        // Call once per frame with delta time in seconds, regardless of
        // input - this drives the slide/ease animation even on frames
        // with no input.
        void Update(float deltaTimeSeconds);

        // Draws the blade bar, plus the active blade's sub-menu.
        void Render();

        // --- Navigation ---
        void NavigateLeft();
        void NavigateRight();
        void NavigateDown(); // enter/move down the active blade's sub-menu
        void NavigateUp();   // move up, or exit the sub-menu from the top item
        void Activate();     // select the highlighted sub-item
        void Back();         // leave the sub-menu, return focus to the blade bar
        void PageDown();
        void PageUp();

        bool IsInSubmenu() const { return m_inSubmenu; }

    private:
        std::vector<Blade> m_blades;
        int m_activeBladeIndex = 0;
        int m_targetBladeIndex = 0; // blade we're animating toward
        float m_transitionElapsed = 0.0f;
        bool m_transitioning = false;

        bool m_inSubmenu = false;
        int m_submenuIndex = 0;
        int m_scrollTop = 0;
        float m_scale = 1.0f;

        void BeginTransition(int target);
        void RenderBladeBar();
        void RenderSubmenu();
        float GetBladeBlend() const; // 0 = at m_activeBladeIndex, 1 = fully at m_targetBladeIndex
        int VisibleRows() const;
    };
}
