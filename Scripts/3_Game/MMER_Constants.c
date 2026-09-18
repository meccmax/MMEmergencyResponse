//------------------------------------------------------------------------------
// MM Emergency Response - shared constants, enums and small utilities
// Loaded in 3_Game so both client and server see the same symbols.
//------------------------------------------------------------------------------

class MMER_Const
{
	static const string MOD_NAME			= "MMEmergencyResponse";
	static const string PROFILE_DIR			= "$profile:MMEmergency";
	static const string CONFIG_PATH			= "$profile:MMEmergency/config.json";
	static const string ARCHIVE_PATH		= "$profile:MMEmergency/archive.json";
	static const string ACTIVE_PATH			= "$profile:MMEmergency/active.json";
	static const string LOG_PATH			= "$profile:MMEmergency/emergency.log";

	static const string LAYOUT_DIR			= "MMEmergencyResponse/GUI/layouts/";

	// Max calls serialised into a single RPC payload. Keeps the packet small.
	static const int	MAX_SYNC_CALLS		= 40;

	// Archive records per RPC page.
	static const int	MAX_ARCHIVE_PAGE	= 25;

	// Body characters per RPC chunk. A Param1<string> above some size is dropped
	// by the engine with no error at either end: RPCSingleParam returns normally
	// and OnRPC simply never fires on the client. Measured on this server, the
	// settings payload (323 bytes) and the roster (197) always arrived, while
	// the archive page (2007, logged as sent) and the call list with diagnostics
	// (~2300) never did. The exact cut-off is undocumented, so this sits below
	// the smallest payload proven to work rather than guessing at the ceiling.
	// Anything longer is split and reassembled; extra chunks cost far less than
	// another round of silent failures.
	static const int	RPC_CHUNK			= 240;

	// Refuse to reassemble beyond this - a malformed or hostile header must not
	// be able to make the client allocate without bound. A full 25-record
	// archive page is about 38 chunks.
	static const int	RPC_MAX_CHUNKS		= 128;

	// Inbound limits. Every client-to-server payload this mod sends is a small
	// JSON object; anything larger is a client trying to make the server work
	// on its behalf. The parse used to happen before any role check.
	static const int	MAX_CLIENT_PAYLOAD		= 512;

	// Floor between accepted requests from one player, and a longer floor for
	// the two that fan out (HELLO sends three payloads, an archive page up to
	// ~38 RPCs).
	static const int	RPC_MIN_INTERVAL_MS			= 250;
	static const int	RPC_EXPENSIVE_INTERVAL_MS	= 2000;

	// Verbose client-side tracing (RPC payloads, click routing, hotkey path).
	// Flip to true when something in the UI needs diagnosing again.
	static const bool	DEBUG				= false;
}

// RPC ids.
//
// Moved to the 87000 block on documented advice: mods should use IDs well
// above the vanilla ERPCs enum, and two mods sharing an integer will intercept
// each other's traffic. The previous 27810 block sits in a range other mods
// (Expansion, VPP, LBmaster) plausibly use.
class MMER_RPC
{
	static const int CLIENT_HELLO			= 87000; // C->S  "give me my state"
	static const int CLIENT_CALL_REQUEST	= 87001; // C->S  patient presses the button
	static const int CLIENT_CALL_CANCEL		= 87002; // C->S  patient cancels
	static const int CLIENT_ACCEPT			= 87003; // C->S  medic accepts a call
	static const int CLIENT_COMPLETE		= 87004; // C->S  medic completes a call
	static const int CLIENT_ABANDON			= 87005; // C->S  medic releases a call
	static const int CLIENT_MARK			= 87006; // C->S  medic (re)places the marker
	static const int CLIENT_ADMIN_ADD		= 87007; // C->S  admin adds a team member
	static const int CLIENT_ADMIN_REMOVE	= 87008; // C->S  admin removes a team member
	static const int CLIENT_ADMIN_LIST		= 87009; // C->S  admin requests roster
	static const int CLIENT_REQ_ARCHIVE		= 87010; // C->S  medic requests archive page

	static const int SERVER_STATE			= 87050; // S->C  personal state (role, cooldown, my call)
	static const int SERVER_CALLLIST		= 87051; // S->C  full active call list (team only)
	static const int SERVER_ROSTER			= 87052; // S->C  admin roster
	static const int SERVER_ARCHIVE			= 87053; // S->C  archive page
	static const int SERVER_TOAST			= 87054; // S->C  short message + sound cue
	static const int SERVER_SETTINGS		= 87055; // S->C  client-relevant settings
	static const int SERVER_TAGS			= 87056; // S->C  online responder/admin names, for chat tags
	static const int SERVER_MARKER			= 87057; // S->C  place/clear a personal map marker on this client

