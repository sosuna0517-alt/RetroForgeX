# Core compatibility notes

Based on each DLL's import table (nightly 2026-11-19, 258 cores). Everything
not listed here only imports Windows system DLLs and should load.

## Needs OpenGL (30)
These import `opengl32.dll`. Xbox has no OpenGL, so they won't load there.
On a Windows PC they load, but RetroForgeX only offers a Direct3D 11 hardware
context, so cores that require GL rendering will refuse to start a game.

3dengine, boom3, boom3_xp, cemu, craft, desmume, kronos, mednafen_psx_hw,
mednafen_vb, melonds, mupen64plus_next, nuance, odin, openlara, openmsx,
parallel_dc, pcee2, play, ppsspp, rpcs3, supermodel, vecx, vircon32,
vitaquake2, vitaquake2-rogue, vitaquake2-xatrix, vitaquake2-zaero, vitaquake3,
xemu, yabasanshiro

## Missing dependency (2)
- doukutsu_rs needs `libstdc++-6.dll` (not shipped)
- rpcs3 needs `vulkan-1.dll`

## Desktop Visual C++ runtime (7)
These import `vcruntime140.dll`/`msvcp140.dll` (desktop runtime, not the
Store VCLibs). They usually load on a PC with the VC++ redistributable but
not on Xbox: citra2018, play, pokket, pokketstation, swanstation, vita3k,
xenia_edge
