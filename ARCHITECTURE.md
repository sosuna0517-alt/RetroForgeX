# ARCHITECTURE.md

> Single source of truth for this project. Paste or attach this file at the start of any AI-assistant session so every tool works from the same stack, layout, and milestone state. Update the **Milestones** and **Changelog** sections whenever something lands.

Last updated: 2026-11-18 (branch `xaml-d3d11-4k`)

---

## 1. Project summary

A native 4K (3840x2160) multi-console retro emulator frontend for **Xbox Series X|S Developer Mode**. It hosts libretro cores (including hardware-accelerated 3D cores), presents output through a 4K CRT post-process chain, and uses Dear ImGui for overlays and quick-swap menus.

| Item | Value |
| --- | --- |
| Language / projection | C++20, C++/WinRT |
| App model | UWP (Universal Windows), packaged `.msix`/`.appx` |
| Platform | x64 only (Xbox Series X\|S Dev Mode + Windows desktop for debugging) |
| IDE | Visual Studio 2022/2026 (v143 toolset, Windows SDK 10.0.26100.0, UWP workload) |
| Target resolution | 3840x2160 swap chain, independent of XAML layout scale |
| Xbox app type | Must be set to **Game** (not App) in Dev Home for full GPU/memory and 4K output |

---

## 2. Tech stack

### 2.1 UI shell
- `MainPage.xaml` hosts a full-screen `SwapChainPanel` (no other XAML chrome during gameplay).
- XAML is used only for app lifecycle, the panel, and system dialogs (folder picker).
- **Dear ImGui** draws all in-app UI (overlays, core/state quick-swap, settings) into the same swap chain.
  - Renderer backend: official `imgui_impl_dx11`.
  - Platform backend: **custom `imgui_impl_uwp`** (lives in `UI/`). There is no official UWP backend in Dear ImGui; `imgui_impl_win32` requires an `HWND`, which UWP does not expose. The custom backend feeds `ImGuiIO` from:
    - `Windows.Gaming.Input.Gamepad` → `io.AddKeyEvent(ImGuiKey_Gamepad*)` with `ImGuiConfigFlags_NavEnableGamepad`
    - `CoreWindow` key/char events (keyboard on PC debugging)
    - `SwapChainPanel` pointer events (mouse/touch on PC debugging)
    - `io.DisplaySize` = swap-chain size (3840x2160), `io.DeltaTime` from a QPC timer

### 2.2 Graphics backend
- **Direct3D 11**: `D3D11CreateDevice` → `ID3D11Device`/`ID3D11DeviceContext` (target feature level 11_1, fallback 11_0).
- Swap chain: `IDXGIFactory2::CreateSwapChainForComposition` → `IDXGISwapChain1`, attached with `ISwapChainPanelNative::SetSwapChain`.
  - Flip model (`DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL`), 2–3 buffers, `DXGI_FORMAT_B8G8R8A8_UNORM` (or `R10G10B10A2_UNORM` for HDR later).
  - Size the buffers at 3840x2160 explicitly and apply an inverse `SwapChainPanel.CompositionScale` transform (`IDXGISwapChain2::SetMatrixTransform`) so XAML scaling does not downsample to 1080p.
  - Query the real output mode via `Windows.Graphics.Display.Core.HdmiDisplayInformation` on Xbox.
- **libretro hardware rendering**: cores requesting `RETRO_ENVIRONMENT_SET_HW_RENDER` with `context_type = RETRO_HW_CONTEXT_D3D11` receive the frontend's device/context via `RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE` → `struct retro_hw_render_interface_d3d11` (from `libretro_d3d11.h`).
  - Note: the enum is `RETRO_HW_CONTEXT_D3D11` (value 7). `RETRO_HW_CONTEXT_DIRECT3D11` does not exist.
  - Software cores: `retro_video_refresh_t` frames are uploaded into a dynamic `ID3D11Texture2D` (XRGB8888 / RGB565 converted on CPU or in a shader).
  - Most 3D libretro cores target OpenGL/Vulkan, not D3D11. GL/Vulkan-only cores are **out of scope** for this backend unless an ANGLE/translation layer is added later.

