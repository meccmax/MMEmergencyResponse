//------------------------------------------------------------------------------
// MM Emergency Response - server-authoritative dispatch.
//
// Owns every piece of mutable state: the active call list, the id counter, the
// per-player cooldowns, the archive and the restart stabiliser. Clients hold a
// read-only projection of this and can only ask for transitions, never assert
// them. Every handler re-derives the caller's identity from the RPC sender, so
// a modified client cannot claim to be someone else.
//------------------------------------------------------------------------------

class MMER_Manager
{
	static ref MMER_Manager	s_Instance;

	protected ref MMER_Settings				m_Settings;
	protected ref array<ref MMER_Call>		m_Calls;			// open + recently closed
	protected ref MMER_ArchiveFile			m_Archive;
	protected bool							m_ArchiveReloaded;
	protected bool							m_ArchiveBroken;	// archive.json would not parse - never write over it
	protected int							m_LastOverflowWarn;	// throttles the call-list overflow warning
	protected int							m_ChunkMsgId;		// monotonic id stamped on every split RPC
	protected ref map<string, int>			m_Cooldowns;		// uid -> unix time the cooldown ends
	protected ref MMER_ClientSettings		m_ClientSettings;

	protected int	m_NextId;
	protected int	m_LastTick;
	protected int	m_LastSync;
	protected bool	m_StabilizedForRestart;
	protected int	m_LastRestartWindow;

	//--------------------------------------------------------------------------

	static MMER_Manager Get()
	{
		if (!s_Instance)
			s_Instance = new MMER_Manager();
		return s_Instance;
	}

	void MMER_Manager()
	{
		m_Calls			= new array<ref MMER_Call>;
		m_Cooldowns		= new map<string, int>;
		m_Archive		= new MMER_ArchiveFile;
		m_NextId		= 1;
		m_LastRestartWindow = -1;
	}

	void Init()
	{
		MMER_SettingsLoader.Load();
		m_Settings = MMER_SettingsLoader.Get();

		m_ClientSettings = new MMER_ClientSettings;
		m_ClientSettings.FromSettings(m_Settings);

		LoadActive();
		LoadArchive();

		MMER_Webhook.Init(m_Settings.discordWebhookUrl, m_Settings.discordTestOnStart == 1, m_Settings.discordUsername);

		MMER_Log.Info(string.Format("Dispatch online. %1 call(s) restored, next id %2, marker mode %3.",
			m_Calls.Count(), m_NextId, m_Settings.markerMode));
	}

	MMER_Settings GetSettings()
	{
		if (!m_Settings)
			m_Settings = MMER_SettingsLoader.Get();
		return m_Settings;
	}

	//==========================================================================
	// Tick - driven from MMER_MissionServer.OnUpdate, throttled to 1 Hz.
	//==========================================================================

	void OnUpdate()
	{
		int now = MMER_Time.NowUnix();
		if (now == m_LastTick)
			return;
		m_LastTick = now;

		if (!GetSettings() || m_Settings.enabled == 0)
			return;

		ExpireStaleCalls(now);
		ExpireStaleMarkers(now);
		PruneCooldowns(now);
		AutoCloseRevivedPatients(now);
		CheckRestartWindow(now);

		int interval = m_Settings.refreshIntervalSeconds;
		if (interval < 1)
			interval = 5;

		if (now - m_LastSync >= interval)
		{
			m_LastSync = now;
			BroadcastCallList();
		}
	}

	//==========================================================================
	// Player lifecycle
	//==========================================================================

	void OnPlayerConnected(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;

		SendSettings(player);
		SendState(player);

		if (IsResponder(player.GetIdentity().GetPlainId()))
			SendCallList(player);

		// Everyone gets the tag list, not just responders - a plain survivor is
		// exactly who the tag is there to inform.
		BroadcastTags();
	}

	void OnPlayerDisconnected(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;

		string uid = player.GetIdentity().GetPlainId();

		// A responder who logs out mid-intervention releases the call back to
		// the queue rather than leaving the patient claimed and unattended.
		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call call = m_Calls.Get(i);
			if (call && call.state == MMER_CallState.IN_PROGRESS && call.medicUid == uid)
			{
				call.state		= MMER_CallState.NEW;
				call.medicUid	= "";
				call.medicName	= "";
				call.acceptedAt	= 0;
				call.note		= "Responder disconnected - returned to queue";
				MMER_Log.Info(string.Format("Call #%1 released: responder %2 disconnected.", call.id, uid));
			}
		}

