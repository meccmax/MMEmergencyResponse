//------------------------------------------------------------------------------
// MM Emergency Response - build flags.
//
// This file is named 00_ so it compiles before everything else in 4_World and
// the flags below are visible to the adapters.
//
// Uncomment a flag ONLY after adding the matching mod to requiredAddons and
// dependencies in config.cpp. Building with a flag on and the mod absent will
// fail to compile with "unknown type" errors, which is the intended behaviour -
// it fails loudly at build time instead of quietly at 3am on a live server.
//------------------------------------------------------------------------------

// Enables the Terje Medicine diagnostics rows.
// Requires: TerjeCore, TerjeMedicine
// #define MMER_TERJE

// Enables DayZ Expansion server markers (markerMode 2).
// Requires: DayZ-Expansion-Core (and CF, which Expansion already pulls in)
// #define MMER_EXPANSION
