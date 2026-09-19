#pragma once
//
// CoreRegistrations.h
//
// Registers every core currently sitting in Assets\Cores with a CoreUpdater,
// grouped by system where I'm confident of the system, with an honest
// "Unsorted / Verify" bucket at the end for names I couldn't confidently
// place - check those against the buildbot yourself before relying on them.
//
// A handful of these aren't traditional console emulators - engine
// reimplementations (ScummVM, PrBoom, the Quake ports, OpenLara, etc.) need
// their own game data files rather than a console BIOS/ROM, and
// "romcleaner" isn't an emulator at all. Worth deciding whether all of
// these belong in a player-facing "Systems" blade or just some of them.
//
// NOTE: written without a compiler to check it against - the core/zip
// filenames are transcribed exactly from what you pasted, but double check
// a few against the actual buildbot listing before shipping, since a wrong
// zip filename will just fail silently as "buildbot layout may have
// changed" per CoreUpdater's existing error message.

#include "CoreUpdater.h"

namespace RetroForgeX
{
    // Implemented in CoreRegistrations.cpp - declared here so multiple
    // translation units can call it without redefinition errors.
    void RegisterAllCores(CoreUpdater& updater);
}
