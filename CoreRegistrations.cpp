#include "pch.h"
#include "CoreRegistrations.h"

namespace RetroForgeX
{
    void RegisterAllCores(CoreUpdater& updater)
    {
        // --- Standalone Games/Demos ---
        updater.RegisterCore({ L"2048", L"2048_libretro.dll", L"2048_libretro.dll.zip" });
        updater.RegisterCore({ L"anarch", L"anarch_libretro.dll", L"anarch_libretro.dll.zip" });
        updater.RegisterCore({ L"craft", L"craft_libretro.dll", L"craft_libretro.dll.zip" });
        updater.RegisterCore({ L"crocods", L"crocods_libretro.dll", L"crocods_libretro.dll.zip" });
        updater.RegisterCore({ L"dinothawr", L"dinothawr_libretro.dll", L"dinothawr_libretro.dll.zip" });
        updater.RegisterCore({ L"dirksimple", L"dirksimple_libretro.dll", L"dirksimple_libretro.dll.zip" });
        updater.RegisterCore({ L"gong", L"gong_libretro.dll", L"gong_libretro.dll.zip" });
        updater.RegisterCore({ L"jumpnbump", L"jumpnbump_libretro.dll", L"jumpnbump_libretro.dll.zip" });
        updater.RegisterCore({ L"mojozork", L"mojozork_libretro.dll", L"mojozork_libretro.dll.zip" });
        updater.RegisterCore({ L"mrboom", L"mrboom_libretro.dll", L"mrboom_libretro.dll.zip" });
        updater.RegisterCore({ L"numero", L"numero_libretro.dll", L"numero_libretro.dll.zip" });
        updater.RegisterCore({ L"superbroswar", L"superbroswar_libretro.dll", L"superbroswar_libretro.dll.zip" });
        updater.RegisterCore({ L"thepowdertoy", L"thepowdertoy_libretro.dll", L"thepowdertoy_libretro.dll.zip" });
        updater.RegisterCore({ L"xrick", L"xrick_libretro.dll", L"xrick_libretro.dll.zip" });

        // --- Sinclair ZX81 ---
        updater.RegisterCore({ L"81", L"81_libretro.dll", L"81_libretro.dll.zip" });

        // --- Atari ---
        updater.RegisterCore({ L"a5200", L"a5200_libretro.dll", L"a5200_libretro.dll.zip" });
        updater.RegisterCore({ L"atari800", L"atari800_libretro.dll", L"atari800_libretro.dll.zip" });
        updater.RegisterCore({ L"prosystem", L"prosystem_libretro.dll", L"prosystem_libretro.dll.zip" });
        updater.RegisterCore({ L"stella", L"stella_libretro.dll", L"stella_libretro.dll.zip" });
        updater.RegisterCore({ L"stella2014", L"stella2014_libretro.dll", L"stella2014_libretro.dll.zip" });
        updater.RegisterCore({ L"stella2023", L"stella2023_libretro.dll", L"stella2023_libretro.dll.zip" });
        updater.RegisterCore({ L"tia", L"tia_libretro.dll", L"tia_libretro.dll.zip" });
        updater.RegisterCore({ L"vecx", L"vecx_libretro.dll", L"vecx_libretro.dll.zip" });
        updater.RegisterCore({ L"virtualjaguar", L"virtualjaguar_libretro.dll", L"virtualjaguar_libretro.dll.zip" });

        // --- Commodore - Amiga ---
        updater.RegisterCore({ L"amiarcadia", L"amiarcadia_libretro.dll", L"amiarcadia_libretro.dll.zip" });
        updater.RegisterCore({ L"puae", L"puae_libretro.dll", L"puae_libretro.dll.zip" });
        updater.RegisterCore({ L"puae2021", L"puae2021_libretro.dll", L"puae2021_libretro.dll.zip" });

        // --- Apple II ---
        updater.RegisterCore({ L"applewin", L"applewin_libretro.dll", L"applewin_libretro.dll.zip" });

        // --- Arduboy/Arduino ---
        updater.RegisterCore({ L"ardens", L"ardens_libretro.dll", L"ardens_libretro.dll.zip" });
        updater.RegisterCore({ L"arduous", L"arduous_libretro.dll", L"arduous_libretro.dll.zip" });
        updater.RegisterCore({ L"uzem", L"uzem_libretro.dll", L"uzem_libretro.dll.zip" });

        // --- Nintendo - 3DS ---
        updater.RegisterCore({ L"azahar", L"azahar_libretro.dll", L"azahar_libretro.dll.zip" });
        updater.RegisterCore({ L"citra", L"citra_libretro.dll", L"citra_libretro.dll.zip" });
        updater.RegisterCore({ L"citra2018", L"citra2018_libretro.dll", L"citra2018_libretro.dll.zip" });
        updater.RegisterCore({ L"panda3ds", L"panda3ds_libretro.dll", L"panda3ds_libretro.dll.zip" });

        // --- BBC Micro ---
        updater.RegisterCore({ L"b2", L"b2_libretro.dll", L"b2_libretro.dll.zip" });

        // --- Unsorted / Verify ---
        updater.RegisterCore({ L"bbkemu", L"bbkemu_libretro.dll", L"bbkemu_libretro.dll.zip" });
        updater.RegisterCore({ L"bk", L"bk_libretro.dll", L"bk_libretro.dll.zip" });
        updater.RegisterCore({ L"boom3", L"boom3_libretro.dll", L"boom3_libretro.dll.zip" });
        updater.RegisterCore({ L"dice", L"dice_libretro.dll", L"dice_libretro.dll.zip" });
        updater.RegisterCore({ L"dingooemu", L"dingooemu_libretro.dll", L"dingooemu_libretro.dll.zip" });
        updater.RegisterCore({ L"gam4980", L"gam4980_libretro.dll", L"gam4980_libretro.dll.zip" });
        updater.RegisterCore({ L"jaxe", L"jaxe_libretro.dll", L"jaxe_libretro.dll.zip" });
        updater.RegisterCore({ L"mu", L"mu_libretro.dll", L"mu_libretro.dll.zip" });
        updater.RegisterCore({ L"native32emu", L"native32emu_libretro.dll", L"native32emu_libretro.dll.zip" });
        updater.RegisterCore({ L"nekop2", L"nekop2_libretro.dll", L"nekop2_libretro.dll.zip" });
        updater.RegisterCore({ L"nicaiemu", L"nicaiemu_libretro.dll", L"nicaiemu_libretro.dll.zip" });
        updater.RegisterCore({ L"nuance", L"nuance_libretro.dll", L"nuance_libretro.dll.zip" });
        updater.RegisterCore({ L"pd777", L"pd777_libretro.dll", L"pd777_libretro.dll.zip" });
        updater.RegisterCore({ L"sameduck", L"sameduck_libretro.dll", L"sameduck_libretro.dll.zip" });
        updater.RegisterCore({ L"vemulator", L"vemulator_libretro.dll", L"vemulator_libretro.dll.zip" });

        // --- Sega - Genesis/Mega Drive ---
        updater.RegisterCore({ L"blastem", L"blastem_libretro.dll", L"blastem_libretro.dll.zip" });
        updater.RegisterCore({ L"clownmdemu", L"clownmdemu_libretro.dll", L"clownmdemu_libretro.dll.zip" });
        updater.RegisterCore({ L"genesis_plus_gx", L"genesis_plus_gx_libretro.dll", L"genesis_plus_gx_libretro.dll.zip" });
        updater.RegisterCore({ L"genesis_plus_gx_wide", L"genesis_plus_gx_wide_libretro.dll", L"genesis_plus_gx_wide_libretro.dll.zip" });
        updater.RegisterCore({ L"picodrive", L"picodrive_libretro.dll", L"picodrive_libretro.dll.zip" });

        // --- MSX ---
        updater.RegisterCore({ L"bluemsx", L"bluemsx_libretro.dll", L"bluemsx_libretro.dll.zip" });
        updater.RegisterCore({ L"fmsx", L"fmsx_libretro.dll", L"fmsx_libretro.dll.zip" });

        // --- Nintendo - SNES ---
        updater.RegisterCore({ L"bsnes_cplusplus98", L"bsnes_cplusplus98_libretro.dll", L"bsnes_cplusplus98_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes_hd_beta", L"bsnes_hd_beta_libretro.dll", L"bsnes_hd_beta_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes", L"bsnes_libretro.dll", L"bsnes_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes_mercury_accuracy", L"bsnes_mercury_accuracy_libretro.dll", L"bsnes_mercury_accuracy_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes_mercury_balanced", L"bsnes_mercury_balanced_libretro.dll", L"bsnes_mercury_balanced_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes_mercury_performance", L"bsnes_mercury_performance_libretro.dll", L"bsnes_mercury_performance_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes-jg", L"bsnes-jg_libretro.dll", L"bsnes-jg_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes2014_accuracy", L"bsnes2014_accuracy_libretro.dll", L"bsnes2014_accuracy_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes2014_balanced", L"bsnes2014_balanced_libretro.dll", L"bsnes2014_balanced_libretro.dll.zip" });
        updater.RegisterCore({ L"bsnes2014_performance", L"bsnes2014_performance_libretro.dll", L"bsnes2014_performance_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_snes", L"mednafen_snes_libretro.dll", L"mednafen_snes_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_supafaust", L"mednafen_supafaust_libretro.dll", L"mednafen_supafaust_libretro.dll.zip" });
        updater.RegisterCore({ L"mesen-s", L"mesen-s_libretro.dll", L"mesen-s_libretro.dll.zip" });
        updater.RegisterCore({ L"snes9x", L"snes9x_libretro.dll", L"snes9x_libretro.dll.zip" });
        updater.RegisterCore({ L"snes9x2002", L"snes9x2002_libretro.dll", L"snes9x2002_libretro.dll.zip" });
        updater.RegisterCore({ L"snes9x2005", L"snes9x2005_libretro.dll", L"snes9x2005_libretro.dll.zip" });
        updater.RegisterCore({ L"snes9x2005_plus", L"snes9x2005_plus_libretro.dll", L"snes9x2005_plus_libretro.dll.zip" });
        updater.RegisterCore({ L"snes9x2010", L"snes9x2010_libretro.dll", L"snes9x2010_libretro.dll.zip" });

        // --- Arcade ---
        updater.RegisterCore({ L"cannonball", L"cannonball_libretro.dll", L"cannonball_libretro.dll.zip" });
        updater.RegisterCore({ L"fbalpha2012_cps1", L"fbalpha2012_cps1_libretro.dll", L"fbalpha2012_cps1_libretro.dll.zip" });
        updater.RegisterCore({ L"fbalpha2012_cps2", L"fbalpha2012_cps2_libretro.dll", L"fbalpha2012_cps2_libretro.dll.zip" });
        updater.RegisterCore({ L"fbalpha2012_cps3", L"fbalpha2012_cps3_libretro.dll", L"fbalpha2012_cps3_libretro.dll.zip" });
        updater.RegisterCore({ L"fbalpha2012", L"fbalpha2012_libretro.dll", L"fbalpha2012_libretro.dll.zip" });
        updater.RegisterCore({ L"hbmame", L"hbmame_libretro.dll", L"hbmame_libretro.dll.zip" });
        updater.RegisterCore({ L"mame", L"mame_libretro.dll", L"mame_libretro.dll.zip" });
        updater.RegisterCore({ L"mame2000", L"mame2000_libretro.dll", L"mame2000_libretro.dll.zip" });
        updater.RegisterCore({ L"mame2003", L"mame2003_libretro.dll", L"mame2003_libretro.dll.zip" });
        updater.RegisterCore({ L"mame2003_midway", L"mame2003_midway_libretro.dll", L"mame2003_midway_libretro.dll.zip" });
        updater.RegisterCore({ L"mame2003_plus", L"mame2003_plus_libretro.dll", L"mame2003_plus_libretro.dll.zip" });
        updater.RegisterCore({ L"mame2010", L"mame2010_libretro.dll", L"mame2010_libretro.dll.zip" });
        updater.RegisterCore({ L"supermodel", L"supermodel_libretro.dll", L"supermodel_libretro.dll.zip" });

        // --- Amstrad CPC ---
        updater.RegisterCore({ L"cap32", L"cap32_libretro.dll", L"cap32_libretro.dll.zip" });

        // --- Philips CD-i ---
        updater.RegisterCore({ L"cdi2015", L"cdi2015_libretro.dll", L"cdi2015_libretro.dll.zip" });

        // --- Nintendo - Wii U ---
        updater.RegisterCore({ L"cemu", L"cemu_libretro.dll", L"cemu_libretro.dll.zip" });

        // --- Fantasy Consoles / Scripting Engines ---
        updater.RegisterCore({ L"chailove", L"chailove_libretro.dll", L"chailove_libretro.dll.zip" });
        updater.RegisterCore({ L"lowresnx", L"lowresnx_libretro.dll", L"lowresnx_libretro.dll.zip" });
        updater.RegisterCore({ L"lutro", L"lutro_libretro.dll", L"lutro_libretro.dll.zip" });
        updater.RegisterCore({ L"retro8", L"retro8_libretro.dll", L"retro8_libretro.dll.zip" });
        updater.RegisterCore({ L"squirreljme", L"squirreljme_libretro.dll", L"squirreljme_libretro.dll.zip" });
        updater.RegisterCore({ L"tic80", L"tic80_libretro.dll", L"tic80_libretro.dll.zip" });
        updater.RegisterCore({ L"uw8", L"uw8_libretro.dll", L"uw8_libretro.dll.zip" });
        updater.RegisterCore({ L"wasm4", L"wasm4_libretro.dll", L"wasm4_libretro.dll.zip" });

        // --- Nintendo - DS ---
        updater.RegisterCore({ L"desmume", L"desmume_libretro.dll", L"desmume_libretro.dll.zip" });
        updater.RegisterCore({ L"desmume2015", L"desmume2015_libretro.dll", L"desmume2015_libretro.dll.zip" });
        updater.RegisterCore({ L"melonds", L"melonds_libretro.dll", L"melonds_libretro.dll.zip" });
        updater.RegisterCore({ L"melondsds", L"melondsds_libretro.dll", L"melondsds_libretro.dll.zip" });
        updater.RegisterCore({ L"noods", L"noods_libretro.dll", L"noods_libretro.dll.zip" });

        // --- Nintendo - GameCube/Wii ---
        updater.RegisterCore({ L"dolphin", L"dolphin_libretro.dll", L"dolphin_libretro.dll.zip" });

        // --- DOS/PC ---
        updater.RegisterCore({ L"dosbox_core", L"dosbox_core_libretro.dll", L"dosbox_core_libretro.dll.zip" });
        updater.RegisterCore({ L"dosbox_pure", L"dosbox_pure_libretro.dll", L"dosbox_pure_libretro.dll.zip" });
        updater.RegisterCore({ L"dosbox_svn", L"dosbox_svn_libretro.dll", L"dosbox_svn_libretro.dll.zip" });
        updater.RegisterCore({ L"virtualxt", L"virtualxt_libretro.dll", L"virtualxt_libretro.dll.zip" });

        // --- Nintendo - Game Boy/Color/Advance ---
        updater.RegisterCore({ L"DoubleCherryGB", L"DoubleCherryGB_libretro.dll", L"DoubleCherryGB_libretro.dll.zip" });
        updater.RegisterCore({ L"fixgb", L"fixgb_libretro.dll", L"fixgb_libretro.dll.zip" });
        updater.RegisterCore({ L"gambatte", L"gambatte_libretro.dll", L"gambatte_libretro.dll.zip" });
        updater.RegisterCore({ L"gearboy", L"gearboy_libretro.dll", L"gearboy_libretro.dll.zip" });
        updater.RegisterCore({ L"gpsp", L"gpsp_libretro.dll", L"gpsp_libretro.dll.zip" });
        updater.RegisterCore({ L"irogb", L"irogb_libretro.dll", L"irogb_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_gba", L"mednafen_gba_libretro.dll", L"mednafen_gba_libretro.dll.zip" });
        updater.RegisterCore({ L"meteor", L"meteor_libretro.dll", L"meteor_libretro.dll.zip" });
        updater.RegisterCore({ L"mgba", L"mgba_libretro.dll", L"mgba_libretro.dll.zip" });
        updater.RegisterCore({ L"sameboy", L"sameboy_libretro.dll", L"sameboy_libretro.dll.zip" });
        updater.RegisterCore({ L"skyemu", L"skyemu_libretro.dll", L"skyemu_libretro.dll.zip" });
        updater.RegisterCore({ L"tgbdual", L"tgbdual_libretro.dll", L"tgbdual_libretro.dll.zip" });
        updater.RegisterCore({ L"vba_next", L"vba_next_libretro.dll", L"vba_next_libretro.dll.zip" });
        updater.RegisterCore({ L"vbam", L"vbam_libretro.dll", L"vbam_libretro.dll.zip" });

        // --- Cave Story engine ---
        updater.RegisterCore({ L"doukutsu_rs", L"doukutsu_rs_libretro.dll", L"doukutsu_rs_libretro.dll.zip" });
        updater.RegisterCore({ L"nxengine", L"nxengine_libretro.dll", L"nxengine_libretro.dll.zip" });

        // --- Wolfenstein 3D engine (ECWolf) ---
        updater.RegisterCore({ L"ecwolf", L"ecwolf_libretro.dll", L"ecwolf_libretro.dll.zip" });

        // --- Enterprise 128 ---
        updater.RegisterCore({ L"ep128emu_core", L"ep128emu_core_libretro.dll", L"ep128emu_core_libretro.dll.zip" });

        // --- SNK - Neo Geo ---
        updater.RegisterCore({ L"fbalpha2012_neogeo", L"fbalpha2012_neogeo_libretro.dll", L"fbalpha2012_neogeo_libretro.dll.zip" });
        updater.RegisterCore({ L"fbneo", L"fbneo_libretro.dll", L"fbneo_libretro.dll.zip" });
        updater.RegisterCore({ L"geolith", L"geolith_libretro.dll", L"geolith_libretro.dll.zip" });
        updater.RegisterCore({ L"neocd", L"neocd_libretro.dll", L"neocd_libretro.dll.zip" });

        // --- Nintendo - NES/Famicom ---
        updater.RegisterCore({ L"fceumm", L"fceumm_libretro.dll", L"fceumm_libretro.dll.zip" });
        updater.RegisterCore({ L"fixnes", L"fixnes_libretro.dll", L"fixnes_libretro.dll.zip" });
        updater.RegisterCore({ L"mesen", L"mesen_libretro.dll", L"mesen_libretro.dll.zip" });
        updater.RegisterCore({ L"nestopia", L"nestopia_libretro.dll", L"nestopia_libretro.dll.zip" });
        updater.RegisterCore({ L"quicknes", L"quicknes_libretro.dll", L"quicknes_libretro.dll.zip" });
        updater.RegisterCore({ L"rustynes", L"rustynes_libretro.dll", L"rustynes_libretro.dll.zip" });

        // --- Sega - Dreamcast ---
        updater.RegisterCore({ L"flycast", L"flycast_libretro.dll", L"flycast_libretro.dll.zip" });

        // --- Fairchild Channel F ---
        updater.RegisterCore({ L"freechaf", L"freechaf_libretro.dll", L"freechaf_libretro.dll.zip" });

        // --- Mattel Intellivision ---
        updater.RegisterCore({ L"freeintv", L"freeintv_libretro.dll", L"freeintv_libretro.dll.zip" });

        // --- Commodore - C64/128/VIC-20/PET/Plus4 ---
        updater.RegisterCore({ L"frodo", L"frodo_libretro.dll", L"frodo_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_x128", L"vice_x128_libretro.dll", L"vice_x128_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_x64", L"vice_x64_libretro.dll", L"vice_x64_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_x64sc", L"vice_x64sc_libretro.dll", L"vice_x64sc_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_xpet", L"vice_xpet_libretro.dll", L"vice_xpet_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_xplus4", L"vice_xplus4_libretro.dll", L"vice_xplus4_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_xscpu64", L"vice_xscpu64_libretro.dll", L"vice_xscpu64_libretro.dll.zip" });
        updater.RegisterCore({ L"vice_xvic", L"vice_xvic_libretro.dll", L"vice_xvic_libretro.dll.zip" });

        // --- Sinclair ZX Spectrum ---
        updater.RegisterCore({ L"fuse", L"fuse_libretro.dll", L"fuse_libretro.dll.zip" });
        updater.RegisterCore({ L"vaporspec", L"vaporspec_libretro.dll", L"vaporspec_libretro.dll.zip" });

        // --- Galaksija ---
        updater.RegisterCore({ L"galaksija", L"galaksija_libretro.dll", L"galaksija_libretro.dll.zip" });

        // --- ColecoVision ---
        updater.RegisterCore({ L"gearcoleco", L"gearcoleco_libretro.dll", L"gearcoleco_libretro.dll.zip" });
        updater.RegisterCore({ L"jollycv", L"jollycv_libretro.dll", L"jollycv_libretro.dll.zip" });

        // --- NEC - PC Engine/TurboGrafx/PC-FX ---
        updater.RegisterCore({ L"geargrafx", L"geargrafx_libretro.dll", L"geargrafx_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_pce_fast", L"mednafen_pce_fast_libretro.dll", L"mednafen_pce_fast_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_pce", L"mednafen_pce_libretro.dll", L"mednafen_pce_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_pcfx", L"mednafen_pcfx_libretro.dll", L"mednafen_pcfx_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_supergrafx", L"mednafen_supergrafx_libretro.dll", L"mednafen_supergrafx_libretro.dll.zip" });
        updater.RegisterCore({ L"pcee2", L"pcee2_libretro.dll", L"pcee2_libretro.dll.zip" });

        // --- Atari Lynx (alt core) ---
        updater.RegisterCore({ L"gearlynx", L"gearlynx_libretro.dll", L"gearlynx_libretro.dll.zip" });

        // --- Sega - Master System/Game Gear ---
        updater.RegisterCore({ L"gearsystem", L"gearsystem_libretro.dll", L"gearsystem_libretro.dll.zip" });
        updater.RegisterCore({ L"smsplus", L"smsplus_libretro.dll", L"smsplus_libretro.dll.zip" });

        // --- Game Music Emu (chiptune playback) ---
        updater.RegisterCore({ L"gme", L"gme_libretro.dll", L"gme_libretro.dll.zip" });

        // --- Game & Watch ---
        updater.RegisterCore({ L"gw", L"gw_libretro.dll", L"gw_libretro.dll.zip" });

        // --- Handhelds - Lynx/NeoGeo Pocket/WonderSwan/Virtual Boy/Supervision ---
        updater.RegisterCore({ L"handy", L"handy_libretro.dll", L"handy_libretro.dll.zip" });
        updater.RegisterCore({ L"holani", L"holani_libretro.dll", L"holani_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_lynx", L"mednafen_lynx_libretro.dll", L"mednafen_lynx_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_ngp", L"mednafen_ngp_libretro.dll", L"mednafen_ngp_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_vb", L"mednafen_vb_libretro.dll", L"mednafen_vb_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_wswan", L"mednafen_wswan_libretro.dll", L"mednafen_wswan_libretro.dll.zip" });
        updater.RegisterCore({ L"potator", L"potator_libretro.dll", L"potator_libretro.dll.zip" });
        updater.RegisterCore({ L"tamalibretro", L"tamalibretro_libretro.dll", L"tamalibretro_libretro.dll.zip" });

        // --- Atari ST ---
        updater.RegisterCore({ L"hatari", L"hatari_libretro.dll", L"hatari_libretro.dll.zip" });

        // --- Sega - Saturn / Sega CD-ish ---
        updater.RegisterCore({ L"kronos", L"kronos_libretro.dll", L"kronos_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_saturn", L"mednafen_saturn_libretro.dll", L"mednafen_saturn_libretro.dll.zip" });
        updater.RegisterCore({ L"same_cdi", L"same_cdi_libretro.dll", L"same_cdi_libretro.dll.zip" });
        updater.RegisterCore({ L"yabasanshiro", L"yabasanshiro_libretro.dll", L"yabasanshiro_libretro.dll.zip" });
        updater.RegisterCore({ L"yabause", L"yabause_libretro.dll", L"yabause_libretro.dll.zip" });
        updater.RegisterCore({ L"ymir", L"ymir_libretro.dll", L"ymir_libretro.dll.zip" });

        // --- MSX-like (Korean) - Zemmix ---
        updater.RegisterCore({ L"m2000", L"m2000_libretro.dll", L"m2000_libretro.dll.zip" });

        // --- Sony - PlayStation ---
        updater.RegisterCore({ L"mednafen_psx_hw", L"mednafen_psx_hw_libretro.dll", L"mednafen_psx_hw_libretro.dll.zip" });
        updater.RegisterCore({ L"mednafen_psx", L"mednafen_psx_libretro.dll", L"mednafen_psx_libretro.dll.zip" });
        updater.RegisterCore({ L"pcsx_rearmed", L"pcsx_rearmed_libretro.dll", L"pcsx_rearmed_libretro.dll.zip" });
        updater.RegisterCore({ L"swanstation", L"swanstation_libretro.dll", L"swanstation_libretro.dll.zip" });

        // --- Apple Macintosh (Mini vMac) ---
        updater.RegisterCore({ L"minivmac", L"minivmac_libretro.dll", L"minivmac_libretro.dll.zip" });

        // --- Nintendo - N64 ---
        updater.RegisterCore({ L"mupen64plus_next", L"mupen64plus_next_libretro.dll", L"mupen64plus_next_libretro.dll.zip" });
        updater.RegisterCore({ L"parallel_n64", L"parallel_n64_libretro.dll", L"parallel_n64_libretro.dll.zip" });

        // --- Japanese Computers - PC-98/PC-88/X68000/Sharp X1 ---
        updater.RegisterCore({ L"np2kai", L"np2kai_libretro.dll", L"np2kai_libretro.dll.zip" });
        updater.RegisterCore({ L"px68k", L"px68k_libretro.dll", L"px68k_libretro.dll.zip" });
        updater.RegisterCore({ L"quasi88", L"quasi88_libretro.dll", L"quasi88_libretro.dll.zip" });
        updater.RegisterCore({ L"x1", L"x1_libretro.dll", L"x1_libretro.dll.zip" });

        // --- Magnavox Odyssey2 ---
        updater.RegisterCore({ L"o2em", L"o2em_libretro.dll", L"o2em_libretro.dll.zip" });

        // --- Oberon system ---
        updater.RegisterCore({ L"oberon", L"oberon_libretro.dll", L"oberon_libretro.dll.zip" });

        // --- Tomb Raider engine (OpenLara) ---
        updater.RegisterCore({ L"openlara", L"openlara_libretro.dll", L"openlara_libretro.dll.zip" });

        // --- 3DO (Opera) ---
        updater.RegisterCore({ L"opera", L"opera_libretro.dll", L"opera_libretro.dll.zip" });

        // --- Sony - PlayStation 2 ---
        updater.RegisterCore({ L"pcsx2", L"pcsx2_libretro.dll", L"pcsx2_libretro.dll.zip" });
        updater.RegisterCore({ L"play", L"play_libretro.dll", L"play_libretro.dll.zip" });

        // --- Karaoke CD+G Player ---
        updater.RegisterCore({ L"pocketcdg", L"pocketcdg_libretro.dll", L"pocketcdg_libretro.dll.zip" });

        // --- Nintendo - Pokemon Mini ---
        updater.RegisterCore({ L"pokemini", L"pokemini_libretro.dll", L"pokemini_libretro.dll.zip" });

        // --- Sony - PocketStation ---
        updater.RegisterCore({ L"pokketstation", L"pokketstation_libretro.dll", L"pokketstation_libretro.dll.zip" });

        // --- Sony - PSP ---
        updater.RegisterCore({ L"ppsspp", L"ppsspp_libretro.dll", L"ppsspp_libretro.dll.zip" });

        // --- Doom engine (PrBoom) ---
        updater.RegisterCore({ L"prboom", L"prboom_libretro.dll", L"prboom_libretro.dll.zip" });

        // --- Amstrad/Sega? (RACE - unclear, verify) ---
        updater.RegisterCore({ L"race", L"race_libretro.dll", L"race_libretro.dll.zip" });

        // --- Flashback engine (REminiscence) ---
        updater.RegisterCore({ L"reminiscence", L"reminiscence_libretro.dll", L"reminiscence_libretro.dll.zip" });

        // --- Utility (ROM header cleaner, not an emulator) ---
        updater.RegisterCore({ L"romcleaner", L"romcleaner_libretro.dll", L"romcleaner_libretro.dll.zip" });

        // --- Adventure Game Engine (ScummVM) ---
        updater.RegisterCore({ L"scummvm", L"scummvm_libretro.dll", L"scummvm_libretro.dll.zip" });

        // --- Generic Handheld Clones (SunPlus SoC-based) ---
        updater.RegisterCore({ L"spmp8000emu", L"spmp8000emu_libretro.dll", L"spmp8000emu_libretro.dll.zip" });

        // --- Thomson MO/TO ---
        updater.RegisterCore({ L"theodore", L"theodore_libretro.dll", L"theodore_libretro.dll.zip" });

        // --- Quake engine (TyrQuake) ---
        updater.RegisterCore({ L"tyrquake", L"tyrquake_libretro.dll", L"tyrquake_libretro.dll.zip" });

        // --- Vircon32 (fantasy console) ---
        updater.RegisterCore({ L"vircon32", L"vircon32_libretro.dll", L"vircon32_libretro.dll.zip" });

        // --- Quake II/III engine ports ---
        updater.RegisterCore({ L"vitaquake2", L"vitaquake2_libretro.dll", L"vitaquake2_libretro.dll.zip" });
        updater.RegisterCore({ L"vitaquake2-rogue", L"vitaquake2-rogue_libretro.dll", L"vitaquake2-rogue_libretro.dll.zip" });
        updater.RegisterCore({ L"vitaquake2-xatrix", L"vitaquake2-xatrix_libretro.dll", L"vitaquake2-xatrix_libretro.dll.zip" });
        updater.RegisterCore({ L"vitaquake2-zaero", L"vitaquake2-zaero_libretro.dll", L"vitaquake2-zaero_libretro.dll.zip" });
        updater.RegisterCore({ L"vitaquake3", L"vitaquake3_libretro.dll", L"vitaquake3_libretro.dll.zip" });
    }
}