### 2.3 Post-processing and transitions (HLSL, Shader Model 5.0)
Render graph per frame:

```
Core output texture (native res)
  → [Pass 1] Integer/aspect scale to 4K intermediate RT
  → [Pass 2] CRT / aperture-grille filter (PS_CRT.hlsl)
  → [Pass 3] Analog Channel Surf transition (PS_ChannelSurf.hlsl, only while active)
  → Back buffer
  → Dear ImGui overlay (imgui_impl_dx11)
  → Present(1, 0)
```

- **CRT / aperture-grille filter** – Trinitron-style vertical RGB phosphor mask sized to 4K pixel pitch, scanline beam profile, bloom/halation, gamma in/out, optional curvature + vignette.
- **Analog Channel Surf transition** – triggered by **LB/RB** when switching cores or save states. Timeline driven by a constant buffer `t` (0→1):
  1. Vertical CRT collapse (image squeezes to a bright horizontal line, then a dot)
  2. TV static (hash-based noise, rolling bars)
  3. Chromatic aberration (R/B channel offset that decays as the new image expands back)
- Shaders are compiled at build time by VS's HLSL item type (FXC → C headers `g_<Name>` in `$(IntDir)CompiledShaders`, embedded in the exe). `D3DCompile` at runtime is only exposed to cores through the libretro HW interface.
- Channel Surf timeline (progress 0→1, ~1.2 s): collapse 0–0.30, dot 0.30–0.36, static 0.36–0.64, expand 0.64–1.0. The old frame (snapshot) is shown below 0.5; the core swap happens at 0.5 and the timeline holds on static while the ROM loads on a worker thread.

### 2.4 Storage
- External USB 3.0 drives (exFAT/NTFS) for ROMs, BIOS, saves, and states.
- `Package.appxmanifest` capabilities:
  - `broadFileSystemAccess` (restricted capability, `rescap:` namespace). On first run the user must grant File System access to the app in system settings; until then, raw path APIs fail with access denied.
  - `removableStorage` (+ declared file type associations) as a fallback for `KnownFolders::RemovableDevices`.
- `Windows.Storage.Pickers.FolderPicker` lets the user pick a ROM root; the result is persisted with `StorageApplicationPermissions::FutureAccessList` so it survives restarts.
- All long-running file work runs on MTA worker threads (blocking `.get()`), never on the UI or render thread.
- Scan order: `LocalState\\roms`, FolderPicker folders (FutureAccessList), `D:`–`K:\\roms` (broadFileSystemAccess), `KnownFolders::RemovableDevices\\*\\roms`.

### 2.5 libretro core loading
- Cores are UWP-compatible x64 DLLs packaged with the app (or in `LocalState`), loaded with `LoadPackagedLibrary`.
- Desktop-built cores that call APIs unavailable in the app container will fail to load; prefer cores built for UWP (`WINAPI_FAMILY=WINAPI_FAMILY_APP`).
- Frontend implements: environment callback, video refresh, audio sample/batch (→ XAudio2), input poll/state (→ `Windows.Gaming.Input`), save-state serialize/unserialize for quick-swap.

---

## 3. Folder structure

The repo root **is** the project folder (single-project solution, `RetroForgeX.slnx`).

