Put UWP (Windows Store / Xbox) builds of libretro cores here, e.g. snes9x_libretro.dll.
Every *.dll in this folder is packaged automatically (see RetroForgeX.vcxproj) and
loaded at runtime with LoadPackagedLibrary. Desktop Win32 builds may fail to load
inside the UWP sandbox if they import APIs that are not available to Store apps.
