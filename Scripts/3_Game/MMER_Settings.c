//------------------------------------------------------------------------------
// MM Emergency Response - server settings, persisted to
// $profile:MMEmergency/config.json and created with defaults on first boot.
//------------------------------------------------------------------------------

class MMER_DiagRow
{
	string	id		= "";		// "vanilla:<name>" or "terje:<name>" - see MMER_Diagnostics
	string	label	= "";		// what the medic sees
	string	unit	= "";		// appended to the value, e.g. "%"
	float	scale	= 1.0;		// value is multiplied by this before display
	int		decimals = 0;
	float	warnAbove = -1;		// highlight amber when value > this (-1 disables)
	float	warnBelow = -1;		// highlight amber when value < this (-1 disables)
}

class MMER_Settings
{
	// ---- general -----------------------------------------------------------
	int		configVersion			= 1;
	int		enabled					= 1;
	string	teamName				= "MEDEVAC";			// chat prefix / marker label
	string	panelTitle				= "MM Emergency Response";	// heading on the K panel
	string	serverLogo				= "";	// no logo ships; point at your own .edds/.paa

	// ---- access ------------------------------------------------------------
	ref TStringArray adminIds;		// Steam64 - full control
	ref TStringArray teamIds;		// Steam64 - responders

	// ---- patient side ------------------------------------------------------
	int		requireUnconscious		= 1;					// 0 lets any player call
	int		callCooldownSeconds		= 300;					// per player
	int		callExpireMinutes		= 25;					// unclaimed calls auto-expire
	float	buttonX					= 0.5;					// 0..1 screen relative
	float	buttonY					= 0.78;
	float	buttonScale				= 1.0;
	int		autoCloseOnRevive		= 1;					// completing when patient wakes

	// ---- responder side ----------------------------------------------------
	int		maxActiveCallsPerMedic	= 1;
	int		showPatientNames		= 1;
	int		refreshIntervalSeconds	= 5;

	// ---- markers -----------------------------------------------------------
	// 0 = grid/range/bearing only (no dependencies), 2 = DayZ Expansion marker
	int		markerMode				= 0;
	int		markerDurationSeconds	= 900;
	string	markerIcon				= "Medic";
	string	markerColor				= "0xFFE04B4B";

	// ---- respawn control ---------------------------------------------------
	int		blockRespawnDuringCall	= 1;
	int		blockRespawnOnlyWhenClaimed = 1;				// only once a medic accepted
	int		respawnBlockMaxSeconds	= 900;					// hard ceiling, never trap a player

	// ---- restart stabilisation --------------------------------------------
	ref TStringArray restartTimesUTC;						// "HH:MM" entries
	int		stabilizeMinutesBefore	= 5;
	int		stabilizeStopBleeding	= 1;
	int		stabilizeWakeUp			= 1;
	float	stabilizeBloodFloor		= 3500.0;
	int		stabilizeCloseCalls		= 1;					// close open calls as COMPLETED

	// ---- audio -------------------------------------------------------------
	string	soundIncoming			= "";	// set once you ship sound files
	string	soundFatal				= "";
	float	soundVolume				= 0.7;

	// ---- discord -----------------------------------------------------------
	string	discordWebhookUrl		= "";					// full https://discord.com/api/webhooks/... URL
	int		discordOnNew			= 1;
	int		discordOnAccept			= 1;
	int		discordOnComplete		= 1;
	int		discordOnDeath			= 1;
	// Cancelled by the patient, expired, or auto-closed because the patient
	// came round on their own. Off means Discord only hears about cases a
	// responder actually touched.
	int		discordOnCancel			= 1;

	// Call gating by item. Off by default - turning it on changes who can call
	// at all, which is not something an upgrade should do silently.
	//
	// The beacon is spent on a call that actually opens, and given back if the
	// call expires with nobody responding or the patient cancels almost
	// immediately: burning a scarce item and getting no rescue is the most
	// frustrating outcome the system can produce, and a misclick should never
	// cost anything.
	int		requireCallItem			= 0;
	ref TStringArray callItemTypes;			// accepted class names, first match is spent
	string	callItemLabel			= "distress beacon";	// what the refusal message calls it
	int		consumeCallItem			= 1;
	int		refundOnExpire			= 1;
	int		refundCancelSeconds		= 30;	// 0 disables the misclick refund

