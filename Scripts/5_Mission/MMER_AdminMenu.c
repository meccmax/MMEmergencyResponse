//------------------------------------------------------------------------------
// MM Emergency Response - roster management. Admin only, and the server checks
// that again on every request; this menu is a convenience, not a gate.
//------------------------------------------------------------------------------

class MMER_RosterRowHandler extends ScriptedWidgetEventHandler
{
	protected MMER_AdminMenu	m_Owner;
	protected string			m_Uid;
	protected ButtonWidget		m_Action;

	void Setup(MMER_AdminMenu owner, Widget root, string uid, ButtonWidget action)
	{
		m_Owner		= owner;
		m_Uid		= uid;
		m_Action	= action;
		root.SetHandler(this);
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		if (w == m_Action && m_Owner)
		{
			m_Owner.OnRowAction(m_Uid);
			return true;
		}
		return false;
	}
}

//------------------------------------------------------------------------------

class MMER_AdminMenu extends UIScriptedMenu
{
	static MMER_AdminMenu	s_Current;

	protected Widget			m_MemberList;
	protected Widget			m_OnlineList;
	protected EditBoxWidget		m_UidInput;
	protected ButtonWidget		m_BtnAdd;
	protected ButtonWidget		m_BtnClose;
	protected TextWidget		m_Hint;

	protected ref array<ref MMER_RosterRowHandler> m_Handlers;

	//--------------------------------------------------------------------------

	static MMER_AdminMenu Current()
	{
		return s_Current;
	}

	void MMER_AdminMenu()
	{
		m_Handlers = new array<ref MMER_RosterRowHandler>;
	}

	void ~MMER_AdminMenu()
	{
		if (s_Current == this)
			s_Current = null;
	}

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets(MMER_Const.LAYOUT_DIR + "mmer_admin.layout");
		if (!layoutRoot)
		{
			MMER_Log.Error("mmer_admin.layout failed to load.");
			return null;
		}

		m_MemberList	= layoutRoot.FindAnyWidget("mmer_member_list");
		m_OnlineList	= layoutRoot.FindAnyWidget("mmer_online_list");
		m_UidInput		= EditBoxWidget.Cast(layoutRoot.FindAnyWidget("mmer_uid_input"));
		m_BtnAdd		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_add"));
		m_BtnClose		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_admin_close"));
		m_Hint			= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_admin_hint"));

