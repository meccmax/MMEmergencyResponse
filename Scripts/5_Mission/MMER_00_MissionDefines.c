//------------------------------------------------------------------------------
// MM Emergency Response - build flags for the 5_Mission module.
//
// A SECOND defines file is not an oversight. Enforce compiles 3_Game, 4_World
// and 5_Mission as separate units, so a #define in Scripts/4_World/MMER_00_Defines.c
// is not visible here. Anything that gates UI code has to be declared in this
// file instead.
//
// Named 00_ so it compiles before everything else in 5_Mission.
//
// Uncomment a flag ONLY after adding the matching mod to requiredAddons in
// config.cpp. Building with a flag on and the mod absent fails to compile with
// "unknown type" errors, which is intended - it fails loudly at build time
// rather than quietly at 3am on a live server.
//------------------------------------------------------------------------------

// Draws the responder chat tag through DayZ Expansion's chat instead of the
// vanilla one.
//
// Turn this on if you run DayZ-Expansion-Chat (it is in Expansion Bundled).
// Expansion replaces the whole chat renderer, so the vanilla ChatLine hook in
// MMER_ChatLine.c never runs on an Expansion server and no tag appears.
//
// Requires: "DayZExpansion_Chat_Scripts" in requiredAddons[] in config.cpp.
#define MMER_EXPANSION_CHAT
