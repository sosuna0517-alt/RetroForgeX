# RetroForge X

A native 4K (3840x2160) multi-console libretro frontend for **Xbox Series X|S Developer Mode**, built as a C++/WinRT UWP app.

- XAML `SwapChainPanel` + Direct3D 11 composition swap chain at native 4K
- Dear ImGui dashboard in the style of the Xbox 360 "blades"
- 4K CRT / aperture-grille pixel shader and an "Analog Channel Surf" transition (CRT collapse, static, chromatic aberration) on LB/RB
- Software and `RETRO_HW_CONTEXT_D3D11` libretro cores
- ROMs from USB 3.0 drives (folder picker, `broadFileSystemAccess`, removable storage)

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full stack, folder layout, controls and milestones.

## Build

1. Open `RetroForgeX.slnx` in Visual Studio 2022/2026 with the **Universal Windows Platform development** workload and Windows SDK 10.0.26100.0.
2. Restore NuGet packages (Microsoft.Windows.CppWinRT 2.0.250303.1) if Visual Studio doesn't do it automatically.
3. Select **Debug | x64** and build. Shaders in `Shaders\` are compiled by FXC into headers automatically.

## Cores

Put UWP builds of libretro cores (`*_libretro.dll`) in `Cores\`. They are packaged with the app and loaded with `LoadPackagedLibrary`. Desktop Win32 builds may fail to load inside the app container.

## ROMs

Scanned from `LocalState\roms`, any folder added via **SYSTEM > Add ROM folder**, `D:\roms` to `K:\roms` (after granting file system access in Settings > Privacy > File system), and a `roms` folder at the root of removable drives.

## Xbox deployment

Deploy through Visual Studio (Remote Machine) or Device Portal, then set the app type to **Game** in Dev Home so it gets full GPU access and 4K output.
