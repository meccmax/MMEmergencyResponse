//------------------------------------------------------------------------------
// MM Emergency Response - Terje Medicine integration.
//
//  >>> THIS IS THE ONLY FILE THAT KNOWS ANYTHING ABOUT TERJE. <<<
//
// The mod compiles and runs with no Terje mods present. The Steam Workshop
// build ships with MMER_TERJE on; a source build can turn it off in
// Scripts/4_World/MMER_00_Defines.c and drop TerjeCore/TerjeMedicine from
// requiredAddons in config.cpp, at which point every terje: row reads "--".
//
// The API used below was read from the official public interface repository
// (TerjeBruoygard/TerjeModsScripting). Three facts it establishes, all of
// which this file depends on:
//
//   1. PlayerBase.GetTerjeStats() returns a TerjePlayerStats, and returns it
//      only on a dedicated server or for the locally controlled player. We
//      only ever call it server-side, which is the case that always resolves.
//
//   2. TerjePlayerStats has NO generic float accessor. Its readings are
//      registered as named records on TerjePlayerRecordsBase, reachable by
//      TryGetIntValue / TryGetFloatValue / TryGetBoolValue - each of which
//      returns false for an id that is not registered. That false is exactly
//      the "this reading is unavailable" signal the diagnostics panel wants,
//      so an unknown id degrades to "--" instead of to a wrong number.
//
//   3. Radiation is not a medicine record at all. It comes from
//      PlayerBase.GetTerjeRadiation(), which TerjeCore defines and
//      TerjeRadiation implements; without TerjeRadiation it returns 0.
//
// Because the stat ids come from config.json, adding or renaming readings is a
// JSON edit, not a script change. Config uses friendly names ("sepsis"), which
// MapId() translates to Terje's own record ids ("tm.sep_l"). A raw record id
// passes straight through, so anything not covered by the table below can
// still be read by putting the real id in config.json.
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

	//--------------------------------------------------------------------------
	// ToLower() mutates in place and returns an int, so it cannot be used
	// inline. Assignment copies, so the caller's string is untouched.
	//--------------------------------------------------------------------------
	static string Lower(string s)
	{
		string t = s;
		t.ToLower();
		return t;
	}

	//--------------------------------------------------------------------------
	// Friendly config id -> Terje record id.
	//
	// Levels ("_l") are 0-3 severity steps and are what a responder actually
	// wants to see; the matching raw values ("_v") are server-only internals
	// and are deliberately not mapped. Counters are plain integers. Anything
	// that is really a yes/no reads back as 1 or 0.
	//--------------------------------------------------------------------------
	static string MapId(string friendly)
	{
		string k = Lower(friendly);

		// Already a Terje record id - pass it through untouched.
		if (k.IndexOf("tm.") == 0)
			return friendly;

		// Conditions
		if (k == "sepsis")			return "tm.sep_l";
		if (k == "pain")			return "tm.pain_l";
		if (k == "influenza")		return "tm.inf_l";
		if (k == "zvirus")			return "tm.zmb_l";
		if (k == "poison")			return "tm.tox_l";
		if (k == "biohazard")		return "tm.bio_l";
		if (k == "rabies")			return "tm.rab_l";
		if (k == "overdose")		return "tm.ovd_l";
		if (k == "contusion")		return "tm.cnc_i";
		if (k == "viscera")			return "tm.vis";
		if (k == "mind")			return "tm.mnd_l";
		if (k == "sleeping")		return "tm.slp_l";

		// Wounds
		if (k == "hematoma")		return "tm.hmt_c";
		if (k == "bullethit")		return "tm.blt_c";
		if (k == "stabwound")		return "tm.stb_c";
		if (k == "bandagesclean")	return "tm.bndg_c";
		if (k == "bandagesdirty")	return "tm.bndg_d";
		if (k == "suturesclean")	return "tm.sut_c";
		if (k == "suturesdirty")	return "tm.sut_d";

		// Treatments on board
		if (k == "painkiller")		return "tm.pain+hl";
		if (k == "antibiotics")		return "tm.inf+hl";
		if (k == "antisepsis")		return "tm.sep+hl";
		if (k == "antipoison")		return "tm.tox+hl";
		if (k == "antibiohazard")	return "tm.bio+hl";
		if (k == "rabiescure")		return "tm.rab+hl";
		if (k == "zantidot")		return "tm.zmv+hl";
		if (k == "hemostatic")		return "tm.hms+hl";
		if (k == "bloodregen")		return "tm.blr+hl";
		if (k == "salve")			return "tm.hmt+hl";
		if (k == "adrenalin")		return "tm.adr_l";
		if (k == "disinfected")		return "tm.disinf_l";

		return "";
	}

	#ifdef MMER_TERJE
	//--------------------------------------------------------------------------
	// Single point of contact with the Terje API.
	//
	// The records are typed, and there is no way to ask which type an id is
	// without knowing it in advance, so this tries each accessor in turn. Only
	// one of the three can match a registered id, and all three return false
	// for an id that was never registered - which is what makes an unknown or
	// disabled reading degrade to "--" rather than to a plausible-looking 0.
	//--------------------------------------------------------------------------
	static bool GetStatTerje(PlayerBase player, string statId, out float value)
	{
		value = 0;

		// Radiation lives on PlayerBase, not in the medicine records.
		if (Lower(statId) == "radiation")
		{
			value = player.GetTerjeRadiation();
			return true;
		}

		string recordId = MapId(statId);
		if (recordId == "")
			return false;

		TerjePlayerStats stats = player.GetTerjeStats();
		if (!stats)
			return false;

		float floatValue = 0;
		if (stats.TryGetFloatValue(recordId, floatValue))
		{
			value = floatValue;
			return true;
		}

		int intValue = 0;
		if (stats.TryGetIntValue(recordId, intValue))
		{
			value = intValue;
			return true;
		}

		bool boolValue = false;
		if (stats.TryGetBoolValue(recordId, boolValue))
		{
			if (boolValue)
				value = 1;
			else
				value = 0;

			return true;
		}

		return false;
	}
	#endif

	//--------------------------------------------------------------------------
	// Called during pre-restart stabilisation. Vanilla stabilisation happens in
	// MMER_Manager; anything Terje-specific belongs here so the rest of the mod
	// stays portable.
	//
	// Deliberately does nothing by default. The point of the pre-restart pass
	// is to stop people bleeding out through a scheduled restart, not to hand
	// out free treatment - and silently curing sepsis would undo a responder's
	// work. Uncomment what your server should reset; the setters below are the
	// real TerjeMedicine API and clamp their own inputs.
	//--------------------------------------------------------------------------
	static void Stabilize(PlayerBase player)
	{
		#ifdef MMER_TERJE
		if (!player)
			return;

		TerjePlayerStats stats = player.GetTerjeStats();
		if (!stats)
			return;

		// stats.SetPainValue(0);
		// stats.SetSepsisValue(0);
		// stats.SetHematomas(0);
		// stats.SetContusionValue(0);
		#endif
	}
}
