//------------------------------------------------------------------------------
// MM Emergency Response - the responder dispatch panel (default key: K).
//
// Left: the call queue. Right: the selected patient's detail and diagnostics.
// Bottom: the actions available for the current selection and the current
// responder's role. Every action is a request; the list only changes when the
// server pushes a new one back.
//------------------------------------------------------------------------------

class MMER_RowHandler extends ScriptedWidgetEventHandler
{
	protected MMER_PanelMenu	m_Owner;
	protected int				m_CallId;
	protected Widget			m_Root;

	void Setup(MMER_PanelMenu owner, Widget root, int callId)
	{
		m_Owner		= owner;
		m_Root		= root;
		m_CallId	= callId;
		root.SetHandler(this);
	}

	int GetCallId()
	{
		return m_CallId;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		if (m_Owner)
		{
			m_Owner.SelectCall(m_CallId);
			return true;
		}
		return false;
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Root)
			m_Root.SetAlpha(0.9);
		return true;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Root)
			m_Root.SetAlpha(1.0);
		return true;
	}
}

//------------------------------------------------------------------------------

class MMER_PanelMenu extends UIScriptedMenu
{
	static MMER_PanelMenu	s_Current;

	protected TextWidget	m_Title;
	protected TextWidget	m_Subtitle;
	protected Widget		m_ListHolder;
	protected Widget		m_EmptyHint;

	protected TextWidget	m_DetailName;
	protected TextWidget	m_DetailState;
	protected TextWidget	m_DetailGrid;
	protected TextWidget	m_DetailRange;
	protected TextWidget	m_DetailElapsed;
	protected TextWidget	m_DetailMedic;
	protected TextWidget	m_DetailNote;
	protected Widget		m_DiagHolder;

	protected ButtonWidget	m_BtnAccept;
	protected ButtonWidget	m_BtnComplete;
	protected ButtonWidget	m_BtnAbandon;
	protected ButtonWidget	m_BtnMark;
	protected ButtonWidget	m_BtnArchive;
	protected ButtonWidget	m_BtnAdmin;
	protected ButtonWidget	m_BtnClose;

	protected ref array<ref MMER_RowHandler> m_RowHandlers;

	protected int	m_SelectedId;
	protected bool	m_ArchiveMode;
	protected int	m_ArchivePage;
	protected float	m_Accum;

	//--------------------------------------------------------------------------

	static MMER_PanelMenu Current()
	{
		return s_Current;
	}

	void MMER_PanelMenu()
	{
		m_RowHandlers = new array<ref MMER_RowHandler>;
	}

	void ~MMER_PanelMenu()
	{
		if (s_Current == this)
			s_Current = null;
	}

	//--------------------------------------------------------------------------

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets(MMER_Const.LAYOUT_DIR + "mmer_panel.layout");
		if (!layoutRoot)
		{
			MMER_Log.Error("mmer_panel.layout failed to load.");
			return null;
		}

		m_Title			= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_title"));
		m_Subtitle		= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_subtitle"));
		m_ListHolder	= layoutRoot.FindAnyWidget("mmer_list");
		m_EmptyHint		= layoutRoot.FindAnyWidget("mmer_empty");

		m_DetailName	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_name"));
		m_DetailState	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_state"));
		m_DetailGrid	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_grid"));
		m_DetailRange	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_range"));
		m_DetailElapsed	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_elapsed"));
		m_DetailMedic	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_medic"));
		m_DetailNote	= TextWidget.Cast(layoutRoot.FindAnyWidget("mmer_d_note"));
		m_DiagHolder	= layoutRoot.FindAnyWidget("mmer_diag_list");