	// The range OnRPC accepts. Named rather than written as the first and last
	// ids: adding SERVER_TAGS past the old literal upper bound would otherwise
	// have made it fall straight through the guard.
	static const int FIRST					= 87000;
	static const int LAST					= 87057;
}

class MMER_CallState
{
	static const int NEW					= 0;
	static const int IN_PROGRESS			= 1;
	static const int DECEASED				= 2;
	static const int COMPLETED				= 3;
	static const int CANCELLED				= 4;
	static const int EXPIRED				= 5;

	static string ToLabel(int s)
	{
		switch (s)
		{
			case MMER_CallState.NEW:			return "#STR_MMER_STATE_NEW";
			case MMER_CallState.IN_PROGRESS:	return "#STR_MMER_STATE_INPROGRESS";
			case MMER_CallState.DECEASED:		return "#STR_MMER_STATE_DECEASED";
			case MMER_CallState.COMPLETED:		return "#STR_MMER_STATE_COMPLETED";
			case MMER_CallState.CANCELLED:		return "#STR_MMER_STATE_CANCELLED";
			case MMER_CallState.EXPIRED:		return "#STR_MMER_STATE_EXPIRED";
		}
		return "?";
	}

	// Plain text for logs and webhooks. ToLabel returns a stringtable KEY, which
	// is correct for SetText and useless in a log file.
	static string ToPlain(int s)
	{
		switch (s)
		{
			case MMER_CallState.NEW:			return "NEW";
			case MMER_CallState.IN_PROGRESS:	return "IN PROGRESS";
			case MMER_CallState.DECEASED:		return "PATIENT LOST";
			case MMER_CallState.COMPLETED:		return "COMPLETED";
			case MMER_CallState.CANCELLED:		return "CANCELLED";
			case MMER_CallState.EXPIRED:		return "EXPIRED";
		}
		return "UNKNOWN";
	}

	static int ToColor(int s)
	{
		switch (s)
		{
			case MMER_CallState.NEW:			return 0xFFE04B4B; // red    - unclaimed
			case MMER_CallState.IN_PROGRESS:	return 0xFFE0A94B; // amber  - claimed
			case MMER_CallState.DECEASED:		return 0xFF8A8A8A; // grey   - lost
			case MMER_CallState.COMPLETED:		return 0xFF4BE07A; // green  - saved
		}
		return 0xFF7A7A7A;
	}

	static bool IsClosed(int s)
	{
		return s >= MMER_CallState.DECEASED;
	}
}

class MMER_Role
{
	static const int NONE					= 0;
	static const int MEMBER					= 1;
	static const int ADMIN					= 2;
}

//------------------------------------------------------------------------------
// Who counts as a responder.
//
// The roster is not only "who may accept" - it is "who may SEE". The dispatch
// panel carries every open call's patient name, grid reference and live vitals,
// so opening it up is a visibility decision before it is an access decision. On
// a PvP server an unrestricted queue is a live feed of who is helpless and
// exactly where, which is why ROSTER is the default and why OPEN_REDACTED
// exists at all.
//------------------------------------------------------------------------------
class MMER_RosterMode
{
	// Only adminIds and teamIds. The roster is the whitelist.
	static const int ROSTER					= 0;

	// Anyone carrying a qualifying radio is a responder, with the full panel.
	// Right for PvE and heavy-RP servers; on a PvP server this is a raid feed.
	static const int OPEN					= 1;

	// Anyone carrying a qualifying radio is a responder, but a responder who is
	// not on the roster sees no patient name, no position and no vitals until
	// they ACCEPT the case - at which point their name is on it and it is in
	// Discord. They can still see that work exists, which is all they need in
	// order to volunteer.
	static const int OPEN_REDACTED			= 2;

	static bool IsOpen(int mode)
	{
		return mode == OPEN || mode == OPEN_REDACTED;
	}

	static string ToLabel(int mode)
	{
		if (mode == OPEN)
			return "OPEN (anyone with a qualifying radio, full detail)";
		if (mode == OPEN_REDACTED)
			return "OPEN, need-to-know (anyone with a qualifying radio; non-roster sees no name, position or vitals until they accept)";
		return "ROSTER ONLY (adminIds + teamIds)";
	}
}

