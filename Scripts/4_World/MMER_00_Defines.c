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
// Requires: TerjeCore, TerjeMedicine (both listed in requiredAddons in
// config.cpp). On the Steam Workshop build this is ON. Comment it out and
// remove those two entries from requiredAddons to build without Terje - the
// terje: rows then read "--" and nothing else changes.
#define MMER_TERJE

// Enables the DayZ Expansion map markers (markerMode 2 and 3).
// Requires: DayZExpansion_Navigation_Scripts, which pulls in
// DayZExpansion_Core_Scripts and Community Framework. Expansion already
// depends on CF, so a server running Expansion has it.
//
// The flag only compiles the code in. markerMode still defaults to 0, so
// turning this on changes nothing until the operator asks for a marker.
#define MMER_EXPANSION