		SaveActive();
		BroadcastCallList();
		BroadcastTags();
	}

	void OnPlayerUnconscious(PlayerBase player)
	{
		// Nothing automatic - the patient chooses to call. This hook exists so
		// the client can be told the button is now available.
		if (player && player.GetIdentity())
			SendState(player);
	}

	void OnPlayerConscious(PlayerBase player)
	{
		if (player && player.GetIdentity())
			SendState(player);
	}

	void OnPlayerDeath(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;

		string uid = player.GetIdentity().GetPlainId();
		MMER_Call call = FindOpenCallForPatient(uid);
		if (!call)
			return;

		CloseCall(call, MMER_CallState.DECEASED, "Patient died", true);

		NotifyResponders(MMER_Toast.FATAL, "Patient lost",
			string.Format("Call #%1 - %2 did not survive.", call.id, call.patientName), call.id);

		if (m_Settings.discordOnDeath)
		{
			MMER_Webhook.Post(
				string.Format("Patient lost - call #%1", call.id),
				string.Format("**Patient:** %1\n**Grid:** %2\n**Responder:** %3\n**Elapsed:** %4",
					MMER_Webhook.SafeName(call.patientName), call.GridRef(), MMER_Webhook.SafeName(ResponderLabel(call)), MMER_Time.Duration(call.AgeSeconds())),
				0x8A8A8A, m_Settings.discordUsername);
		}
	}

	//==========================================================================
	// Transitions
	//==========================================================================

	void RequestCall(PlayerBase player, PlayerIdentity sender)
	{
		if (!player || !sender)
			return;

		string uid	= sender.GetPlainId();
		string name	= sender.GetName();
		int now		= MMER_Time.NowUnix();

		if (m_Settings.enabled == 0)
		{
			SendToast(player, MMER_Toast.ERROR, "Unavailable", "Emergency dispatch is offline.", 0);
			return;
		}

		if (m_Settings.requireUnconscious == 1 && !player.IsUnconscious())
		{
			SendToast(player, MMER_Toast.ERROR, "Rejected", "You can only call while unconscious.", 0);
			return;
		}

		if (!player.IsAlive())
			return;

		if (FindOpenCallForPatient(uid))
		{
			SendToast(player, MMER_Toast.ERROR, "Already dispatched", "Your call is already in the queue.", 0);
			return;
		}

		int cooldownEnd;
		if (m_Cooldowns.Find(uid, cooldownEnd) && cooldownEnd > now)
		{
			SendToast(player, MMER_Toast.ERROR, "Cooldown",
				string.Format("You can call again in %1.", MMER_Time.Duration(cooldownEnd - now)), 0);
			return;
		}

		MMER_Call call		= new MMER_Call;
		call.id				= m_NextId++;
		call.state			= MMER_CallState.NEW;
		call.patientUid		= uid;
		call.patientName	= name;
		call.createdAt		= now;
		call.SetPosition(player.GetPosition());

		if (player.IsUnconscious())
			call.wasUnconscious = 1;

		RefreshDiagnostics(call, player);

		m_Calls.Insert(call);
		m_Cooldowns.Set(uid, now + m_Settings.callCooldownSeconds);

		MMER_MarkerAdapter.Place(call, m_Settings);

		// SafeName on the way into the log too: a newline inside a player name
		// would otherwise let them forge whole log lines, including fake
		// "added responder" audit entries.
		MMER_Log.Info(string.Format("Call #%1 opened by %2 (%3) at grid %4.",
			call.id, MMER_Webhook.SafeName(name), uid, call.GridRef()));

		SaveActive();
		SendState(player);
		BroadcastCallList();

		NotifyResponders(MMER_Toast.INCOMING, "Emergency call",
			string.Format("#%1 - %2 down at grid %3.", call.id, PatientLabel(call), call.GridRef()), call.id);

		MMER_Log.Info(string.Format("Call #%1 pushed to %2 responder(s) online.",
			call.id, CountOnlineResponders()));

		if (m_Settings.discordOnNew)
		{
			MMER_Webhook.Post(
				string.Format("New emergency call #%1", call.id),
				string.Format("**Patient:** %1\n**Grid:** %2\n**Responders online:** %3",
					MMER_Webhook.SafeName(call.patientName), call.GridRef(), CountOnlineResponders()),
				0xE04B4B, m_Settings.discordUsername);
		}
	}

	void CancelCall(PlayerBase player, PlayerIdentity sender)
	{
		if (!sender)
			return;

		MMER_Call call = FindOpenCallForPatient(sender.GetPlainId());
		if (!call)
			return;

		CloseCall(call, MMER_CallState.CANCELLED, "Cancelled by patient");
		SendState(player);
		BroadcastCallList();

		NotifyResponders(MMER_Toast.PLAIN, "Call cancelled",
			string.Format("#%1 was cancelled by the patient.", call.id), call.id);
	}

	void AcceptCall(PlayerBase medic, PlayerIdentity sender, int callId)
	{
		if (!medic || !sender)
			return;

		string uid = sender.GetPlainId();
		if (!IsResponder(uid))
			return;

		MMER_Call call = FindCall(callId);
		if (!call || call.state != MMER_CallState.NEW)
		{
			SendToast(medic, MMER_Toast.ERROR, "Unavailable", "That call is no longer open.", 0);
			return;
		}

		if (CountActiveFor(uid) >= m_Settings.maxActiveCallsPerMedic)
		{
			SendToast(medic, MMER_Toast.ERROR, "Limit reached",
				"Complete your current intervention first.", 0);
			return;
		}

		call.state		= MMER_CallState.IN_PROGRESS;
		call.medicUid	= uid;
		call.medicName	= sender.GetName();
		call.acceptedAt	= MMER_Time.NowUnix();

		RefreshDiagnosticsFromWorld(call);
		MMER_MarkerAdapter.Place(call, m_Settings);

		MMER_Log.Info(string.Format("Call #%1 accepted by %2 (%3).", call.id, call.medicName, uid));

		SaveActive();
		BroadcastCallList();

		PlayerBase patient = FindPlayerByUid(call.patientUid);
		if (patient)
		{
			SendState(patient);
			SendToast(patient, MMER_Toast.PLAIN, "Help is coming",
				string.Format("%1 is en route.", call.medicName), call.id);
		}

		if (m_Settings.discordOnAccept)
		{
			MMER_Webhook.Post(
				string.Format("Call #%1 accepted", call.id),
				string.Format("**Responder:** %1\n**Patient:** %2\n**Grid:** %3\n**Waited:** %4",
					MMER_Webhook.SafeName(call.medicName), MMER_Webhook.SafeName(call.patientName), call.GridRef(),
					MMER_Time.Duration(call.acceptedAt - call.createdAt)),
				0xE0A94B, m_Settings.discordUsername);
		}
	}

	void AbandonCall(PlayerBase medic, PlayerIdentity sender, int callId)
	{
		if (!sender)
			return;

		MMER_Call call = FindCall(callId);
		if (!call || call.state != MMER_CallState.IN_PROGRESS)
			return;

		string uid = sender.GetPlainId();
		if (call.medicUid != uid && !m_Settings.IsAdmin(uid))
			return;

		call.state		= MMER_CallState.NEW;
		call.medicUid	= "";
		call.medicName	= "";
		call.acceptedAt	= 0;
		call.note		= "Returned to queue";

		MMER_Log.Info(string.Format("Call #%1 released by %2.", call.id, uid));

		SaveActive();
		BroadcastCallList();
	}

	void CompleteCall(PlayerBase medic, PlayerIdentity sender, int callId)
	{
		if (!sender)
			return;

		MMER_Call call = FindCall(callId);
		if (!call || !call.IsOpen())
			return;

		string uid = sender.GetPlainId();
		if (call.medicUid != uid && !m_Settings.IsAdmin(uid))
			return;

		CloseCall(call, MMER_CallState.COMPLETED, "Intervention completed", true);

		PlayerBase patient = FindPlayerByUid(call.patientUid);
		if (patient)
		{
			SendState(patient);
			SendToast(patient, MMER_Toast.PLAIN, "Treated",
				string.Format("Intervention #%1 closed.", call.id), call.id);
		}

		BroadcastCallList();

		if (m_Settings.discordOnComplete)
		{
			MMER_Webhook.Post(
				string.Format("Call #%1 completed", call.id),
				string.Format("**Responder:** %1\n**Patient:** %2\n**On scene:** %3\n**Total:** %4",
					MMER_Webhook.SafeName(ResponderLabel(call)), MMER_Webhook.SafeName(call.patientName),
					MMER_Time.Duration(call.closedAt - call.acceptedAt),
					MMER_Time.Duration(call.AgeSeconds())),
				0x4BE07A, m_Settings.discordUsername);
		}
	}

	void RemarkCall(PlayerBase medic, PlayerIdentity sender, int callId)
	{
		MMER_Call call = FindCall(callId);
		if (!call || !call.IsOpen() || !sender)
			return;

		if (!IsResponder(sender.GetPlainId()))
			return;

		PlayerBase patient = FindPlayerByUid(call.patientUid);
		if (patient)
		{
			call.SetPosition(patient.GetPosition());
			RefreshDiagnostics(call, patient);
		}

		MMER_MarkerAdapter.Place(call, m_Settings);
		BroadcastCallList();
	}

	//==========================================================================
	// Roster administration
	//==========================================================================

	void AdminAddMember(PlayerBase admin, PlayerIdentity sender, string target)
	{
		if (!sender || !m_Settings.IsAdmin(sender.GetPlainId()))
			return;

		// The box accepts either a Steam64 ID or the in-game name of someone
		// who is online. Typing a name used to fail SanitiseUid and return
		// here in silence, which looked like the button was dead.
		string targetUid = SanitiseUid(target);
		if (targetUid == "")
			targetUid = ResolveOnlineName(target);

		if (targetUid == "")
		{
			SendToast(admin, MMER_Toast.ERROR, "No match",
				string.Format("Nobody online is called %1, and that is not a Steam64 ID.", target), 0);
			return;
		}

		if (m_Settings.IsAdmin(targetUid))
		{
			SendToast(admin, MMER_Toast.PLAIN, "Already a responder",
				"Admins always have responder access. Nothing to add.", 0);
			return;
		}

		if (m_Settings.teamIds.Find(targetUid) > -1)
		{
			SendToast(admin, MMER_Toast.PLAIN, "Already on the roster",
				"That player is already a responder.", 0);
			return;
		}

		m_Settings.teamIds.Insert(targetUid);
		MMER_SettingsLoader.Save();
		MMER_Log.Info(string.Format("%1 added responder %2.", sender.GetPlainId(), targetUid));

		SendRoster(admin);
		BroadcastTags();

		PlayerBase added = FindPlayerByUid(targetUid);
		if (added)
		{
			SendState(added);
			SendCallList(added);
			SendToast(added, MMER_Toast.PLAIN, m_Settings.teamName, "You are now on the response roster.", 0);
		}
	}

	void AdminRemoveMember(PlayerBase admin, PlayerIdentity sender, string targetUid)
	{
		if (!sender || !m_Settings.IsAdmin(sender.GetPlainId()))
			return;

		targetUid = SanitiseUid(targetUid);
		int idx = m_Settings.teamIds.Find(targetUid);
		if (idx < 0)
			return;

		m_Settings.teamIds.Remove(idx);
		MMER_SettingsLoader.Save();
		MMER_Log.Info(string.Format("%1 removed responder %2.", sender.GetPlainId(), targetUid));

		// Release anything they were holding. Authority over a claimed call is
		// checked against call.medicUid, which does not consult the roster, so
		// without this a removed responder kept Complete/Abandon on their open
		// cases - and kept those patients' respawn locked - indefinitely.
		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call held = m_Calls.Get(i);
			if (held && held.state == MMER_CallState.IN_PROGRESS && held.medicUid == targetUid)
			{
				held.state		= MMER_CallState.NEW;
				held.medicUid	= "";
				held.medicName	= "";
				held.acceptedAt	= 0;
				held.note		= "Responder removed from roster - returned to queue";

				PlayerBase heldPatient = FindPlayerByUid(held.patientUid);
				if (heldPatient)
					SendState(heldPatient);
			}
		}

		SaveActive();
		BroadcastCallList();

		SendRoster(admin);
		BroadcastTags();

		PlayerBase removed = FindPlayerByUid(targetUid);
		if (removed)
			SendState(removed);
	}

	void SendRoster(PlayerBase admin)
	{
		if (!admin || !admin.GetIdentity())
			return;

		if (!m_Settings.IsAdmin(admin.GetIdentity().GetPlainId()))
			return;

		MMER_RosterPayload payload = new MMER_RosterPayload;
		// Looped for the same reason as SaveActive - no Copy() on collections
		// anywhere in this mod, so there is one rule instead of two.
		for (int a = 0; a < m_Settings.adminIds.Count(); a++)
			payload.admins.Insert(m_Settings.adminIds.Get(a));

		for (int t = 0; t < m_Settings.teamIds.Count(); t++)
			payload.members.Insert(m_Settings.teamIds.Get(t));

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p || !p.GetIdentity())
				continue;

			payload.onlineUids.Insert(p.GetIdentity().GetPlainId());
			payload.onlineNames.Insert(p.GetIdentity().GetName());
		}

		Send(admin, MMER_RPC.SERVER_ROSTER, payload.ToJson());
	}

	//==========================================================================
	// Archive
	//==========================================================================

	void SendArchivePage(PlayerBase medic, PlayerIdentity sender, int page)
	{
		if (!sender)
			return;

		// Refuse silently. Logging here wrote to disk on an UNAUTHENTICATED
		// request - any client could loop this RPC and drive a full
		// open/append/close cycle per call, filling the volume and starving
		// every JSON persist. Every other refusal in this file is silent for
		// the same reason; Debug is suppressed at the default log level.
		if (!IsResponder(sender.GetPlainId()))
		{
			MMER_Log.Debug(string.Format("Archive refused for %1 - not on the roster.", sender.GetPlainId()));
			return;
		}

		if (m_Settings.archiveEnabled == 0)
		{
			MMER_Log.Warn("Archive requested but archiveEnabled is 0.");
			return;
		}

		if (!m_Archive || !m_Archive.calls)
		{
			MMER_Log.Error("Archive requested but the archive object is missing.");
			return;
		}

		// Self-heal: an in-memory archive that is empty while archive.json has
		// records on disk means the boot-time load did not take. Re-read once
		// rather than showing the responder an empty list forever.
		if (!m_ArchiveReloaded && m_Archive.calls.Count() == 0 && FileExist(MMER_Const.ARCHIVE_PATH))
		{
			m_ArchiveReloaded = true;
			MMER_Log.Warn("Archive is empty in memory but archive.json exists - reloading it.");
			LoadArchive();
		}

		int size = m_Settings.archivePageSize;
		if (size < 1)
			size = 25;

		// Hard ceiling regardless of config. One archived record serialises to
		// roughly 340 bytes and an over-long RPC string is dropped by the
		// engine without an error, which would look exactly like an empty
		// archive on the client.
		if (size > MMER_Const.MAX_ARCHIVE_PAGE)
			size = MMER_Const.MAX_ARCHIVE_PAGE;

		int total = m_Archive.calls.Count();
		int pages = (total + size - 1) / size;
		if (page < 0)
			page = 0;
		if (page >= pages)
			page = Math.Max(0, pages - 1);

		MMER_ArchivePayload payload = new MMER_ArchivePayload;
		payload.page		= page;
		payload.pageCount	= pages;
		payload.total		= total;

		// Newest first.
		int start = total - 1 - (page * size);
		int count = 0;
		for (int i = start; i >= 0 && count < size; i--)
		{
			// Scrubbed per viewer: an archive page used to hand a responder the
			// Steam64 of every patient in it, hundreds of rows at a time.
			payload.calls.Insert(m_Archive.calls.Get(i).ForViewer(
				sender.GetPlainId(), m_Settings.showPatientNames == 1, false));
			count++;
		}

		string json = payload.ToJson();

		MMER_Log.Info(string.Format("Archive page %1/%2 sent to %3 (%4 of %5 record(s), %6 bytes).",
			page + 1, Math.Max(1, pages), sender.GetPlainId(), count, total, json.Length()));

		Send(medic, MMER_RPC.SERVER_ARCHIVE, json);
	}

	//==========================================================================
	// Restart stabilisation
	//==========================================================================

	protected void CheckRestartWindow(int now)
	{
		if (m_Settings.stabilizeMinutesBefore <= 0)
			return;

		int minutesLeft = MMER_Time.MinutesUntilNext(m_Settings.restartTimesUTC);
		if (minutesLeft < 0)
			return;

		// One-shot per window: reset the latch once we are clear of it again.
		if (minutesLeft > m_Settings.stabilizeMinutesBefore)
		{
			m_StabilizedForRestart = false;
			return;
		}

		if (m_StabilizedForRestart)
			return;

		m_StabilizedForRestart = true;
		StabilizeEveryone(minutesLeft);
	}

	protected void StabilizeEveryone(int minutesLeft)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		int treated = 0;
		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p || !p.IsAlive())
				continue;

			// Truthiness test rather than "!= null" - Enforce does not compare
			// class references against null with the equality operators.
			bool hasOpenCall = false;
			if (p.GetIdentity() && FindOpenCallForPatient(p.GetIdentity().GetPlainId()))
				hasOpenCall = true;

			if (!p.IsUnconscious() && !hasOpenCall)
				continue;

			Stabilize(p);
			treated++;

			SendToast(p, MMER_Toast.PLAIN, "Stabilised",
				string.Format("Server restart in ~%1 min. You were stabilised automatically.", minutesLeft), 0);
		}

		if (m_Settings.stabilizeCloseCalls)
		{
			for (int c = m_Calls.Count() - 1; c >= 0; c--)
			{
				MMER_Call call = m_Calls.Get(c);
				if (call && call.IsOpen())
					// selfReported: the single summary embed below covers the
					// whole sweep. One post per closed call would flood the
					// channel on a busy restart.
					CloseCall(call, MMER_CallState.COMPLETED, "Auto-stabilised before restart", true);
			}
			BroadcastCallList();
		}

		if (treated > 0)
		{
			MMER_Log.Info(string.Format("Pre-restart stabilisation: %1 player(s) treated, ~%2 min to restart.",
				treated, minutesLeft));

			MMER_Webhook.Post("Pre-restart stabilisation",
				string.Format("%1 player(s) stabilised ahead of the restart in ~%2 minutes.", treated, minutesLeft),
				0x4B9BE0, m_Settings.discordUsername);
		}
	}

	protected void Stabilize(PlayerBase player)
	{
		if (!player)
			return;

		if (m_Settings.stabilizeStopBleeding)
		{
			// RemoveAllSources() is inherited from BleedingSourcesManagerBase;
			// there is no RemoveAllBleedingSources on the server subclass.
			BleedingSourcesManagerServer bleeding = player.GetBleedingManagerServer();
			if (bleeding)
				bleeding.RemoveAllSources();
		}

		if (player.GetHealth("", "Blood") < m_Settings.stabilizeBloodFloor)
			player.SetHealth("", "Blood", m_Settings.stabilizeBloodFloor);

		if (m_Settings.stabilizeWakeUp)
		{
			// Shock is the unconsciousness driver - restoring it to full is what
			// actually brings the player round.
			player.SetHealth("", "Shock", player.GetMaxHealth("", "Shock"));
		}

		MMER_TerjeAdapter.Stabilize(player);
	}

	//==========================================================================
	// Housekeeping
	//==========================================================================

	// One entry per uid that has ever called, held for the life of the server
	// process. Bounded by distinct players rather than by anything an attacker
	// controls, but unbounded in principle - so drop the dead ones.
	protected void PruneCooldowns(int now)
	{
		array<string> stale = new array<string>;

		for (int i = 0; i < m_Cooldowns.Count(); i++)
		{
			if (m_Cooldowns.GetElement(i) < now)
				stale.Insert(m_Cooldowns.GetKey(i));
		}

		for (int j = 0; j < stale.Count(); j++)
			m_Cooldowns.Remove(stale.Get(j));
	}

	protected void ExpireStaleCalls(int now)
	{
		if (m_Settings.callExpireMinutes <= 0)
			return;

		int limit = m_Settings.callExpireMinutes * 60;
		bool dirty = false;

		for (int i = m_Calls.Count() - 1; i >= 0; i--)
		{
			MMER_Call call = m_Calls.Get(i);
			if (!call)
			{
				m_Calls.Remove(i);
				continue;
			}

			if (call.state == MMER_CallState.NEW && (now - call.createdAt) > limit)
			{
				CloseCall(call, MMER_CallState.EXPIRED, "No responder available");
				dirty = true;
				continue;
			}

			// A claimed case used to live for the whole server uptime if the
			// responder simply never came back to it: it held the patient's
			// respawn locked, kept their position streaming to every responder,
			// and sat in the sync payload forever. Give it its own, longer
			// ceiling rather than no ceiling at all.
			if (call.state == MMER_CallState.IN_PROGRESS && call.acceptedAt > 0)
			{
				if ((now - call.acceptedAt) > (limit * 2))
				{
					CloseCall(call, MMER_CallState.EXPIRED, "Responder did not close the case");
					dirty = true;
					continue;
				}
			}

			// Closed calls linger briefly so the panel can show the outcome.
			if (MMER_CallState.IsClosed(call.state) && call.closedAt > 0 && (now - call.closedAt) > 120)
			{
				m_Calls.Remove(i);
				dirty = true;
			}
		}

		if (dirty)
		{
			SaveActive();
			BroadcastCallList();
		}
	}

	// A marker that has been on the map for markerDurationSeconds comes down
	// even if the case is still open - a stale pin is worse than none, because
	// the patient has almost certainly been dragged or has crawled off.
	protected void ExpireStaleMarkers(int now)
	{
		if (m_Settings.markerDurationSeconds <= 0)
			return;

		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call call = m_Calls.Get(i);
			if (!call || !call.IsOpen())
				continue;

			if ((now - call.createdAt) > m_Settings.markerDurationSeconds)
				MMER_MarkerAdapter.Remove(call, m_Settings);
		}
	}

	protected void AutoCloseRevivedPatients(int now)
	{
		if (m_Settings.autoCloseOnRevive == 0)
			return;

		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call call = m_Calls.Get(i);
			if (!call || !call.IsOpen())
				continue;

			PlayerBase patient = FindPlayerByUid(call.patientUid);
			if (!patient)
				continue;

			// Refresh the position and vitals of every open case so the panel
			// stays live without the responder having to poke it.
			call.SetPosition(patient.GetPosition());
			RefreshDiagnostics(call, patient);

			if (!patient.IsAlive())
				continue;

			if (patient.IsUnconscious())
				continue;

			// A call made by a conscious player (requireUnconscious 0) must not
			// be closed the instant it opens just because the caller is upright.
			// Only someone who was actually down can "recover".
			if (call.wasUnconscious == 0)
				continue;

			// Only auto-close a case a responder actually took; an unclaimed
			// call from someone who woke up on their own just gets cancelled.
			if (call.state == MMER_CallState.IN_PROGRESS)
				CloseCall(call, MMER_CallState.COMPLETED, "Patient regained consciousness");
			else
				CloseCall(call, MMER_CallState.CANCELLED, "Patient recovered before dispatch");

			SendState(patient);
		}
	}

	// selfReported: the caller posts its own, richer Discord embed (a medic
	// pressing Complete, or a death). Everything else - cancelled by the
	// patient, expired, auto-closed because the patient came round on their
	// own, stabilised before a restart - closed silently before this, which is
	// why Discord never heard about a cancel or a self-heal.
	protected void CloseCall(MMER_Call call, int state, string reason, bool selfReported = false)
	{
		if (!call || !call.IsOpen())
			return;

		call.state		= state;
		call.closedAt	= MMER_Time.NowUnix();
		call.closeReason = reason;

		MMER_MarkerAdapter.Remove(call, m_Settings);

		MMER_Log.Info(string.Format("Call #%1 closed as %2 (%3).",
			call.id, MMER_CallState.ToPlain(state), reason));

		ArchiveCall(call);
		SaveActive();

		// The patient's client caches respawnBlocked from the last SendState.
		// Without this push, a patient who DIED while a responder was en route
		// kept a stale "blocked" flag and found all three respawn buttons
		// disabled with nothing to refresh them short of a relog. Same for a
		// call that expired. Every other close path already pushed; these two
		// did not, which is the one way this mod could actually trap someone.
		PlayerBase patient = FindPlayerByUid(call.patientUid);
		if (patient)
			SendState(patient);

		if (!selfReported)
			PostClosureWebhook(call, state, reason);
	}

	protected void PostClosureWebhook(MMER_Call call, int state, string reason)
	{
		// Cancelled and expired ride the cancel flag; an auto-close that ended
		// as COMPLETED is still a completion as far as the roster is concerned.
		bool wantedFlag = false;

		if (state == MMER_CallState.CANCELLED || state == MMER_CallState.EXPIRED)
			wantedFlag = (m_Settings.discordOnCancel != 0);
		else if (state == MMER_CallState.COMPLETED)
			wantedFlag = (m_Settings.discordOnComplete != 0);
		else if (state == MMER_CallState.DECEASED)
			wantedFlag = (m_Settings.discordOnDeath != 0);

		if (!wantedFlag)
			return;

		int colour = 0x6E7B8B;
		if (state == MMER_CallState.COMPLETED)
			colour = 0x4BE07A;
		else if (state == MMER_CallState.DECEASED)
			colour = 0x8A8A8A;

		// Not ToLower(): in Enforce that mutates the string in place and
		// returns an int, so it would print the length instead of the word.
		string word = MMER_CallState.ToPlain(state);
		string title = string.Format("Call #%1 closed - %2", call.id, word);
		string body = string.Format("**Patient:** %1\n**Grid:** %2\n**Reason:** %3\n**Open for:** %4",
			MMER_Webhook.SafeName(call.patientName), call.GridRef(), reason, MMER_Time.Duration(call.AgeSeconds()));

		if (call.medicName != "")
			body = body + string.Format("\n**Responder:** %1", MMER_Webhook.SafeName(call.medicName));

		MMER_Webhook.Post(title, body, colour, m_Settings.discordUsername);
	}

	protected void ArchiveCall(MMER_Call call)
	{
		if (m_Settings.archiveEnabled == 0 || !m_Archive)
			return;

		m_Archive.calls.Insert(call.Slim());

		// 0 is not "unlimited": archive.json is fully rewritten on every
		// closure, so an unbounded file means an ever-growing write on every
		// single call. Fall back to the documented default instead.
		int max = m_Settings.archiveMaxEntries;
		if (max <= 0)
			max = 500;

		while (m_Archive.calls.Count() > max)
			m_Archive.calls.Remove(0);

		m_Archive.nextId = m_NextId;
		SaveArchive();
	}

	protected void RefreshDiagnostics(MMER_Call call, PlayerBase player)
	{
		if (!call || !player)
			return;

		array<ref MMER_DiagValue> rows = call.diagnostics;
		MMER_Diagnostics.Build(player, m_Settings, rows);
		call.diagnostics = rows;
	}

	protected void RefreshDiagnosticsFromWorld(MMER_Call call)
	{
		PlayerBase p = FindPlayerByUid(call.patientUid);
		if (p)
			RefreshDiagnostics(call, p);
	}

	//==========================================================================
	// Queries
	//==========================================================================

	MMER_Call FindCall(int id)
	{
		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (c && c.id == id)
				return c;
		}
		return null;
	}

	MMER_Call FindOpenCallForPatient(string uid)
	{
		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (c && c.patientUid == uid && c.IsOpen())
				return c;
		}
		return null;
	}

	int CountActiveFor(string medicUid)
	{
		int n = 0;
		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (c && c.state == MMER_CallState.IN_PROGRESS && c.medicUid == medicUid)
				n++;
		}
		return n;
	}

	int CountOnlineResponders()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		int n = 0;
		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.GetIdentity() && IsResponder(p.GetIdentity().GetPlainId()))
				n++;
		}
		return n;
	}

	bool IsResponder(string uid)
	{
		return GetSettings().IsTeam(uid);
	}

	bool IsRespawnBlockedFor(string uid)
	{
		if (m_Settings.blockRespawnDuringCall == 0)
			return false;

		MMER_Call call = FindOpenCallForPatient(uid);
		if (!call)
			return false;

		if (m_Settings.blockRespawnOnlyWhenClaimed == 1 && call.state != MMER_CallState.IN_PROGRESS)
			return false;

		// Never trap a player indefinitely. 0 used to disable the ceiling
		// altogether, which is the opposite of what an operator setting a
		// safety limit to zero is asking for.
		int ceiling = m_Settings.respawnBlockMaxSeconds;
		if (ceiling <= 0)
			ceiling = 900;

		if (call.AgeSeconds() > ceiling)
			return false;

		return true;
	}

	static PlayerBase FindPlayerByUid(string uid)
	{
		if (uid == "")
			return null;

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.GetIdentity() && p.GetIdentity().GetPlainId() == uid)
				return p;
		}
		return null;
	}

	protected string PatientLabel(MMER_Call call)
	{
		if (m_Settings.showPatientNames == 1)
			return call.patientName;
		return "Survivor";
	}

	protected string ResponderLabel(MMER_Call call)
	{
		if (call.medicName == "")
			return "unassigned";
		return call.medicName;
	}

	// Steam64 ids are 17 digits. Reject anything else outright rather than
	// writing operator typos or injected junk into config.json.
	// Matches typed text against the names of players who are online. Exact
	// match first (case-insensitive), then a unique prefix - an ambiguous
	// prefix resolves to nothing rather than to the wrong person.
	protected string ResolveOnlineName(string typed)
	{
		typed.Trim();
		if (typed == "")
			return "";

		string wanted = Lower(typed);

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		string prefixHit = "";
		int prefixCount = 0;

		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p || !p.GetIdentity())
				continue;

			string name = Lower(p.GetIdentity().GetName());
			if (name == wanted)
				return p.GetIdentity().GetPlainId();

			if (name.IndexOf(wanted) == 0)
			{
				prefixHit = p.GetIdentity().GetPlainId();
				prefixCount++;
			}
		}

		if (prefixCount == 1)
			return prefixHit;

		return "";
	}

	// ToLower() mutates in place and returns an int, so it cannot be used
	// inline. Assignment copies the string, so the caller's value is untouched.
	protected string Lower(string s)
	{
		string t = s;
		t.ToLower();
		return t;
	}

	protected string SanitiseUid(string uid)
	{
		uid.Trim();

		// Steam64 ids are exactly 17 digits and begin with 7. The old 8-24
		// range let an admin typo write a permanently dead entry into
		// config.json that would never match a real player.
		if (uid.Length() != 17)
			return "";

		if (uid.Get(0) != "7")
			return "";

		for (int i = 0; i < uid.Length(); i++)
		{
			if ("0123456789".IndexOf(uid.Get(i)) < 0)
				return "";
		}
		return uid;
	}

	//==========================================================================
	// Networking
	//==========================================================================

	protected void Send(PlayerBase player, int rpcId, string json)
	{
		if (!player || !player.GetIdentity())
			return;

		int len = json.Length();

		// Short payloads go as they always did - one RPC, no header.
		if (len <= MMER_Const.RPC_CHUNK)
		{
			GetGame().RPCSingleParam(player, rpcId, new Param1<string>(json), true, player.GetIdentity());
			return;
		}

		int total = (len + MMER_Const.RPC_CHUNK - 1) / MMER_Const.RPC_CHUNK;
		int msg = m_ChunkMsgId++;

		if (total > MMER_Const.RPC_MAX_CHUNKS)
		{
			MMER_Log.Error(string.Format(
				"RPC %1 is %2 bytes - too large to send even split up. Reduce archivePageSize.", rpcId, len));
			return;
		}

		for (int i = 0; i < total; i++)
		{
			int start = i * MMER_Const.RPC_CHUNK;
			int take = MMER_Const.RPC_CHUNK;
			if (start + take > len)
				take = len - start;

			string body = json.Substring(start, take);
			string framed = MMER_Chunk.Wrap(msg, i, total, body);

			GetGame().RPCSingleParam(player, rpcId, new Param1<string>(framed), true, player.GetIdentity());
		}
	}

	void SendSettings(PlayerBase player)
	{
		if (!m_ClientSettings)
		{
			m_ClientSettings = new MMER_ClientSettings;
			m_ClientSettings.FromSettings(GetSettings());
		}
		Send(player, MMER_RPC.SERVER_SETTINGS, m_ClientSettings.ToJson());
	}

	void SendState(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;

		string uid	= player.GetIdentity().GetPlainId();
		int now		= MMER_Time.NowUnix();

		MMER_StatePayload state = new MMER_StatePayload;
		state.myUid = uid;
		state.role = GetSettings().RoleOf(uid);

		int cooldownEnd;
		if (m_Cooldowns.Find(uid, cooldownEnd) && cooldownEnd > now)
			state.cooldownLeft = cooldownEnd - now;

		MMER_Call mine = FindOpenCallForPatient(uid);
		if (mine)
		{
			state.myCallId		= mine.id;
			state.myCallState	= mine.state;
			state.myMedicName	= mine.medicName;
		}

		bool eligible = player.IsAlive();
		if (m_Settings.requireUnconscious == 1 && !player.IsUnconscious())
			eligible = false;
		if (mine || state.cooldownLeft > 0 || m_Settings.enabled == 0)
			eligible = false;

		if (eligible)
			state.canCall = 1;

		if (IsRespawnBlockedFor(uid))
			state.respawnBlocked = 1;

		Send(player, MMER_RPC.SERVER_STATE, state.ToJson());
	}

	void SendCallList(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;

		if (!IsResponder(player.GetIdentity().GetPlainId()))
			return;

		MMER_CallListPayload payload = new MMER_CallListPayload;
		payload.serverTime = MMER_Time.NowUnix();

		int now = payload.serverTime;
		int added = 0;

		string viewer = player.GetIdentity().GetPlainId();
		bool showNames = (m_Settings.showPatientNames == 1);

		for (int i = 0; i < m_Calls.Count() && added < MMER_Const.MAX_SYNC_CALLS; i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (!c)
				continue;

			if (c.IsOpen())
			{
				// Open cases carry their full diagnostics - that is what the
				// responder is looking at while deciding whether to go.
				payload.calls.Insert(c.ForViewer(viewer, showNames, true));
				payload.openCount++;
				added++;
				continue;
			}

			// The panel hides anything closed more than 90 seconds ago, so
			// there is no point paying to send it, and the closed ones that do
			// go are slimmed - nobody reads vitals on a finished case.
			if (c.closedAt > 0 && (now - c.closedAt) > 90)
				continue;

			payload.calls.Insert(c.ForViewer(viewer, showNames, false));
			added++;
		}

		string listJson = payload.ToJson();

		// Past roughly two dozen simultaneous open cases the serialised list
		// exceeds what the chunker will send, and Send() would drop the whole
		// thing - a blank dispatch panel for every responder, exactly when the
		// server is busiest. Shed diagnostics first, then oldest cases, so the
		// queue degrades instead of disappearing.
		int ceiling = MMER_Const.RPC_MAX_CHUNKS * MMER_Const.RPC_CHUNK;

		if (listJson.Length() > ceiling)
		{
			MMER_CallListPayload lean = new MMER_CallListPayload;
			lean.serverTime	= payload.serverTime;
			lean.openCount	= payload.openCount;

			for (int j = 0; j < payload.calls.Count(); j++)
				lean.calls.Insert(payload.calls.Get(j).Slim());

			listJson = lean.ToJson();

			while (listJson.Length() > ceiling && lean.calls.Count() > 1)
			{
				lean.calls.Remove(0);
				listJson = lean.ToJson();
			}

			// Once a minute at most. This fires per responder per sync
			// interval, and an unthrottled warning here is its own disk-write
			// loop on a server busy enough to trip it.
			if (now - m_LastOverflowWarn >= 60)
			{
				m_LastOverflowWarn = now;
				MMER_Log.Warn(string.Format(
					"Call list too large for one sync - sent %1 of %2 case(s) without diagnostics.",
					lean.calls.Count(), payload.calls.Count()));
			}
		}

		Send(player, MMER_RPC.SERVER_CALLLIST, listJson);
	}

	// Names of online responders and admins, pushed to EVERY player so the chat
	// tag can be drawn. Names only: this reaches the whole server, and a Steam64
	// is not something to hand to everyone just to colour a chat line.
	void BroadcastTags()
	{
		if (m_Settings.chatTagEnabled == 0)
			return;

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		MMER_TagPayload payload = new MMER_TagPayload;
		int i;

		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p || !p.GetIdentity())
				continue;

			string uid = p.GetIdentity().GetPlainId();
			string name = p.GetIdentity().GetName();

			if (m_Settings.IsAdmin(uid))
				payload.admins.Insert(name);
			else if (IsResponder(uid))
				payload.responders.Insert(name);
		}

		string json = payload.ToJson();

		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase target = PlayerBase.Cast(players.Get(i));
			if (target && target.GetIdentity())
				Send(target, MMER_RPC.SERVER_TAGS, json);
		}
	}

	void BroadcastCallList()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.GetIdentity() && IsResponder(p.GetIdentity().GetPlainId()))
				SendCallList(p);
		}
	}

	void SendToast(PlayerBase player, int kind, string title, string body, int callId)
	{
		MMER_ToastPayload toast = new MMER_ToastPayload;
		toast.kind		= kind;
		toast.title		= title;
		toast.body		= body;
		toast.callId	= callId;

		Send(player, MMER_RPC.SERVER_TOAST, toast.ToJson());
	}

	void NotifyResponders(int kind, string title, string body, int callId)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.GetIdentity() && IsResponder(p.GetIdentity().GetPlainId()))
				SendToast(p, kind, title, body, callId);
		}
	}

	//==========================================================================
	// Persistence
	//==========================================================================

	protected void SaveActive()
	{
		if (!FileExist(MMER_Const.PROFILE_DIR))
			MakeDirectory(MMER_Const.PROFILE_DIR);

		MMER_CallListPayload snapshot = new MMER_CallListPayload;
		snapshot.serverTime = m_NextId;			// reused as the id watermark

		// Explicit loop rather than Copy(): in Enforce, array<ref T> and
		// array<T> are unrelated types, so Copy() will not take a ref array.
		for (int i = 0; i < m_Calls.Count(); i++)
			snapshot.calls.Insert(m_Calls.Get(i));

		JsonFileLoader<MMER_CallListPayload>.JsonSaveFile(MMER_Const.ACTIVE_PATH, snapshot);
	}

	protected void LoadActive()
	{
		if (!FileExist(MMER_Const.ACTIVE_PATH))
			return;

		MMER_CallListPayload snapshot = new MMER_CallListPayload;
		JsonFileLoader<MMER_CallListPayload>.JsonLoadFile(MMER_Const.ACTIVE_PATH, snapshot);

		if (!snapshot || !snapshot.calls)
			return;

		// Anything still open when the server went down is closed on boot:
		// the patient is not where the marker says any more.
		for (int i = 0; i < snapshot.calls.Count(); i++)
		{
			MMER_Call c = snapshot.calls.Get(i);
			if (!c)
				continue;

			if (c.IsOpen())
			{
				c.state			= MMER_CallState.EXPIRED;
				c.closedAt		= MMER_Time.NowUnix();
				c.closeReason	= "Server restarted";
			}

			if (c.id >= m_NextId)
				m_NextId = c.id + 1;
		}

		if (snapshot.serverTime >= m_NextId)
			m_NextId = snapshot.serverTime;
	}

	protected void SaveArchive()
	{
		// Never overwrite a file we failed to read: that turns one bad byte
		// into the loss of the whole intervention history.
		if (m_ArchiveBroken)
			return;

		if (!FileExist(MMER_Const.PROFILE_DIR))
			MakeDirectory(MMER_Const.PROFILE_DIR);

		JsonFileLoader<MMER_ArchiveFile>.JsonSaveFile(MMER_Const.ARCHIVE_PATH, m_Archive);
	}

	protected void LoadArchive()
	{
		if (!FileExist(MMER_Const.ARCHIVE_PATH))
		{
			MMER_Log.Info("No archive.json yet - starting an empty archive.");
			return;
		}

		// Load into a local, never straight into the member, and use LoadFile
		// rather than the deprecated JsonLoadFile - the latter returns void and
		// leaves its out-parameter untouched on a parse error, so a failure was
		// indistinguishable from success and the next SaveArchive() would have
		// written the half-parsed result back over the operator's history.
		MMER_ArchiveFile loaded = new MMER_ArchiveFile;
		string err;

		if (!JsonFileLoader<MMER_ArchiveFile>.LoadFile(MMER_Const.ARCHIVE_PATH, loaded, err))
		{
			MMER_Log.Error("archive.json failed to parse: " + err);
			MMER_Log.Error("Keeping the archive empty in memory. The file on disk has NOT been touched.");

			// Block the self-heal reload and, more importantly, stop
			// ArchiveCall() from persisting over a file we could not read.
			m_ArchiveReloaded = true;
			m_ArchiveBroken = true;
			return;
		}

		if (!loaded.calls)
			loaded.calls = new array<ref MMER_Call>;

		m_Archive = loaded;

		if (m_Archive.nextId >= m_NextId)
			m_NextId = m_Archive.nextId;

		MMER_Log.Info(string.Format("archive.json loaded (%1 record(s), next id %2).",
			m_Archive.calls.Count(), m_Archive.nextId));
	}
}