		m_BtnAccept		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_accept"));
		m_BtnComplete	= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_complete"));
		m_BtnAbandon	= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_abandon"));
		m_BtnMark		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_mark"));
		m_BtnArchive	= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_archive"));
		m_BtnAdmin		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_admin"));
		m_BtnClose		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("mmer_btn_close"));

		// No SetHandler here. UIScriptedMenu derives from Managed, not from
		// ScriptedWidgetEventHandler, so it cannot be a widget handler - and it
		// does not need to be: the engine routes events for a menu's own
		// layoutRoot to the menu's OnClick override directly.
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

		MMER_ClientState state = MMER_ClientState.Get();
		state.RequestHello();

		if (m_BtnAdmin)
			m_BtnAdmin.Show(state.IsAdmin());

		m_ArchiveMode = false;
		m_ArchivePage = 0;

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

		// The client state pushes RefreshFromState() at us when a reply lands,
		// but that push is lost if this menu was not yet the current one at
		// that instant. Polling the dirty flags as well means a late archive
		// page or call list always reaches the screen.
		MMER_ClientState st = MMER_ClientState.Get();
		bool stale = st.ConsumeDirty();
		if (st.ConsumeArchiveDirty())
			stale = true;

		if (stale)
			RefreshFromState();

		m_Accum += timeslice;
		if (m_Accum >= 1.0)
		{
			m_Accum = 0;
			UpdateDetail();		// keeps the elapsed and range columns live
		}

		// ESC. The action is UAUIBack, read through the UAInput API - the
		// string form used previously ("UAUIMenu") is not the back action and
		// silently never fired, which is why the menu could not be closed.
		UAInput back = GetUApi().GetInputByID(UAUIBack);
		if (back && back.LocalPress())
			Close();
	}

	//--------------------------------------------------------------------------

	void SelectCall(int id)
	{
		m_SelectedId = id;
		UpdateDetail();
		UpdateButtons();
	}

	void RefreshFromState()
	{
		if (!layoutRoot)
			return;

		RebuildList();
		UpdateDetail();
		UpdateButtons();
		UpdateHeader();
	}

	protected void UpdateHeader()
	{
		MMER_ClientState state = MMER_ClientState.Get();

		if (m_Title)
			m_Title.SetText(state.GetSettings().panelTitle);

		if (!m_Subtitle)
			return;

		if (m_ArchiveMode)
		{
			MMER_ArchivePayload a = state.GetArchive();
			m_Subtitle.SetText(string.Format("Archive - page %1 of %2 (%3 records)",
				a.page + 1, Math.Max(1, a.pageCount), a.total));
		}
		else
		{
			m_Subtitle.SetText(string.Format("%1 open call(s)", state.OpenCount()));
		}
	}

	//--------------------------------------------------------------------------

	protected void RebuildList()
	{
		if (!m_ListHolder)
			return;

		// Tear the old rows down explicitly - handlers hold a back-reference
		// to this menu and would otherwise outlive it.
		MMER_WidgetUtil.ClearChildren(m_ListHolder);
		m_RowHandlers.Clear();

		array<ref MMER_Call> source = SourceList();
		int shown = 0;

		if (source)
		{
			for (int i = 0; i < source.Count(); i++)
			{
				MMER_Call call = source.Get(i);
				if (!call)
					continue;

				// Broken into locals rather than one wrapped condition: Enforce
				// will not parse a condition continued onto a line that starts
				// with an operator.
				if (!m_ArchiveMode)
				{
					bool isClosed = !call.IsOpen() && call.closedAt > 0;
					int sinceClosed = MMER_Time.NowUnix() - call.closedAt;

					if (isClosed && sinceClosed > 90)
						continue;
				}

				if (BuildRow(call))
					shown++;
			}
		}

		// A WrapSpacer does not reflow on its own after children are added or
		// removed from script - without this the rows stack at 0,0.
		m_ListHolder.Update();

		if (m_EmptyHint)
			m_EmptyHint.Show(shown == 0);

		if (shown > 0 && !FindSelected())
			SelectFirst(source);
	}

	protected array<ref MMER_Call> SourceList()
	{
		MMER_ClientState state = MMER_ClientState.Get();
		if (m_ArchiveMode)
			return state.GetArchive().calls;
		return state.GetCalls();
	}

	protected bool BuildRow(MMER_Call call)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(
			MMER_Const.LAYOUT_DIR + "mmer_call_row.layout", m_ListHolder);

		if (!row)
			return false;

		TextWidget id		= TextWidget.Cast(row.FindAnyWidget("mmer_row_id"));
		TextWidget name		= TextWidget.Cast(row.FindAnyWidget("mmer_row_name"));
		TextWidget grid		= TextWidget.Cast(row.FindAnyWidget("mmer_row_grid"));
		TextWidget stateTxt	= TextWidget.Cast(row.FindAnyWidget("mmer_row_state"));
		TextWidget age		= TextWidget.Cast(row.FindAnyWidget("mmer_row_age"));
		Widget stripe		= row.FindAnyWidget("mmer_row_stripe");

		MMER_ClientState client = MMER_ClientState.Get();

		if (id)		id.SetText("#" + call.id.ToString());
		if (name)	name.SetText(PatientLabel(call, client));

		// A redacted call carries a zeroed position, so GridRef() would draw a
		// perfectly plausible "000 000" and send someone to the map corner.
		if (grid)
		{
			if (call.redacted == 1)
				grid.SetText("- - -");
			else
				grid.SetText(call.GridRef());
		}
		if (age)	age.SetText(MMER_Time.Duration(call.AgeSeconds()));

		if (stateTxt)
		{
			stateTxt.SetText(MMER_CallState.ToLabel(call.state));
			stateTxt.SetColor(MMER_CallState.ToColor(call.state));
		}

		if (stripe)
			stripe.SetColor(MMER_CallState.ToColor(call.state));

		MMER_RowHandler handler = new MMER_RowHandler();
		handler.Setup(this, row, call.id);
		m_RowHandlers.Insert(handler);

		return true;
	}

	protected string PatientLabel(MMER_Call call, MMER_ClientState client)
	{
		// Two different silences. showPatientNames off is the operator choosing
		// anonymity for everyone; redacted is this viewer not having earned the
		// detail yet, and saying so is the point - it tells them the case is
		// real and that accepting it is what opens it up.
		if (call.redacted == 1)
			return "Unidentified";

		if (client.GetSettings().showPatientNames == 1 && call.patientName != "")
			return call.patientName;

		return "Survivor";
	}

	protected bool FindSelected()
	{
		for (int i = 0; i < m_RowHandlers.Count(); i++)
		{
			if (m_RowHandlers.Get(i).GetCallId() == m_SelectedId)
				return true;
		}
		return false;
	}

	protected void SelectFirst(array<ref MMER_Call> source)
	{
		// Prefer the oldest unclaimed call - that is the one that needs a
		// responder most.
		MMER_Call best = null;
		for (int i = 0; i < source.Count(); i++)
		{
			MMER_Call c = source.Get(i);
			if (!c)
				continue;

			if (c.state == MMER_CallState.NEW && (!best || c.createdAt < best.createdAt))
				best = c;
		}

		if (!best && source.Count() > 0)
			best = source.Get(0);

		if (best)
			m_SelectedId = best.id;
	}

	//--------------------------------------------------------------------------

	protected MMER_Call Selected()
	{
		array<ref MMER_Call> source = SourceList();
		if (!source)
			return null;

		for (int i = 0; i < source.Count(); i++)
		{
			MMER_Call c = source.Get(i);
			if (c && c.id == m_SelectedId)
				return c;
		}
		return null;
	}

	protected void UpdateDetail()
	{
		MMER_Call call = Selected();
		MMER_ClientState client = MMER_ClientState.Get();

		if (!call)
		{
			SetText(m_DetailName,	"--");
			SetText(m_DetailState,	"");
			SetText(m_DetailGrid,	"");
			SetText(m_DetailRange,	"");
			SetText(m_DetailElapsed, "");
			SetText(m_DetailMedic,	"");
			SetText(m_DetailNote,	"");
			ClearDiagnostics();
			return;
		}

		SetText(m_DetailName, PatientLabel(call, client));

		if (m_DetailState)
		{
			m_DetailState.SetText(MMER_CallState.ToLabel(call.state));
			m_DetailState.SetColor(MMER_CallState.ToColor(call.state));
		}

		// With markerMode 0 there is no pin anywhere, so this line is the whole
		// locate mechanism: grid for the map, raw metres for calling it out on
		// comms or pasting into a map tool.
		if (call.redacted == 1)
			SetText(m_DetailGrid, "Location withheld - accept the case to reveal it");
		else
			SetText(m_DetailGrid, string.Format("Grid %1  ·  %2 / %3",
				call.GridRef(), Math.Round(call.posX), Math.Round(call.posZ)));

		SetText(m_DetailElapsed, MMER_Time.Duration(call.AgeSeconds()));

		if (call.medicName != "")
			SetText(m_DetailMedic, "Responder: " + call.medicName);
		else
			SetText(m_DetailMedic, "Responder: unassigned");

		string note = call.note;
		if (call.closeReason != "")
		{
			if (note != "")
				note = note + " - ";
			note = note + call.closeReason;
		}
		SetText(m_DetailNote, note);

		if (call.redacted == 1)
		{
			SetText(m_DetailRange, "Range and bearing withheld");
			BuildDiagnostics(call);
			return;
		}

		PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
		if (me && m_DetailRange)
		{
			vector here	= me.GetPosition();
			vector there = call.Position();

			// Flat range - the 3D distance would read long on a hillside and
			// mislead the responder about how far they have to walk.
			float dist = vector.Distance(Vector(here[0], 0, here[2]), Vector(there[0], 0, there[2]));

			int climb = Math.Round(there[1] - here[1]);
			string climbTxt = climb.ToString() + " m";
			if (climb > 0)
				climbTxt = "+" + climbTxt;

			m_DetailRange.SetText(string.Format("%1 m  ·  %2  ·  alt %3",
				Math.Round(dist), MMER_Grid.Bearing(here, there), climbTxt));
		}

		BuildDiagnostics(call);
	}

	protected void ClearDiagnostics()
	{
		MMER_WidgetUtil.ClearChildren(m_DiagHolder);
	}

	protected void BuildDiagnostics(MMER_Call call)
	{
		ClearDiagnostics();

		if (!m_DiagHolder)
			return;

		// An empty readout and a withheld one look identical, and "no vitals"
		// on a patient reads as "nothing wrong". Say which it is.
		if (call.redacted == 1)
		{
			AddDiagRow("Vitals", "withheld", 2);
			AddDiagRow("", "Accept the case to see them", 2);
			m_DiagHolder.Update();
			return;
		}

		if (!call.diagnostics)
			return;

		for (int i = 0; i < call.diagnostics.Count(); i++)
		{
			MMER_DiagValue val = call.diagnostics.Get(i);
			if (!val)
				continue;

			Widget row = GetGame().GetWorkspace().CreateWidgets(
				MMER_Const.LAYOUT_DIR + "mmer_diag_row.layout", m_DiagHolder);

			if (!row)
				continue;

			TextWidget label = TextWidget.Cast(row.FindAnyWidget("mmer_diag_label"));
			TextWidget value = TextWidget.Cast(row.FindAnyWidget("mmer_diag_value"));

			if (label)
				label.SetText(val.label);

			if (value)
			{
				value.SetText(val.value);
				if (val.flag == 1)
					value.SetColor(0xFFE0A94B);		// out of range
				else if (val.flag == 2)
					value.SetColor(0xFF6A6A6A);		// not reported
				else
					value.SetColor(0xFFDDDDDD);
			}
		}

		m_DiagHolder.Update();
	}

	protected void AddDiagRow(string label, string value, int flag)
	{
		Widget row = GetGame().GetWorkspace().CreateWidgets(
			MMER_Const.LAYOUT_DIR + "mmer_diag_row.layout", m_DiagHolder);

		if (!row)
			return;

		TextWidget labelWidget = TextWidget.Cast(row.FindAnyWidget("mmer_diag_label"));
		TextWidget valueWidget = TextWidget.Cast(row.FindAnyWidget("mmer_diag_value"));

		if (labelWidget)
			labelWidget.SetText(label);

		if (valueWidget)
		{
			valueWidget.SetText(value);

			if (flag == 1)
				valueWidget.SetColor(0xFFE0A94B);
			else if (flag == 2)
				valueWidget.SetColor(0xFF6A6A6A);
			else
				valueWidget.SetColor(0xFFDDDDDD);
		}
	}

	protected void SetText(TextWidget w, string txt)
	{
		if (w)
			w.SetText(txt);
	}

	//--------------------------------------------------------------------------

	protected void UpdateButtons()
	{
		MMER_Call call = Selected();
		MMER_ClientState client = MMER_ClientState.Get();

		bool mine = false;
		bool isOpen = false;
		bool unclaimed = false;

		if (call)
		{
			isOpen		= call.IsOpen();
			unclaimed	= call.state == MMER_CallState.NEW;
			mine		= call.state == MMER_CallState.IN_PROGRESS && IsMine(call);
		}

		EnableButton(m_BtnAccept,	!m_ArchiveMode && unclaimed);
		EnableButton(m_BtnComplete,	!m_ArchiveMode && mine);
		EnableButton(m_BtnAbandon,	!m_ArchiveMode && mine);
		EnableButton(m_BtnMark,		!m_ArchiveMode && isOpen);

		if (m_BtnArchive)
		{
			if (m_ArchiveMode)
				m_BtnArchive.SetText("#STR_MMER_BTN_LIVE");
			else
				m_BtnArchive.SetText("#STR_MMER_BTN_ARCHIVE");
		}

		if (m_BtnAdmin)
			m_BtnAdmin.Show(client.IsAdmin());
	}

	protected bool IsMine(MMER_Call call)
	{
		// Compare against the uid the server sent us, not GetIdentity() - the
		// local player's PlayerIdentity is null on the client in multiplayer.
		string myUid = MMER_ClientState.Get().GetState().myUid;
		if (myUid == "" || call.medicUid == "")
			return false;

		return call.medicUid == myUid;
	}

	protected void EnableButton(ButtonWidget btn, bool enabled)
	{
		if (!btn)
			return;

		btn.Enable(enabled);
		if (enabled)
			btn.SetAlpha(1.0);
		else
			btn.SetAlpha(0.4);
	}

	//--------------------------------------------------------------------------

	override bool OnClick(Widget w, int x, int y, int button)
	{
		// Diagnostic while bringing the UI up: if nothing prints on click, the
		// event is not reaching the menu at all rather than being mis-routed.
		if (MMER_Const.DEBUG)
			Print("[MMER] Panel OnClick: " + w.GetName());

		MMER_ClientState state = MMER_ClientState.Get();

		if (w == m_BtnClose)
		{
			Close();
			return true;
		}

		if (w == m_BtnAccept)
		{
			state.AcceptCall(m_SelectedId);
			return true;
		}

		if (w == m_BtnComplete)
		{
			state.CompleteCall(m_SelectedId);
			return true;
		}

		if (w == m_BtnAbandon)
		{
			state.AbandonCall(m_SelectedId);
			return true;
		}

		if (w == m_BtnMark)
		{
			state.RemarkCall(m_SelectedId);
			return true;
		}

		if (w == m_BtnArchive)
		{
			m_ArchiveMode = !m_ArchiveMode;
			m_ArchivePage = 0;

			if (m_ArchiveMode)
				state.RequestArchive(0);

			RefreshFromState();
			return true;
		}

		if (w == m_BtnAdmin)
		{
			if (state.IsAdmin())
			{
				state.RequestRoster();
				GetGame().GetUIManager().EnterScriptedMenu(MMER_MenuID.ADMIN, this);
			}
			return true;
		}

		return super.OnClick(w, x, y, button);
	}
}