	// Symmetric variant: responders must also carry a radio to accept a case.
	// Theirs is never consumed - they pay in kit, the patient pays in stock.
	//
	// A frequency requirement is reasonable HERE and would not be on the
	// patient's side: a responder is conscious, can retune on the spot, and is
	// someone you can simply tell the frequency to. Every refusal says exactly
	// what is wrong, including which frequency they are actually on.
	int		requireItemForResponder	= 0;
	ref TStringArray responderItemTypes;			// e.g. {"PersonalRadio"}
	string	responderItemLabel		= "radio";
	float	responderFrequency		= 0;	// 0 = any frequency accepted
	int		responderRadioMustBeOn	= 1;	// powered and switched on, not just carried

	// Chat tags. Purely cosmetic - the tag is drawn by the client next to the
	// sender's name and confers nothing. Chat carries a name, not a Steam64, so
	// a player who renames themselves to match a responder gets the tag too;
	// that is a display quirk, not an access path.
	int		chatTagEnabled			= 1;
	string	chatTagText				= "[MEDEVAC]";
	string	chatTagColor			= "0xFF4BE07A";
	string	chatTagAdminText		= "[MEDEVAC CMD]";
	string	chatTagAdminColor		= "0xFFE0A94B";
	string	discordUsername			= "MEDEVAC Dispatch";
	int		discordTestOnStart		= 1;	// post once on boot to prove it works

	// ---- archive -----------------------------------------------------------
	int		archiveEnabled			= 1;
	int		archiveMaxEntries		= 500;
	int		archivePageSize			= 25;

	// ---- diagnostics -------------------------------------------------------
	// terjeEnabled only matters if the mod was built with MMER_TERJE defined,
	// which the Steam Workshop build is. Set it to 0 to hide every terje: row
	// without editing the diagnostics array.
	int		terjeEnabled			= 1;
	ref array<ref MMER_DiagRow> diagnostics;

	//--------------------------------------------------------------------------

	// Ref members are built here rather than inline - Enforce is inconsistent
	// about member initialisers that allocate.
	void MMER_Settings()
	{
		adminIds		= new TStringArray;
		teamIds			= new TStringArray;
		restartTimesUTC	= new TStringArray;
		diagnostics		= new array<ref MMER_DiagRow>;
		callItemTypes	= new TStringArray;
		responderItemTypes = new TStringArray;
	}