```
RetroForgeX/
├── ARCHITECTURE.md              ← this file
├── README.md
├── RetroForgeX.slnx             ← solution (x64, ARM64)
├── RetroForgeX.vcxproj(.filters)← Blank App (C++/WinRT) based UWP project
├── Package.appxmanifest         ← broadFileSystemAccess, removableStorage, internetClient
├── packages.config              ← Microsoft.Windows.CppWinRT 2.0.250303.1
├── PropertySheet.props
├── RetroForgeX_TemporaryKey.pfx ← dev signing cert (CN=sosun)
├── pch.h / pch.cpp
├── App.xaml / App.h / App.cpp / App.idl      ← lifecycle, suspend/resume → Frontend
├── MainPage.xaml / .h / .cpp / .idl          ← full-screen SwapChainPanel host
├── Frontend.h / Frontend.cpp    ← render thread, modes, channel surf, saves, dashboard
│
├── Core/                        ← libretro hosting
│   ├── LibretroCore.h/.cpp      (LoadPackagedLibrary, environment, HW render D3D11, options)
│   ├── AudioOutput.h/.cpp       (XAudio2 ring of 8 buffers)
│   ├── InputMapper.h/.cpp       (Windows.Gaming.Input → RETRO_DEVICE_JOYPAD/ANALOG, rumble)
│   ├── CoreUpdater.h/.cpp       (buildbot download + miniz unzip; carried over, not wired yet)
│   └── CoreRegistrations.h/.cpp (216-core catalog; carried over, not wired yet)
│
├── Graphics/                    ← D3D11 device + render graph
│   ├── DeviceResources.h/.cpp   (device, composition swap chain, 4K sizing, SetMatrixTransform)
│   └── PostProcess.h/.cpp       (frame upload, scene/snapshot 4K RTs, CRT + Channel Surf passes)
│
├── Shaders/                     ← HLSL SM 5.0, compiled by FXC to headers ($(IntDir)CompiledShaders)
│   ├── Common.hlsli
│   ├── VS_Fullscreen.hlsl
│   ├── PS_CRT_ApertureGrille.hlsl
│   └── PS_ChannelSurf.hlsl
│
├── Storage/
│   └── RomLibrary.h/.cpp        (core probe, ROM scan: LocalState, picked folders, X:\roms, USB)
│
├── UI/
│   ├── imgui_impl_uwp.h/.cpp    (custom platform backend: CoreWindow keys/pointer, gamepad)
│   ├── BladeMenu.h/.cpp         (Xbox 360 "blades" dashboard widget, 4K-scaled, scrolling)
│   └── BladeTheme.h
│
├── Cores/                       ← drop UWP-built *_libretro.dll here; packaged automatically
├── Assets/                      ← tiles, splash
└── ThirdParty/                  ← vendored, pinned, not edited
    ├── imgui/                   (v1.93.0 core + backends/imgui_impl_dx11.*)
    ├── libretro/                (libretro.h, libretro_d3d.h, libretro_d3d11.h — RetroArch master)
    └── miniz/                   (3.x single-file)
```

