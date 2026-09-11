//------------------------------------------------------------------------------
// MM Emergency Response - client-side mirror of the server state.
//
// Purely a cache. Nothing here decides anything: every button in the UI sends
// a request and waits for the server to say what happened. If the client is
// tampered with, the worst it can do is show its owner a lie.
//------------------------------------------------------------------------------

class MMER_ClientState extends MMER_ClientSink
{
	static ref MMER_ClientState	s_Instance;

	protected ref MMER_ClientSettings	m_Settings;
	protected ref MMER_StatePayload		m_State;
	protected ref array<ref MMER_Call>	m_Calls;
	protected ref MMER_RosterPayload		m_Roster;
	protected ref MMER_ArchivePayload	m_Archive;
	protected ref MMER_TagPayload		m_Tags;

	protected int	m_LastToastTime;
	protected int	m_CooldownStamp;		// GetTime() ms when cooldownLeft arrived
	protected bool	m_CooldownRefreshed;
	protected bool	m_Dirty;
	protected bool	m_RosterDirty;
	protected bool	m_ArchiveDirty;

	//--------------------------------------------------------------------------

	static MMER_ClientState Get()
	{
		if (!s_Instance)
		{
			s_Instance = new MMER_ClientState();
			MMER_ClientSink.Register(s_Instance);
		}
		return s_Instance;
	}

	void MMER_ClientState()
	{
		m_Settings	= new MMER_ClientSettings;
		m_State		= new MMER_StatePayload;
		m_Calls		= new array<ref MMER_Call>;
		m_Roster	= new MMER_RosterPayload;
		m_Archive	= new MMER_ArchivePayload;
		m_Tags		= new MMER_TagPayload;
	}

	MMER_ClientSettings GetSettings()	{ return m_Settings; }
	MMER_StatePayload GetState()		{ return m_State; }
	array<ref MMER_Call> GetCalls()		{ return m_Calls; }
	MMER_RosterPayload GetRoster()		{ return m_Roster; }
	MMER_ArchivePayload GetArchive()	{ return m_Archive; }

	bool IsResponder()	{ return m_State.role >= MMER_Role.MEMBER; }

	// Seconds left on the call cooldown, counted down locally from the last
	// server push. Returns 0 once it expires.
	int CooldownRemaining()
	{
		if (m_State.cooldownLeft <= 0)
			return 0;

		int elapsed = (GetGame().GetTime() - m_CooldownStamp) / 1000;
		int left = m_State.cooldownLeft - elapsed;
		if (left < 0)
			left = 0;

		// One refresh when it hits zero: the server owns canCall, so ask it
		// rather than flipping the button on our own authority.
		if (left == 0 && !m_CooldownRefreshed)
		{
			m_CooldownRefreshed = true;
			RequestHello();
		}

		return left;
	}
	bool IsAdmin()		{ return m_State.role == MMER_Role.ADMIN; }

	bool ConsumeDirty()			{ bool d = m_Dirty;        m_Dirty = false;        return d; }
	bool ConsumeRosterDirty()	{ bool d = m_RosterDirty;  m_RosterDirty = false;  return d; }
	bool ConsumeArchiveDirty()	{ bool d = m_ArchiveDirty; m_ArchiveDirty = false; return d; }

	//--------------------------------------------------------------------------
	// Incoming
	//--------------------------------------------------------------------------

	override void OnSettings(string json)
	{
		MMER_ClientSettings s = MMER_ClientSettings.FromJson(json);
		if (!s)
			return;

		m_Settings = s;
		m_Dirty = true;
	}

	override void OnState(string json)
	{
		MMER_StatePayload s = MMER_StatePayload.FromJson(json);
		if (!s)
			return;

		m_State = s;
		m_CooldownStamp = GetGame().GetTime();
		m_CooldownRefreshed = false;
		m_Dirty = true;
		MMER_CallButton.Get().Refresh();
	}

	override void OnCallList(string json)
	{
		MMER_CallListPayload p = MMER_CallListPayload.FromJson(json);
		if (!p)
			return;

		m_Calls = p.calls;
		if (!m_Calls)
			m_Calls = new array<ref MMER_Call>;

		m_Dirty = true;

		MMER_PanelMenu panel = MMER_PanelMenu.Current();
		if (panel)
			panel.RefreshFromState();
	}

	override void OnRoster(string json)
	{
		MMER_RosterPayload p = MMER_RosterPayload.FromJson(json);
		if (p)
		{
			m_Roster = p;
			m_RosterDirty = true;

			MMER_AdminMenu admin = MMER_AdminMenu.Current();
			if (admin)
				admin.RefreshFromState();
		}
	}

