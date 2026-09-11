//------------------------------------------------------------------------------
// MM Emergency Response - Terje Medicine integration.
//
//  >>> THIS IS THE ONLY FILE THAT KNOWS ANYTHING ABOUT TERJE. <<<
//
// The mod compiles and runs with no Terje mods present. To turn the Terje
// readings on:
//
//   1. Add  MMER_TERJE  to the "defines" list in config.cpp (see the comment
//      block in that file - it is already written out, just uncommented).
//   2. Add TerjeCore and TerjeMedicine to "dependencies" in config.cpp.
//   3. Confirm the accessor in GetStatTerje() below matches the signature in
//      TerjeBruoygard/TerjeModsScripting for the version you run, and fix the
//      single line if it differs.
//
// Because the stat ids come from config.json, adding or renaming readings after
// that is a JSON edit, not a script change.
//------------------------------------------------------------------------------

class MMER_TerjeAdapter
{
	static bool s_WarnedMissing = false;

	static bool IsAvailable()
	{
		#ifdef MMER_TERJE
		return true;
		#else
		return false;
		#endif
	}

	static bool GetStat(PlayerBase player, string statId, out float value)
	{
		value = 0;

		if (!player || statId == "")
			return false;

		MMER_Settings settings = MMER_SettingsLoader.Get();
		if (settings && settings.terjeEnabled == 0)
			return false;

		#ifdef MMER_TERJE
		return GetStatTerje(player, statId, value);
		#else
		if (!s_WarnedMissing)
		{
			s_WarnedMissing = true;
			MMER_Log.Info("Terje diagnostics requested but the mod was built without MMER_TERJE - those rows will read '--'.");
		}
		return false;
		#endif
	}

	#ifdef MMER_TERJE
	//--------------------------------------------------------------------------
	// Single point of contact with the Terje API.
	//
	// TerjeCore exposes the per-player stat registry off PlayerBase. Both the
	// modern and the older accessor are shown; keep whichever one your installed
	// TerjeCore version provides and delete the other.
	//--------------------------------------------------------------------------
	static bool GetStatTerje(PlayerBase player, string statId, out float value)
	{
		value = 0;

		TerjePlayerStats stats = player.GetTerjeStats();
		if (!stats)
			return false;

		// Terje stats are float-valued and the getter reports whether the id is
		// registered at all, which is exactly the "unavailable" signal we want.
		return stats.GetStatValue(statId, value);

		// --- older TerjeCore builds used this shape instead: -----------------
		// float level;
		// if (!stats.GetStatLevel(statId, level))
		//     return false;
		// value = level;
		// return true;
	}
	#endif

	//--------------------------------------------------------------------------
	// Called during pre-restart stabilisation. Vanilla stabilisation happens in
	// MMER_Manager; anything Terje-specific (clearing sepsis, topping up
	// hemostatic, etc.) belongs here so the rest of the mod stays portable.
	//--------------------------------------------------------------------------
	static void Stabilize(PlayerBase player)
	{
		#ifdef MMER_TERJE
		if (!player)
			return;

		TerjePlayerStats stats = player.GetTerjeStats();
		if (!stats)
			return;

		// Deliberately conservative: we are keeping people alive across a
		// restart, not curing them. Uncomment what your server should reset.
		//
		// stats.SetStatValue("pain",     0.0);
		// stats.SetStatValue("sepsis",   0.0);
		// stats.SetStatValue("hematoma", 0.0);
		#endif
	}
}
