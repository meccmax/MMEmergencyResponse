//------------------------------------------------------------------------------
// MM Emergency Response - the overlay a downed player sees.
//
// Lives on top of the unconscious vignette rather than inside a menu, because
// opening a scripted menu while unconscious fights the engine's input lock.
// The widget is created once and shown/hidden, so there is no per-frame churn.
//------------------------------------------------------------------------------

class MMER_CallButton extends ScriptedWidgetEventHandler
{
	static ref MMER_CallButton	s_Instance;

	protected Widget		m_Root;
	protected Widget		m_Panel;
	protected ButtonWidget	m_Button;
	protected TextWidget	m_Status;
	protected ImageWidget	m_Logo;

	protected bool			m_Visible;
	protected bool			m_CursorForced;
	protected bool			m_LayoutFailed;
	protected int			m_LastRefresh;

	//--------------------------------------------------------------------------

	static MMER_CallButton Get()
	{
		if (!s_Instance)
			s_Instance = new MMER_CallButton();
		return s_Instance;
	}

	//--------------------------------------------------------------------------

	protected bool EnsureWidgets()
	{
		if (m_Root)
			return true;

		// Refresh() runs once a second while unconscious. Without this latch a
		// layout that fails to load would be retried every second for as long
		// as the player is down.
		if (m_LayoutFailed)
			return false;

		m_Root = GetGame().GetWorkspace().CreateWidgets(MMER_Const.LAYOUT_DIR + "mmer_call_button.layout");
		if (!m_Root)
		{
			m_LayoutFailed = true;
			MMER_Log.Error("mmer_call_button.layout failed to load - the emergency button will not appear.");
			return false;
		}

		m_Panel			= m_Root.FindAnyWidget("mmer_panel");
		m_Button		= ButtonWidget.Cast(m_Root.FindAnyWidget("mmer_call_btn"));
		m_Status		= TextWidget.Cast(m_Root.FindAnyWidget("mmer_status"));
		m_Logo			= ImageWidget.Cast(m_Root.FindAnyWidget("mmer_logo"));

		if (!m_Button)
		{
			m_LayoutFailed = true;
			MMER_Log.Error("mmer_call_button.layout is missing the 'mmer_call_btn' ButtonWidget.");
			m_Root.Unlink();
			m_Root = null;
			return false;
		}

		m_Root.SetHandler(this);
		m_Root.Show(false);

		ApplyPlacement();
		return true;
	}

	protected void ApplyPlacement()
	{
		MMER_ClientSettings settings = MMER_ClientState.Get().GetSettings();
		if (!m_Panel || !settings)
			return;

		float w, h;
		m_Panel.GetSize(w, h);

		float scale = settings.buttonScale;
		if (scale <= 0)
			scale = 1.0;

		m_Panel.SetSize(w * scale, h * scale);
		m_Panel.SetPos(settings.buttonX - (w * scale * 0.5), settings.buttonY - (h * scale * 0.5));

		// Hide the logo unless an image actually loads. Leaving an ImageWidget
		// visible with no texture draws a white rectangle, which is what the
		// blank box next to "YOU ARE DOWN" was.
		if (!m_Logo)
			return;

		bool loaded = false;
		if (settings.serverLogo != "")
			loaded = m_Logo.LoadImageFile(0, settings.serverLogo);

		m_Logo.Show(loaded);
	}

	//--------------------------------------------------------------------------
	// Called on state change and once a second from MissionGameplay.
	//--------------------------------------------------------------------------