//------------------------------------------------------------------------------
// How a responder is told where the patient is.
//
// The important distinction is who ELSE learns the position. A DayZ Expansion
// SERVER marker is global: every player on the server sees it, which turns a
// MEDEVAC call into a public announcement that somebody is lying unconscious at
// a precise grid. That is the same disclosure rosterMode 2 exists to prevent,
// so it is not the default and the boot log says so out loud.
//------------------------------------------------------------------------------
class MMER_MarkerMode
{
	// No pin anywhere. The panel's grid, range, bearing and altitude are the
	// whole locate mechanism. No dependencies. THE DEFAULT.
	static const int COORDS					= 0;

	// Expansion SERVER marker - global, visible to every player on the server.
	static const int EXPANSION_SERVER		= 2;

	// Expansion PERSONAL marker, pushed only to players who are responders at
	// that moment, and never to a redacted viewer under rosterMode 2. Precise,
	// and it disappears for everyone when the case closes.
	static const int EXPANSION_RESPONDERS	= 3;

	static bool IsExpansion(int mode)
	{
		return mode == EXPANSION_SERVER || mode == EXPANSION_RESPONDERS;
	}

	static string ToLabel(int mode)
	{
		if (mode == EXPANSION_SERVER)
			return "EXPANSION SERVER MARKER - WARNING: global, every player on the server sees the patient's position";
		if (mode == EXPANSION_RESPONDERS)
			return "EXPANSION PERSONAL MARKER - responders only";
		return "COORDS ONLY (grid, range and bearing in the panel)";
	}
}

class MMER_Toast
{
	static const int PLAIN					= 0;
	static const int INCOMING				= 1; // plays the "incoming call" cue
	static const int FATAL					= 2; // plays the "patient lost" cue
	static const int ERROR					= 3;
}

//------------------------------------------------------------------------------
// Real-world clock helpers. GetYearMonthDayUTC / GetHourMinuteSecondUTC are
// vanilla globals; everything else here is plain arithmetic so it behaves the
// same on Windows and Linux servers.
//------------------------------------------------------------------------------
class MMER_Time
{
	static int NowUnix()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDayUTC(y, mo, d);
		GetHourMinuteSecondUTC(h, mi, s);
		return DaysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
	}

	// Howard Hinnant's days_from_civil, integer-only.
	static int DaysFromCivil(int y, int m, int d)
	{
		if (m <= 2)
			y -= 1;

		int era;
		if (y >= 0)
			era = y / 400;
		else
			era = (y - 399) / 400;

		int yoe = y - era * 400;						// [0, 399]

		int mp;
		if (m > 2)
			mp = m - 3;
		else
			mp = m + 9;

		int doy = (153 * mp + 2) / 5 + d - 1;			// [0, 365]
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;// [0, 146096]
		return era * 146097 + doe - 719468;
	}

	static string StampUTC()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDayUTC(y, mo, d);
		GetHourMinuteSecondUTC(h, mi, s);
		return string.Format("%1-%2-%3 %4:%5:%6 UTC", y, Pad2(mo), Pad2(d), Pad2(h), Pad2(mi), Pad2(s));
	}

	static string Pad2(int v)
	{
		if (v < 10)
			return "0" + v.ToString();
		return v.ToString();
	}

	// "1h 04m" / "42s" - for elapsed columns in the panel.
	static string Duration(int seconds)
	{
		if (seconds < 0)
			seconds = 0;

		if (seconds < 60)
			return seconds.ToString() + "s";

		int mins = seconds / 60;
		if (mins < 60)
			return mins.ToString() + "m " + Pad2(seconds % 60) + "s";

		int hours = mins / 60;
		return hours.ToString() + "h " + Pad2(mins % 60) + "m";
	}

	// Minutes until the next "HH:MM" entry in a UTC schedule. -1 when empty.
	static int MinutesUntilNext(array<string> schedule)
	{
		if (!schedule || schedule.Count() == 0)
			return -1;

		int h, mi, s;
		GetHourMinuteSecondUTC(h, mi, s);
		int nowMin = h * 60 + mi;

		int best = -1;
		for (int i = 0; i < schedule.Count(); i++)
		{
			int rh, rm;
			if (!ParseHHMM(schedule.Get(i), rh, rm))
				continue;

			int delta = (rh * 60 + rm) - nowMin;
			if (delta < 0)
				delta += 1440;

			if (best < 0 || delta < best)
				best = delta;
		}
		return best;
	}

	static bool ParseHHMM(string txt, out int hour, out int minute)
	{
		hour = 0;
		minute = 0;

		TStringArray parts = new TStringArray;
		txt.Split(":", parts);
		if (parts.Count() != 2)
			return false;

		hour = parts.Get(0).ToInt();
		minute = parts.Get(1).ToInt();
		return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
	}
}