	override void OnArchive(string json)
	{
		MMER_ArchivePayload p = MMER_ArchivePayload.FromJson(json);
		if (p)
		{
			m_Archive = p;
			m_ArchiveDirty = true;

			MMER_PanelMenu panel = MMER_PanelMenu.Current();
			if (panel)
				panel.RefreshFromState();
		}
	}

	override void OnTags(string json)
	{
		MMER_TagPayload p = MMER_TagPayload.FromJson(json);
		if (p)
			m_Tags = p;
	}

	// Which tag, if any, a chat sender should carry. Chat gives us a display
	// name and nothing else, so this is a name match by necessity - it decides
	// what colour to draw, never what anyone is allowed to do.
	int TagRoleFor(string senderName)
	{
		if (senderName == "" || !m_Tags)
			return MMER_Role.NONE;

		if (m_Tags.admins && m_Tags.admins.Find(senderName) > -1)
			return MMER_Role.ADMIN;

		if (m_Tags.responders && m_Tags.responders.Find(senderName) > -1)
			return MMER_Role.MEMBER;

		return MMER_Role.NONE;
	}

	override void OnToast(string json)
	{
		MMER_ToastPayload t = MMER_ToastPayload.FromJson(json);
		if (!t)
			return;

		string text = t.title;
		if (t.body != "")
			text = t.title + ": " + t.body;

		// Vanilla system chat is the one notification channel that is always
		// present and never fights the HUD for space.
		if (GetGame() && GetGame().GetMission())
		{
			GetGame().GetMission().OnEvent(
				ChatMessageEventTypeID,
				new ChatMessageEventParams(CCSystem, m_Settings.teamName, text, ""));
		}

		PlayCue(t.kind);
	}

	//--------------------------------------------------------------------------

	void PlayCue(int kind)
	{
		string soundSet = "";
		if (kind == MMER_Toast.INCOMING)
			soundSet = m_Settings.soundIncoming;
		else if (kind == MMER_Toast.FATAL)
			soundSet = m_Settings.soundFatal;

		if (soundSet == "")
			return;

		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (!player)
			return;

		// SEffectManager returns null for an unknown sound set rather than
		// throwing, so a missing custom sound simply means silence.
		EffectSound sound = SEffectManager.PlaySound(soundSet, player.GetPosition());
		if (sound)
		{
			sound.SetSoundVolume(m_Settings.soundVolume);
			sound.SetAutodestroy(true);
		}
	}

	//--------------------------------------------------------------------------
	// Outgoing
	//--------------------------------------------------------------------------

	protected PlayerBase Me()
	{
		return PlayerBase.Cast(GetGame().GetPlayer());
	}

	void RequestHello()
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendSimple(MMER_RPC.CLIENT_HELLO);
	}

	void RequestCall()
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendSimple(MMER_RPC.CLIENT_CALL_REQUEST);
	}

	void CancelCall()
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendSimple(MMER_RPC.CLIENT_CALL_CANCEL);
	}

	void AcceptCall(int id)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendInt(MMER_RPC.CLIENT_ACCEPT, id);
	}

	void CompleteCall(int id)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendInt(MMER_RPC.CLIENT_COMPLETE, id);
	}

	void AbandonCall(int id)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendInt(MMER_RPC.CLIENT_ABANDON, id);
	}

	void RemarkCall(int id)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendInt(MMER_RPC.CLIENT_MARK, id);
	}

	void RequestRoster()
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendSimple(MMER_RPC.CLIENT_ADMIN_LIST);
	}

	void AddMember(string uid)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendString(MMER_RPC.CLIENT_ADMIN_ADD, uid);
	}

	void RemoveMember(string uid)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendString(MMER_RPC.CLIENT_ADMIN_REMOVE, uid);
	}

	void RequestArchive(int page)
	{
		PlayerBase p = Me();
		if (p) p.MMER_SendInt(MMER_RPC.CLIENT_REQ_ARCHIVE, page);
	}

	//--------------------------------------------------------------------------

	MMER_Call FindCall(int id)
	{
		if (!m_Calls)
			return null;

		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (c && c.id == id)
				return c;
		}
		return null;
	}

	int OpenCount()
	{
		int n = 0;
		if (!m_Calls)
			return 0;

		for (int i = 0; i < m_Calls.Count(); i++)
		{
			MMER_Call c = m_Calls.Get(i);
			if (c && c.IsOpen())
				n++;
		}
		return n;
	}
}