	void ApplyDefaults()
	{
		if (!callItemTypes)
			callItemTypes = new TStringArray;

		// Seeded even when the feature is off, so an operator turning
		// requireCallItem on has a working example rather than an empty list
		// that silently refuses every call.
		if (callItemTypes.Count() == 0)
			callItemTypes.Insert("Roadflare");

		if (!responderItemTypes)
			responderItemTypes = new TStringArray;

		if (responderItemTypes.Count() == 0)
			responderItemTypes.Insert("PersonalRadio");

		if (restartTimesUTC.Count() == 0)
		{
			restartTimesUTC.Insert("01:00");
			restartTimesUTC.Insert("07:00");
			restartTimesUTC.Insert("13:00");
			restartTimesUTC.Insert("19:00");
		}

		if (diagnostics.Count() == 0)
		{
			// Vanilla rows always resolve. Terje rows resolve only when the
			// Terje build flag is on AND the stat id exists - unknown ids are
			// simply skipped, so this list is safe to edit freely.
			AddDiag("vanilla:blood",		"Blood",			"",		1.0,	0,	-1,		4000);
			AddDiag("vanilla:health",		"Health",			"",		1.0,	0,	-1,		50);
			AddDiag("vanilla:shock",		"Shock",			"",		1.0,	0,	-1,		40);
			AddDiag("vanilla:bleeding",		"Bleed sources",	"",		1.0,	0,	0,		-1);
			AddDiag("vanilla:energy",		"Energy",			"",		1.0,	0,	-1,		600);
			AddDiag("vanilla:water",		"Water",			"",		1.0,	0,	-1,		600);
			AddDiag("vanilla:temperature",	"Body temp",		"C",	1.0,	1,	38.5,	35.0);

			// Terje severity readings are 0-3 steps and the wound readings are
			// counts, so both show best with no decimal places. A warnAbove of
			// 0.5 means "anything above zero is worth flagging" without
			// tripping on float noise. Radiation is the exception - it is a
			// continuous value from TerjeRadiation, not a Medicine record.
			AddDiag("terje:sepsis",			"Sepsis",			"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:pain",			"Pain",				"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:influenza",		"Influenza",		"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:zvirus",			"Z-Virus",			"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:contusion",		"Contusion",		"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:hematoma",		"Hematoma",			"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:bulletHit",		"Retained round",	"",		1.0,	0,	0.5,	-1);
			AddDiag("terje:radiation",		"Radiation",		"",		1.0,	2,	0.10,	-1);

			// Treatments already on board. These are not problems, so they are
			// never flagged - a responder reads them to decide what NOT to
			// give a second dose of.
			AddDiag("terje:painkiller",		"Painkillers",		"",		1.0,	0,	-1,		-1);
			AddDiag("terje:antibiotics",	"Antibiotics",		"",		1.0,	0,	-1,		-1);
			AddDiag("terje:hemostatic",		"Hemostatic",		"",		1.0,	0,	-1,		-1);
		}
	}

	void AddDiag(string id, string label, string unit, float scale, int decimals, float warnAbove, float warnBelow)
	{
		MMER_DiagRow row = new MMER_DiagRow;
		row.id			= id;
		row.label		= label;
		row.unit		= unit;
		row.scale		= scale;
		row.decimals	= decimals;
		row.warnAbove	= warnAbove;
		row.warnBelow	= warnBelow;
		diagnostics.Insert(row);
	}

	bool IsAdmin(string uid)
	{
		return adminIds && adminIds.Find(uid) > -1;
	}

	bool IsTeam(string uid)
	{
		if (IsAdmin(uid))
			return true;
		return teamIds && teamIds.Find(uid) > -1;
	}

	int RoleOf(string uid)
	{
		if (IsAdmin(uid))
			return MMER_Role.ADMIN;
		if (IsTeam(uid))
			return MMER_Role.MEMBER;
		return MMER_Role.NONE;
	}
}

//------------------------------------------------------------------------------
// The trimmed-down settings blob the server ships to each client. Never send
// the admin/team lists or the webhook URL to clients.
//------------------------------------------------------------------------------
class MMER_ClientSettings
{
	string	teamName		= "MEDEVAC";
	string	panelTitle		= "MM Emergency Response";
	string	serverLogo		= "";
	float	buttonX			= 0.5;
	float	buttonY			= 0.78;
	float	buttonScale		= 1.0;
	int		showPatientNames = 1;
	string	soundIncoming	= "";
	string	soundFatal		= "";
	float	soundVolume		= 0.7;
	int		blockRespawn	= 1;

	int		chatTagEnabled		= 1;
	string	chatTagText			= "[MEDEVAC]";
	string	chatTagColor		= "0xFF4BE07A";
	string	chatTagAdminText	= "[MEDEVAC CMD]";
	string	chatTagAdminColor	= "0xFFE0A94B";