		// See the note in MMER_PanelMenu.Init - a UIScriptedMenu is not a
		// ScriptedWidgetEventHandler and does not need to be.
		return layoutRoot;
	}

	override void OnShow()
	{
		super.OnShow();
		s_Current = this;

		// Player control MUST be disabled while a menu is open, otherwise the
		// game keeps consuming mouse input and no click ever reaches a widget.
		// This is the pattern every working DayZ menu uses.
		GetGame().GetUIManager().ShowUICursor(true);
		GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);

		if (GetGame().GetMission().GetHud())
			GetGame().GetMission().GetHud().Show(false);

		MMER_ClientState.Get().RequestRoster();
		RefreshFromState();
	}

	override void OnHide()
	{
		super.OnHide();

		if (s_Current == this)
			s_Current = null;

		GetGame().GetUIManager().ShowUICursor(false);
		GetGame().GetMission().PlayerControlEnable(false);

		if (GetGame().GetMission().GetHud())
			GetGame().GetMission().GetHud().Show(true);
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		// Same reason as the dispatch panel: the push from the client state is
		// lost if this menu was not current when the roster reply landed.
		if (MMER_ClientState.Get().ConsumeRosterDirty())
			RefreshFromState();

		// ESC. The action is UAUIBack, read through the UAInput API - the
		// string form used previously ("UAUIMenu") is not the back action and
		// silently never fired, which is why the menu could not be closed.
		UAInput back = GetUApi().GetInputByID(UAUIBack);
		if (back && back.LocalPress())
			Close();
	}

	//--------------------------------------------------------------------------

	void RefreshFromState()
	{
		if (!layoutRoot)
			return;

		m_Handlers.Clear();

		MMER_RosterPayload roster = MMER_ClientState.Get().GetRoster();

		Clear(m_MemberList);
		Clear(m_OnlineList);

		// Admins are responders too - IsTeam() returns true for them, so they can
		// accept and complete calls. Listing only teamIds here made the roster
		// look empty on a server whose responders are all admins.
		if (roster.admins)
		{
			for (int a = 0; a < roster.admins.Count(); a++)
			{
				string auid = roster.admins.Get(a);
				BuildRow(m_MemberList, auid, NameFor(roster, auid) + " (admin)", "", false);
			}
		}

		if (roster.members)
		{
			for (int i = 0; i < roster.members.Count(); i++)
			{
				string uid = roster.members.Get(i);
				BuildRow(m_MemberList, uid, NameFor(roster, uid), "#STR_MMER_BTN_REMOVE", true);
			}
		}

		// Every online player, so an admin can see who is actually on the server
		// and what access they have. Previously admins and existing responders
		// were skipped entirely, which on this server meant the list was always
		// empty.
		if (roster.onlineUids)
		{
			for (int j = 0; j < roster.onlineUids.Count(); j++)
			{
				string ouid = roster.onlineUids.Get(j);

				string oname = "";
				if (roster.onlineNames && j < roster.onlineNames.Count())
					oname = roster.onlineNames.Get(j);

				bool isAdmin = (roster.admins && roster.admins.Find(ouid) > -1);
				bool isMember = (roster.members && roster.members.Find(ouid) > -1);

				if (isAdmin)
					BuildRow(m_OnlineList, ouid, oname + " (admin)", "", false);
				else if (isMember)
					BuildRow(m_OnlineList, ouid, oname + " (responder)", "#STR_MMER_BTN_REMOVE", true);
				else
					BuildRow(m_OnlineList, ouid, oname, "#STR_MMER_BTN_ADD", false);
			}
		}

		// WrapSpacers must be told to reflow after script adds children.
		if (m_MemberList)
			m_MemberList.Update();
		if (m_OnlineList)
			m_OnlineList.Update();

		if (m_Hint)
		{
			int adminCount = 0;
			if (roster.admins)
				adminCount = roster.admins.Count();

			// Admins count as responders - they can take calls. Say the total
			// rather than implying a server of four admins has nobody on duty.
			int memberCount = 0;
			if (roster.members)
				memberCount = roster.members.Count();

			m_Hint.SetText(string.Format(
				"%1 responder(s) total: %2 admin(s) + %3 added. Type a name or Steam64 ID to add.",
				adminCount + memberCount, adminCount, memberCount));
		}
	}

	protected string NameFor(MMER_RosterPayload roster, string uid)
	{
		if (!roster.onlineUids || !roster.onlineNames)
			return "(offline)";

		int idx = roster.onlineUids.Find(uid);
		if (idx > -1 && idx < roster.onlineNames.Count())
			return roster.onlineNames.Get(idx);

		return "(offline)";
	}

	protected void Clear(Widget holder)
	{
		MMER_WidgetUtil.ClearChildren(holder);
	}

	protected void BuildRow(Widget holder, string uid, string name, string actionLabel, bool isMember)
	{
		if (!holder)
			return;

		Widget row = GetGame().GetWorkspace().CreateWidgets(
			MMER_Const.LAYOUT_DIR + "mmer_roster_row.layout", holder);

		if (!row)
			return;

		TextWidget uidText	= TextWidget.Cast(row.FindAnyWidget("mmer_roster_uid"));
		TextWidget nameText	= TextWidget.Cast(row.FindAnyWidget("mmer_roster_name"));
		ButtonWidget action	= ButtonWidget.Cast(row.FindAnyWidget("mmer_roster_action"));

		if (uidText)	uidText.SetText(uid);
		if (nameText)	nameText.SetText(name);

		// An empty label means there is nothing to do to this row - admins are
		// set in config.json only. Hide the button rather than showing a dead one.
		if (action)
		{
			if (actionLabel == "")
			{
				action.Show(false);
			}
			else
			{
				action.Show(true);
				action.SetText(actionLabel);
			}
		}

		MMER_RosterRowHandler handler = new MMER_RosterRowHandler();
		handler.Setup(this, row, uid, action);
		m_Handlers.Insert(handler);
	}

	//--------------------------------------------------------------------------

	void OnRowAction(string uid)
	{
		MMER_RosterPayload roster = MMER_ClientState.Get().GetRoster();

		if (roster.members && roster.members.Find(uid) > -1)
			MMER_ClientState.Get().RemoveMember(uid);
		else
			MMER_ClientState.Get().AddMember(uid);
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		if (w == m_BtnClose)
		{
			Close();
			return true;
		}

		if (w == m_BtnAdd && m_UidInput)
		{
			string uid = m_UidInput.GetText();
			uid.Trim();

			if (uid != "")
			{
				MMER_ClientState.Get().AddMember(uid);
				m_UidInput.SetText("");
			}
			return true;
		}

		return super.OnClick(w, x, y, button);
	}
}
