//------------------------------------------------------------------------------
// MM Emergency Response - map marker delivery.
//
// Two modes, chosen by "markerMode" in config.json:
//
//   0  COORDS      No pin anywhere. The panel gives the responder a grid
//                  reference, straight-line range, bearing and altitude
//                  difference and they navigate like a person would. No
//                  dependencies, nothing to verify, works on any server.
//                  THIS IS THE DEFAULT.
//
//   2  EXPANSION   DayZ Expansion server marker. Requires the MMER_EXPANSION
//                  build define and DayZExpansion_Core in requiredAddons.
//                  Note that Expansion SERVER markers are global - every player
//                  on the server sees them, not just responders.
//
// (Mode 1, a client-drawn marker on the vanilla map, was removed. It needed a
// modded MapMenu and an engine member name I could not verify, and it earned
// nothing that mode 0 does not already give you.)
//
// Mode 0 is also the runtime fallback: if Expansion fails to resolve, the
// adapter logs and degrades to it rather than throwing.
//------------------------------------------------------------------------------

class MMER_MarkerAdapter
{
	static const int MODE_COORDS	= 0;
	static const int MODE_EXPANSION	= 2;

	static ref map<int, string> s_ExpansionMarkerUids;

	static map<int, string> MarkerUids()
	{
		if (!s_ExpansionMarkerUids)
			s_ExpansionMarkerUids = new map<int, string>;
		return s_ExpansionMarkerUids;
	}

	//--------------------------------------------------------------------------
	// Called when a call is created, accepted, or re-marked.
	//--------------------------------------------------------------------------
	static void Place(MMER_Call call, MMER_Settings settings)
	{
		if (!call || !settings)
			return;

		if (settings.markerMode != MODE_EXPANSION)
			return;		// MODE_COORDS needs nothing - the panel already has it

		if (!PlaceExpansion(call, settings))
			MMER_Log.Warn("Expansion marker unavailable - responders will navigate by grid and bearing.");
	}

	static void Remove(MMER_Call call, MMER_Settings settings)
	{
		if (!call || !settings)
			return;

		if (settings.markerMode == MODE_EXPANSION)
			RemoveExpansion(call);
	}

	//--------------------------------------------------------------------------
	// Expansion bridge. Isolated so a signature change is a one-file fix.
	//
	// VERIFY ON FIRST COMPILE (only if you enable MMER_EXPANSION) against your
	// installed DayZ-Expansion-Core:
	//   ExpansionMarkerModule.CreateServerMarker(name, icon, position, colour, is3D)
	//--------------------------------------------------------------------------
	static bool PlaceExpansion(MMER_Call call, MMER_Settings settings)
	{
		#ifdef MMER_EXPANSION
		ExpansionMarkerModule module;
		if (!CF_Modules<ExpansionMarkerModule>.Get(module) || !module)
			return false;

		string name = string.Format("%1 #%2", settings.teamName, call.id);
		int colour = MMER_Color.Parse(settings.markerColor, MMER_CallState.ToColor(call.state));

		module.CreateServerMarker(name, settings.markerIcon, call.Position(), colour, false);
		MarkerUids().Set(call.id, name);
		return true;
		#else
		return false;
		#endif
	}

	static void RemoveExpansion(MMER_Call call)
	{
		#ifdef MMER_EXPANSION
		string uid;
		if (!MarkerUids().Find(call.id, uid))
			return;

		ExpansionMarkerModule module;
		if (CF_Modules<ExpansionMarkerModule>.Get(module) && module)
			module.RemoveServerMarker(uid);

		MarkerUids().Remove(call.id);
		#endif
	}
}
