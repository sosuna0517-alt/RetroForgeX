#pragma once
// imgui_impl_uwp: Dear ImGui platform backend for UWP (CoreWindow + XAML).
//
// Dear ImGui has no official UWP backend (imgui_impl_win32 needs an HWND).
// This backend:
//   - subscribes to CoreWindow keyboard / character / pointer events on the
//     UI thread and queues them (no ImGui calls happen on the UI thread);
//   - drains the queue in ImGui_ImplUwp_NewFrame() on the render thread, which
//     owns the ImGui context;
//   - feeds Windows.Gaming.Input gamepad state as ImGuiKey_Gamepad* events.
//
// Pair with the official renderer backend imgui_impl_dx11.

#include "imgui.h"

// UI thread: call once with the window that hosts the SwapChainPanel.
void ImGui_ImplUwp_AttachWindow(winrt::Windows::UI::Core::CoreWindow const& window);
void ImGui_ImplUwp_DetachWindow();

// Render thread (ImGui context owner).
bool ImGui_ImplUwp_Init();
void ImGui_ImplUwp_Shutdown();
// displayWidth/Height: swap-chain size in pixels. pointerScale: pixels per DIP
// (swap-chain width / panel DIP width) so pointer coordinates match.
void ImGui_ImplUwp_NewFrame(float displayWidth, float displayHeight, float pointerScale, bool feedGamepad);