	void Refresh()
	{
		MMER_ClientState state = MMER_ClientState.Get();
		MMER_StatePayload s = state.GetState();

		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		bool shouldShow = player && player.IsAlive() && player.IsUnconscious();

		// Keep the panel up while a call is live so the patient can see the
		// responder's name and cancel if they want to.
		if (player && player.IsAlive() && s.myCallId > 0)
			shouldShow = true;

		if (!shouldShow)
		{
			Hide();
			return;
		}

		if (!EnsureWidgets())
			return;

		Show();

		int cooldown = state.CooldownRemaining();

		string label = "#STR_MMER_BTN_CALL";
		bool enabled = s.canCall == 1;

		if (s.myCallId > 0)
		{
			enabled	= true;
			label	= "#STR_MMER_BTN_CANCEL";
		}
		else if (cooldown > 0)
		{
			enabled	= false;
			label	= "#STR_MMER_BTN_COOLDOWN";
		}

		m_Button.SetText(label);

		m_Button.Enable(enabled);
		m_Button.SetAlpha(1.0);
		if (!enabled)
			m_Button.SetAlpha(0.45);

		if (m_Status)
			m_Status.SetText(StatusLine(s));
	}

	protected string StatusLine(MMER_StatePayload s)
	{
		if (s.myCallId > 0)
		{
			if (s.myCallState == MMER_CallState.IN_PROGRESS)
			{
				if (s.myMedicName != "")
					return MMER_WidgetUtil.Tr("#STR_MMER_STATUS_ENROUTE") + " " + s.myMedicName;
				return "#STR_MMER_STATUS_ENROUTE";
			}
			return "#STR_MMER_STATUS_QUEUED";
		}

		int cooldown = MMER_ClientState.Get().CooldownRemaining();
		if (cooldown > 0)
			return MMER_WidgetUtil.Tr("#STR_MMER_STATUS_COOLDOWN") + " " + MMER_Time.Duration(cooldown);

		if (s.canCall == 0)
			return "#STR_MMER_STATUS_UNAVAILABLE";

		return "#STR_MMER_STATUS_READY";
	}

	//--------------------------------------------------------------------------

	void Show()
	{
		if (m_Visible || !m_Root)
			return;

		m_Visible = true;
		m_Root.Show(true);

		// The engine hides the cursor AND locks input while unconscious. Showing
		// the cursor alone is not enough - without taking a game focus the
		// overlay renders but swallows no clicks, which is exactly how it
		// behaved before this was added.
		if (!m_CursorForced)
		{
			m_CursorForced = true;
			GetGame().GetUIManager().ShowUICursor(true);
			GetGame().GetInput().ChangeGameFocus(1);

			// Same reason as the menus: without releasing player control the
			// game swallows the mouse and the button never sees a click.
			GetGame().GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
		}
	}

	void Hide()
	{
		if (!m_Visible)
			return;

		m_Visible = false;

		if (m_Root)
			m_Root.Show(false);

		if (m_CursorForced)
		{
			m_CursorForced = false;

			// Decrement, never reset. ResetGameFocus() zeroes a counter other
			// systems also use; every ChangeGameFocus(1) must be paired with
			// exactly one ChangeGameFocus(-1).
			GetGame().GetInput().ChangeGameFocus(-1);
			GetGame().GetMission().PlayerControlEnable(false);

			// Only release the cursor if nothing else wants it.
			if (!GetGame().GetUIManager().GetMenu())
				GetGame().GetUIManager().ShowUICursor(false);
		}
	}

	void Destroy()
	{
		Hide();
		if (m_Root)
		{
			m_Root.Unlink();
			m_Root = null;
		}
	}

	bool IsVisible()
	{
		return m_Visible;
	}

	//--------------------------------------------------------------------------

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		if (MMER_Const.DEBUG)
			Print("[MMER] Overlay OnClick: " + w.GetName());

		if (w != m_Button)
			return super.OnClick(w, x, y, button);

		MMER_ClientState state = MMER_ClientState.Get();
		if (state.GetState().myCallId > 0)
			state.CancelCall();
		else
			state.RequestCall();

		// Optimistic lockout until the server answers, so a panicking player
		// cannot spam the button and burn their cooldown on a rejected call.
		m_Button.Enable(false);
		return true;
	}

	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (w == m_Button)
			w.SetAlpha(0.85);
		return true;
	}

	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (w == m_Button)
			w.SetAlpha(1.0);
		return true;
	}
}