//------------------------------------------------------------------------------
// "0xAARRGGBB" -> int. EnScript's ToInt() is decimal-only, so hex colours from
// config.json have to be parsed by hand.
//------------------------------------------------------------------------------
class MMER_Color
{
	static int Parse(string txt, int fallback)
	{
		txt.Trim();
		if (txt == "")
			return fallback;

		string body = txt;
		if (body.IndexOf("0x") == 0 || body.IndexOf("0X") == 0)
			body = body.Substring(2, body.Length() - 2);
		else if (body.Get(0) == "#")
			body = body.Substring(1, body.Length() - 1);

		if (body.Length() != 8 && body.Length() != 6)
			return fallback;

		int value = 0;
		for (int i = 0; i < body.Length(); i++)
		{
			int digit = HexDigit(body.Get(i));
			if (digit < 0)
				return fallback;

			value = (value * 16) + digit;
		}

		// A 6-digit value is RGB - assume fully opaque.
		if (body.Length() == 6)
			value = value | 0xFF000000;

		return value;
	}

	static int HexDigit(string ch)
	{
		int idx = "0123456789abcdef".IndexOf(ch);
		if (idx >= 0)
			return idx;

		idx = "0123456789ABCDEF".IndexOf(ch);
		return idx;
	}
}

//------------------------------------------------------------------------------
// Grid reference helper - "094 037" style, matching the in-game map grid.
//------------------------------------------------------------------------------
class MMER_Grid
{
	// The in-game map numbers its northing DOWN from the top edge, not up from
	// world Z = 0, so a naive z/100 is wrong by (worldSize - z) on every map.
	// The engine already knows the answer: World.GetGridCoords() returns the
	// exact pair the map draws, and the square size comes out of the world
	// config the same way vanilla's map-navigation code reads it.
	// (4_World/Entities/ItemBase/MapNavigationBehaviour.c, DayZ 1.29.)
	static string Ref(float x, float z)
	{
		float gridSize = GridSize();

		int gx = 0;
		int gz = 0;

		World world = GetGame().GetWorld();
		if (world)
		{
			world.GetGridCoords(Vector(x, 0, z), gridSize, gx, gz);

			// Vanilla takes the absolute value: the engine hands back a
			// negative northing because it counts southward from the top edge.
			gx = Math.AbsInt(gx);
			gz = Math.AbsInt(gz);
		}

		// If the engine gave us nothing usable (no world yet, or a terrain with
		// no Grid block) fall back to the manual form - still measured from the
		// north edge, so it agrees with what the map draws.
		bool engineFailed = (gx == 0 && gz == 0);
		bool awayFromCorner = (x > gridSize || z > gridSize);

		if (engineFailed && awayFromCorner)
		{
			int worldSize = 0;
			if (world)
				worldSize = world.GetWorldSize();

			if (worldSize <= 0)
				worldSize = 15360;

			gx = Math.Floor(x / gridSize);
			gz = Math.Floor((worldSize - z) / gridSize);
		}

		return string.Format("%1 %2", Pad3(gx), Pad3(gz));
	}

	// Metres per map square, read from the world's own config. Chernarus, Deer
	// Isle and Sakhal all use 100 m at Zoom1, but a custom terrain may not.
	static float GridSize()
	{
		string path = string.Format("CfgWorlds %1 Grid Zoom1 stepX", GetGame().GetWorldName());
		float step = GetGame().ConfigGetFloat(path);

		if (step < 1.0)
			step = 100.0;

		return step;
	}

	static string Pad3(int v)
	{
		string t = v.ToString();
		while (t.Length() < 3)
			t = "0" + t;
		return t;
	}

	static string Bearing(vector from, vector to)
	{
		vector diff = to - from;
		float ang = Math.Atan2(diff[0], diff[2]) * Math.RAD2DEG;
		if (ang < 0)
			ang += 360;

		int deg = Math.Round(ang);
		string card = "N";
		if (deg >= 23  && deg < 68)  card = "NE";
		else if (deg >= 68  && deg < 113) card = "E";
		else if (deg >= 113 && deg < 158) card = "SE";
		else if (deg >= 158 && deg < 203) card = "S";
		else if (deg >= 203 && deg < 248) card = "SW";
		else if (deg >= 248 && deg < 293) card = "W";
		else if (deg >= 293 && deg < 338) card = "NW";

		return string.Format("%1 (%2)", card, deg);
	}
}