	void FromSettings(MMER_Settings s)
	{
		teamName			= s.teamName;
		panelTitle			= s.panelTitle;
		serverLogo			= s.serverLogo;
		buttonX				= s.buttonX;
		buttonY				= s.buttonY;
		buttonScale			= s.buttonScale;
		showPatientNames	= s.showPatientNames;
		soundIncoming		= s.soundIncoming;
		soundFatal			= s.soundFatal;
		soundVolume			= s.soundVolume;
		blockRespawn		= s.blockRespawnDuringCall;
		chatTagEnabled		= s.chatTagEnabled;
		chatTagText			= s.chatTagText;
		chatTagColor		= s.chatTagColor;
		chatTagAdminText	= s.chatTagAdminText;
		chatTagAdminColor	= s.chatTagAdminColor;
	}
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_ClientSettings>.MakeData(this, json, err, false))
			return "";
		return json;
	}

	static MMER_ClientSettings FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_ClientSettings obj = new MMER_ClientSettings;
		string err;
		if (!JsonFileLoader<MMER_ClientSettings>.LoadData(data, obj, err))
			return null;
		return obj;
	}

}

//------------------------------------------------------------------------------
// Loader. Server-authoritative: only ever called from MMER_Manager on the
// server. Writes the file back out after loading so new fields added in a
// future version appear in the operator's config with their defaults.
//------------------------------------------------------------------------------
class MMER_SettingsLoader
{
	static ref MMER_Settings s_Settings;

	static MMER_Settings Get()
	{
		if (!s_Settings)
			Load();
		return s_Settings;
	}

	static void Load()
	{
		if (!FileExist(MMER_Const.PROFILE_DIR))
			MakeDirectory(MMER_Const.PROFILE_DIR);

		s_Settings = new MMER_Settings;

		if (FileExist(MMER_Const.CONFIG_PATH))
		{
			// LoadFile, not the deprecated JsonLoadFile. JsonLoadFile returns
			// void and NEVER nulls its out-parameter on a parse error - it just
			// calls ErrorEx and leaves the object as it found it. The old
			// "if (!s_Settings)" guard here was therefore dead code, the early
			// return unreachable, and Save() below ran regardless: one
			// malformed byte in config.json silently overwrote the operator's
			// adminIds, teamIds and live Discord webhook URL with defaults,
			// while logging that it had not. Verified against
			// 3_Game/tools/JsonFileLoader.c in the 1.29 source.
			string err;
			if (!JsonFileLoader<MMER_Settings>.LoadFile(MMER_Const.CONFIG_PATH, s_Settings, err))
			{
				MMER_Log.Error("config.json failed to parse: " + err);
				MMER_Log.Error("Running on defaults for this session. The bad file has NOT been touched - fix it and restart.");

				s_Settings = new MMER_Settings;
				s_Settings.ApplyDefaults();

				// Deliberately no Save(): overwriting a file we could not read
				// destroys the admin list and the webhook credential.
				return;
			}

			if (!s_Settings.adminIds)			s_Settings.adminIds = new TStringArray;
			if (!s_Settings.teamIds)			s_Settings.teamIds = new TStringArray;
			if (!s_Settings.restartTimesUTC)	s_Settings.restartTimesUTC = new TStringArray;
			if (!s_Settings.diagnostics)		s_Settings.diagnostics = new array<ref MMER_DiagRow>;
			if (!s_Settings.callItemTypes)		s_Settings.callItemTypes = new TStringArray;
			if (!s_Settings.responderItemTypes)	s_Settings.responderItemTypes = new TStringArray;

			MMER_Log.Info(string.Format("config.json loaded (%1 admins, %2 responders, %3 diagnostic rows)",
				s_Settings.adminIds.Count(), s_Settings.teamIds.Count(), s_Settings.diagnostics.Count()));
		}
		else
		{
			s_Settings.ApplyDefaults();
			MMER_Log.Info("No config.json found - writing defaults.");
		}

		s_Settings.ApplyDefaults();
		Save();
	}

	static void Save()
	{
		if (!s_Settings)
			return;

		if (!FileExist(MMER_Const.PROFILE_DIR))
			MakeDirectory(MMER_Const.PROFILE_DIR);

		JsonFileLoader<MMER_Settings>.JsonSaveFile(MMER_Const.CONFIG_PATH, s_Settings);
	}
}