### Runtime data (LocalState)
| Path | Contents |
| --- | --- |
| `roms\` | Always scanned |
| `system\` | BIOS (`RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY`) |
| `saves\<game>.<core>.srm` | SRAM |
| `states\<game>.<core>.auto.state` / `.slot0.state` | Auto (channel surf / suspend) and manual states |
| `cache\` | Copies of ROMs for `need_fullpath` cores when the original path isn't directly readable |
| `channels.txt`, `display.txt` | Channel list, CRT settings |

### Controls
| Context | Input | Action |
| --- | --- | --- |
| Dashboard | D-pad / stick, A, B, LT/RT | Navigate blades, select, back, page |
| Dashboard | LB / RB | Channel surf to previous / next channel |
| In game | View + Menu (or F1/Esc) | Open / close the in-game blade menu (pauses) |
| In game / menu | View + LB / RB | Channel surf (auto-saves state, loads the other channel's auto state) |
| Menu | Left / Right on a value | Adjust (CRT settings, per-game core choice) |

---

## 4. Dependencies

| Dependency | Source | How it's added | Required |
| --- | --- | --- | --- |
| Microsoft.Windows.CppWinRT 2.0.250303.1 | NuGet | `packages.config`, restored into `packages\\` | Yes |
| Dear ImGui (v1.93.0 or pinned tag) | github.com/ocornut/imgui | Vendored into `ThirdParty/imgui` (git submodule or copy) | Yes |
| `imgui_impl_dx11` | ImGui `backends/` | Vendored with ImGui | Yes |
| `imgui_impl_uwp` | Written in-project | `UI/imgui_impl_uwp.*` | Yes |
| `libretro.h`, `libretro_d3d.h`, `libretro_d3d11.h` | libretro/RetroArch `libretro-common/include` | Vendored into `ThirdParty/libretro` (pin a commit) | Yes |
| Windows SDK libs: `d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`, `xaudio2.lib`, `windowsapp.lib` | Windows SDK | Linker → Additional Dependencies | Yes |
| Microsoft.Windows.ImplementationLibrary (WIL) | NuGet | Optional error-handling helpers | Optional |
| DirectXTK (`directxtk_uwp`) | NuGet | Optional texture loading / sprite batch for box art | Optional |
| miniz | GitHub | Vendored in `ThirdParty/miniz` (used by CoreUpdater) | Yes |

---

## 5. Build milestones

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done and verified.

| # | Milestone | Status | Verified on |
| --- | --- | --- | --- |
| M0 | Blank App (C++/WinRT) x64 project; `Core/ Graphics/ Shaders/ Storage/ UI/` folders + filters | [x] | Repo layout |
| M1 | `ThirdParty/` vendored (ImGui 1.93.0, libretro headers, miniz); all sources compile | [x] | Full VS 2026 build (v145) + deploy to desktop on DESKTOP-I40NUUM, 2026-11-19 |
| M2 | D3D11 device + 3840x2160 composition swap chain on `SwapChainPanel` | [~] | App deploys and launches on desktop; 4K output not yet checked |
| M3 | Dear ImGui (dx11 + custom UWP backend) + Blade dashboard, gamepad/keyboard navigation | [~] | Implemented, not run |
| M4 | Software libretro core: LoadPackagedLibrary, frame upload, XAudio2, WGI input | [~] | Implemented, not run |
| M5 | CRT / aperture-grille pass at 4K, tunable from DISPLAY blade | [~] | Implemented, not run |
| M6 | USB storage: folder picker + FutureAccessList + broadFileSystemAccess + RemovableDevices scan | [~] | Implemented, not run |
| M7 | SRAM, manual + auto save states, channel list | [~] | Implemented, not run |
| M8 | Analog Channel Surf on LB/RB (dashboard) and View+LB/RB (in game) | [~] | Implemented, not run |
| M9 | `RETRO_HW_CONTEXT_D3D11` core (shared device/context, PS slot 0 SRV → post chain) | [~] | Implemented, no D3D11 core tested |
| M10 | Deployed to Xbox Series X\|S Dev Mode as **Game**; 4K output, pacing, suspend/resume | [ ] | — |

### Current state
- 2026-11-18: RetroForgeX rebuilt on this stack (branch `xaml-d3d11-4k`). The old CoreWindow/D3D12 entry point referenced engine files outside the repo (`..\..\..\Downloads\RetroForgeX\...`) that were never committed, so the engine was rewritten; BladeMenu, BladeTheme, CoreUpdater, CoreRegistrations and miniz were kept.
- Next: open `RetroForgeX.slnx` in Visual Studio, build Debug|x64, deploy locally with a UWP-built core in `Cores\`, then deploy to the Xbox.
- Known gaps: CoreUpdater/CoreRegistrations are compiled but not wired into the UI; frame pacing runs cores off a time accumulator on a 60 Hz vsync (no audio-driven rate control yet); GL/Vulkan-only cores are unsupported.

---

## 6. Conventions
- C++20, `/permissive-`, warnings level 4, treat warnings as errors in Release.
- `winrt::com_ptr<>` for all COM objects; no raw `Release()` calls.
- Render thread owns the D3D11 immediate context. Cores with HW render share it, so the frontend must restore pipeline state after each `retro_run()`.
- Third-party code in `ThirdParty/` is never edited in place; patches go in the project tree.
- Every AI-assistant session: read this file first, update **Milestones** and **Changelog** before ending.

---

## 7. Changelog
- 2026-11-18 – Initial ARCHITECTURE.md created (stack, folder layout, dependencies, milestones M0–M10).
- 2026-11-19 – First successful VS build + desktop deploy. 258 Windows x64 nightly cores in `Cores\` (listed via .info files); reserved .ws/.wsc/.vb removed from manifest.
- 2026-11-18 – RetroForgeX converted to XAML + SwapChainPanel + D3D11 + ImGui. New: DeviceResources, PostProcess, HLSL CRT + Channel Surf, LibretroCore (D3D11 HW render), AudioOutput, InputMapper, RomLibrary, Frontend, imgui_impl_uwp; BladeMenu gained 4K scaling, scrolling lists, adjustable values. Legacy C++ namespace `RetroForgeX` renamed to `rfx` (it collided with the WinRT namespace).
