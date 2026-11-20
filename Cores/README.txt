libretro cores live here. Every *.dll and *.info in this folder is packaged
automatically (see RetroForgeX.vcxproj) and loaded at runtime with
LoadPackagedLibrary. Neither file type is committed to git (.gitignore).

Source: https://buildbot.libretro.com/nightly/windows/x86_64/latest/ (the same
Windows x64 cores RetroArch's own UWP/Xbox build downloads), plus metadata from
https://buildbot.libretro.com/assets/frontend/info.zip.

The .info file next to each dll gives its display name and file extensions, so
the app doesn't have to load every dll at startup. Cores without a .info file
are probed by loading them once.

See COMPATIBILITY.md for cores that are expected to fail on Xbox.