//------------------------------------------------------------------------------
// RPC chunking.
//
// A Param1<string> larger than roughly a kilobyte never arrives: RPCSingleParam
// returns normally on the server and OnRPC is simply never called on the client,
// with nothing logged at either end. That is what made the call list and the
// archive page look empty while the server log confirmed it had sent them -
// every small payload (settings, state, roster) got through, both 2 KB ones did
// not.
//
// Wire format for a split message, one chunk per RPC, same rpc id throughout:
//
//     ~<seq>:<total>:<body>
//
// A payload that fits in one RPC is sent exactly as before, with no header, so
// the common case costs nothing. JSON always starts with '{', never '~', so the
// two forms can never be confused.
//------------------------------------------------------------------------------
class MMER_Chunk
{
	static string Mark()
	{
		return "~";
	}

	static bool IsChunk(string s)
	{
		if (s.Length() < 1)
			return false;
		return s.Get(0) == MMER_Chunk.Mark();
	}

	// "~<msg>:<seq>:<total>:<body>". msg distinguishes one message from the
	// next on the same rpc id: without it, a message that lost its tail and a
	// following one with the SAME chunk count would splice together into a
	// single corrupt payload.
	static string Wrap(int msg, int seq, int total, string body)
	{
		return string.Format("~%1:%2:%3:%4", msg, seq, total, body);
	}

	// Splits the header off a chunk. Returns false on anything malformed.
	static bool Unwrap(string raw, out int msg, out int seq, out int total, out string body)
	{
		msg		= 0;
		seq		= 0;
		total	= 0;
		body	= "";

		if (!MMER_Chunk.IsChunk(raw))
			return false;

		string rest = raw.Substring(1, raw.Length() - 1);

		// Distinct locals at every step. Passing the same variable as both the
		// source and the out-parameter would alias them, and Enforce makes no
		// promise about the order those are written.
		string msgStr, afterMsg;
		if (!MMER_Chunk.Split(rest, msgStr, afterMsg))
			return false;

		string seqStr, afterSeq;
		if (!MMER_Chunk.Split(afterMsg, seqStr, afterSeq))
			return false;

		string totalStr, afterTotal;
		if (!MMER_Chunk.Split(afterSeq, totalStr, afterTotal))
			return false;

		msg		= msgStr.ToInt();
		seq		= seqStr.ToInt();
		total	= totalStr.ToInt();
		body	= afterTotal;

		if (total < 1 || total > MMER_Const.RPC_MAX_CHUNKS)
			return false;
		if (seq < 0 || seq >= total)
			return false;

		return true;
	}

	// Splits "head:tail" at the first colon. False when there is no colon or
	// the head is empty, which rejects every malformed header shape.
	static bool Split(string src, out string head, out string tail)
	{
		int at = src.IndexOf(":");
		if (at < 1)
		{
			head = "";
			tail = "";
			return false;
		}

		head = src.Substring(0, at);
		tail = src.Substring(at + 1, src.Length() - at - 1);
		return true;
	}
}

//------------------------------------------------------------------------------
// Client-side reassembly buffer. One slot per rpc id: a newer message for the
// same id always replaces whatever was part-way through, so a dropped tail can
// never poison the next send. Chunks are placed by index rather than appended,
// so arrival order does not matter.
//------------------------------------------------------------------------------
class MMER_ChunkBuffer
{
	int					rpcId;
	int					msgId;
	int					total;
	int					filled;
	ref array<string>	parts;

	void MMER_ChunkBuffer()
	{
		parts = new array<string>;
	}

	void Begin(int id, int message, int count)
	{
		rpcId	= id;
		msgId	= message;
		total	= count;
		filled	= 0;

		parts.Clear();
		for (int i = 0; i < count; i++)
			parts.Insert("");
	}

	// Returns true once every slot has been filled.
	bool Put(int seq, string body)
	{
		if (seq < 0 || seq >= parts.Count())
			return false;

		if (parts.Get(seq) == "")
			filled++;

		parts.Set(seq, body);
		return filled >= total;
	}

	string Join()
	{
		string whole = "";
		for (int i = 0; i < parts.Count(); i++)
			whole = whole + parts.Get(i);
		return whole;
	}
}
