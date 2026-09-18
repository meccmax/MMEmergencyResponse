//------------------------------------------------------------------------------
// MM Emergency Response - map marker delivery.
//
// Three modes, chosen by "markerMode" in config.json:
//
//   0  COORDS       No pin anywhere. The panel gives the responder a grid
//                   reference, straight-line range, bearing and altitude
//                   difference and they navigate like a person would. No
//                   dependencies, nothing to verify, works on any server.
//                   THIS IS THE DEFAULT.
//
//   2  EXPANSION    DayZ Expansion SERVER marker. Global - EVERY PLAYER ON THE
//      _SERVER      SERVER SEES IT. That turns a MEDEVAC call into a public
//                   announcement that somebody is lying helpless at a precise
//                   grid, which is the same disclosure rosterMode 2 exists to
//                   prevent. Kept because a PvE or RP server may genuinely want
//                   it; not recommended anywhere else.
//
//   3  EXPANSION    DayZ Expansion PERSONAL marker, pushed to each responder
//      _RESPONDERS  individually and to nobody else. Precise, private, and it
//                   disappears when the case closes. This is the one to use.
//
// (Mode 1, a client-drawn marker on the vanilla map, was removed. It needed a
// modded MapMenu and an engine member name I could not verify, and it earned
// nothing that mode 0 does not already give you.)
//
// Mode 0 is also the runtime fallback: if Expansion fails to resolve, the
// adapter logs and degrades to it rather than throwing.
//
//------------------------------------------------------------------------------
// The Expansion API below was read from the mod's own source rather than
// guessed. The three facts it rests on:
//
//   1. ExpansionMarkerModule is reached with
//      CF_Modules<ExpansionMarkerModule>.Get(module) - Expansion uses exactly
//      this form itself in NamalskAdventure. (CF_ModuleCoreManager.Get() also
//      works; they are equivalent.)
//
//   2. CreateServerMarker(name, icon, position, colour, is3D, uid = "")
//      GENERATES ITS OWN UID when you pass "" - it builds
//      name + Math.RandomInt(0, int.MAX) internally. The uid it used is not
//      returned to you in any usable form, so RemoveServerMarker(name) never
//      matches and the marker stays on every player's map forever. We pass an
//      explicit uid; see MarkerUid().
//
//   3. A personal marker is ordinary client-side marker data:
//      ExpansionMarkerData.Create(ExpansionMapMarkerType.PERSONAL, uid), then
//      module.CreateMarker(data) / UpdateMarker(data) /
//      RemovePersonalMarkerByUID(uid). Expansion's own death marker works this
//      way - the server RPCs one client, which builds the marker locally. Mode
//      3 is that pattern with our own recipients list.
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

	// Stable, unique, and ours. Same string the client uses for a personal
	// marker, so both modes name a call's pin the same way.
	static string MarkerUid(int callId)
	{
		return "MMER_" + callId.ToString();
	}

	//--------------------------------------------------------------------------
	// Server marker (mode 2). Mode 3 is driven from MMER_Manager, which is the
	// only place that knows who the responders currently are.
	//--------------------------------------------------------------------------
	static void Place(MMER_Call call, MMER_Settings settings)
	{
		if (!call || !settings)
			return;

		if (settings.markerMode != MMER_MarkerMode.EXPANSION_SERVER)
			return;		// COORDS needs nothing; RESPONDERS is pushed per player

		if (!PlaceExpansion(call, settings))
			MMER_Log.Warn("Expansion marker unavailable - responders will navigate by grid and bearing.");
	}

	static void Remove(MMER_Call call, MMER_Settings settings)
	{
		if (!call || !settings)
			return;

		if (settings.markerMode == MMER_MarkerMode.EXPANSION_SERVER)
			RemoveExpansion(call);
	}

	//--------------------------------------------------------------------------
	// Builds the payload one responder gets. Server side.
	//--------------------------------------------------------------------------
	static MMER_MarkerPayload BuildPayload(MMER_Call call, MMER_Settings settings, bool clear)
	{
		MMER_MarkerPayload p = new MMER_MarkerPayload;
		p.callId = call.id;

		if (clear)
		{
			p.clear = 1;
			return p;
		}

		p.x		= call.posX;
		p.y		= call.posY;
		p.z		= call.posZ;
		p.name	= string.Format("%1 #%2", settings.teamName, call.id);
		p.icon	= settings.markerIcon;
		p.colour = MMER_Color.Parse(settings.markerColor, MMER_CallState.ToColor(call.state));
		p.is3D	= settings.marker3D;

		return p;
	}

	//--------------------------------------------------------------------------
	// Client side. Applies whatever the server just handed us.
	//
	// Deliberately does not check whether the viewer "should" have a marker -
	// that decision was made on the server, which is the only place it can be
	// made safely. The client just draws.
	//--------------------------------------------------------------------------
	static void ApplyClient(string json)
	{
		#ifdef MMER_EXPANSION
		// Belt and braces. SERVER_MARKER is only ever sent to clients, but the
		// same OnRPC handles inbound client traffic on the server, and a marker
		// module call on the server side would be a confusing no-op at best.
		if (!GetGame() || !GetGame().IsClient())
			return;

		MMER_MarkerPayload p = MMER_MarkerPayload.FromJson(json);
		if (!p)
			return;

		ExpansionMarkerModule module;
		if (!CF_Modules<ExpansionMarkerModule>.Get(module) || !module)
			return;

		string uid = p.Uid();

		if (p.clear == 1)
		{
			module.RemovePersonalMarkerByUID(uid);
			return;
		}

		// Re-placing an existing marker updates it rather than stacking a
		// second pin: the position is refreshed every sync while the case is
		// open, so without this the map would fill up with one marker per tick.
		// persist = false, deliberately. Expansion defaults personal markers to
		// persistent, which writes them to the player's local marker file and
		// brings them back on every login. A responder who logs out mid-case
		// would then carry a pin pointing at a patient who was rescued, died or
		// respawned hours ago, and our clear would never reach them because
		// they were offline when it went out. A marker for a live case should
		// not outlive the case.
		ExpansionMarkerData data = ExpansionMarkerData.Create(ExpansionMapMarkerType.PERSONAL, uid, false);
		if (!data)
			return;

		data.SetUID(uid);
		data.SetName(p.name);
		data.SetIcon(p.icon);
		data.SetColor(p.colour);
		data.SetPosition(p.Position());
		data.Set3D(p.is3D == 1);

		// Locked: the responder should not be able to drag the patient's pin
		// somewhere else and then wonder why nobody is there.
		data.SetLockState(true);

		module.RemovePersonalMarkerByUID(uid);
		module.CreateMarker(data);
		#endif
	}

	//--------------------------------------------------------------------------
	// Expansion bridge for the global server marker.
	//--------------------------------------------------------------------------
	static bool PlaceExpansion(MMER_Call call, MMER_Settings settings)
	{
		#ifdef MMER_EXPANSION
		ExpansionMarkerModule module;
		if (!CF_Modules<ExpansionMarkerModule>.Get(module) || !module)
			return false;

		string name = string.Format("%1 #%2", settings.teamName, call.id);
		string uid = MarkerUid(call.id);
		int colour = MMER_Color.Parse(settings.markerColor, MMER_CallState.ToColor(call.state));

		// The explicit uid is not optional. Passing "" makes Expansion invent
		// one we never see, and the marker then outlives the call on every
		// player's map for the rest of the server's uptime.
		module.RemoveServerMarker(uid);
		module.CreateServerMarker(name, settings.markerIcon, call.Position(), colour,
			settings.marker3D == 1, uid);

		MarkerUids().Set(call.id, uid);
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
			uid = MarkerUid(call.id);

		ExpansionMarkerModule module;
		if (CF_Modules<ExpansionMarkerModule>.Get(module) && module)
			module.RemoveServerMarker(uid);

		if (MarkerUids().Contains(call.id))
			MarkerUids().Remove(call.id);
		#endif
	}

	static bool IsAvailable()
	{
		#ifdef MMER_EXPANSION
		return true;
		#else
		return false;
		#endif
	}
}
