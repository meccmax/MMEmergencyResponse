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
	protected TextWidget	m_Title;
	protected TextWidget	m_Status;
	protected TextWidget	m_Opt1;
	protected TextWidget	m_Opt2;
	protected TextWidget	m_Opt3;
	protected TextWidget	m_Hint;
	protected ImageWidget	m_Logo;

	// Returned when the server sent no wording block. Every field is "", which
	// means every line falls through to its built-in - so the call sites never
	// have to null-check.
	protected ref MMER_OverlayText m_NoWords;

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
		m_Title			= TextWidget.Cast(m_Root.FindAnyWidget("mmer_title"));
		m_Status		= TextWidget.Cast(m_Root.FindAnyWidget("mmer_status"));

		// Every one of these is optional and null-checked at the point of use.
		// A server still running the pre-1.8.1 layout keeps a working button
		// instead of losing the overlay over four missing text widgets.
		m_Opt1			= TextWidget.Cast(m_Root.FindAnyWidget("mmer_opt1"));
		m_Opt2			= TextWidget.Cast(m_Root.FindAnyWidget("mmer_opt2"));
		m_Opt3			= TextWidget.Cast(m_Root.FindAnyWidget("mmer_opt3"));
		m_Hint			= TextWidget.Cast(m_Root.FindAnyWidget("mmer_hint"));

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

		MMER_OverlayText w = Words();

		string label = Say(w.btnCall, "#STR_MMER_BTN_CALL");
		bool enabled = s.canCall == 1;

		if (s.myCallId > 0)
		{
			enabled	= true;
			label	= Say(w.btnCancel, "#STR_MMER_BTN_CANCEL");
		}
		else if (cooldown > 0)
		{
			enabled	= false;
			label	= Say(w.btnCooldown, "#STR_MMER_BTN_COOLDOWN");
		}

		m_Button.SetText(label);

		m_Button.Enable(enabled);
		m_Button.SetAlpha(1.0);
		if (!enabled)
			m_Button.SetAlpha(0.45);

		// The panel stays up for a moment after someone comes round with a call
		// still open, and on a requireUnconscious 0 server a conscious player
		// can call at all - "YOU ARE UNCONSCIOUS" would be a lie in both cases.
		if (m_Title)
		{
			if (player.IsUnconscious())
				FitText(m_Title, Say(w.title, "#STR_MMER_TITLE_DOWN"), 24);
			else
				FitText(m_Title, Say(w.titleCallActive, "#STR_MMER_TITLE_CALL"), 24);
		}

		if (m_Status)
			FitText(m_Status, StatusLine(s), 17);

		BuildOptions(s, cooldown);
	}

	//--------------------------------------------------------------------------
	// What the player can actually do, spelled out.
	//
	// Players kept treating the overlay as "a button that may or may not do
	// something", because a title and a status line never said that waiting and
	// respawning were also choices, or what pressing the button costs. Three
	// numbered lines, rewritten per state, and every claim in them is one the
	// server actually honours - the beacon lines are driven by the real config
	// rather than assumed, because copy that promises a refund the server does
	// not give is worse than no copy at all.
	//--------------------------------------------------------------------------

	protected void BuildOptions(MMER_StatePayload s, int cooldown)
	{
		if (!m_Opt1 && !m_Opt2 && !m_Opt3 && !m_Hint)
			return;

		MMER_OverlayText w = Words();

		string one		= "";
		string two		= "";
		string three	= "";
		string hint		= "";

		if (s.myCallId > 0 && s.myCallState == MMER_CallState.IN_PROGRESS)
		{
			// Someone is running across the map for them. The only thing that
			// matters now is that they do not respawn out from under it.
			one = Say(w.enroute1, "{medic} has your case and is on the way.");

			if (s.respawnBlocked == 1)
				two = Say(w.enroute2Held, "Respawn is held for up to {hold} while they come.");
			else
				two = Say(w.enroute2Free, "You can still respawn if you want to.");

			three = Say(w.enroute3, "Cancel below if you would rather not wait.");

			// Only claimed when a beacon was genuinely spent. On a server with
			// no beacon requirement this line would be describing nothing.
			if (BeaconIsSpent())
				hint = Say(w.enrouteHint, "Your {beacon} is spent - a responder answered it.");
		}
		else if (s.myCallId > 0)
		{
			one = Say(w.queued1, "Your call is out. Nobody has taken it yet.");

			if (BeaconIsSpent() && RefundsOnSelfRecovery())
				two = Say(w.queued2Refund, "Wake up first and your {beacon} comes back.");
			else
				two = Say(w.queued2, "Sit tight - a responder may still pick it up.");

			three = Say(w.queued3, "You can respawn or cancel at any time.");

			if (BeaconIsSpent() && RefundsOnQuickCancel())
				hint = Say(w.queuedHint, "Cancel within {cancel} to keep your {beacon}.");
		}
		else
		{
			one = Say(w.idle1, "1.  WAIT - you may come round on your own.");

			if (cooldown > 0)
				two = Say(w.idle2Cooldown, "2.  CALL FOR HELP - on cooldown for {cooldown}.");
			else if (s.canCall == 0)
				two = Say(w.idle2Unavailable, "2.  CALL FOR HELP - not available right now.");
			else if (BeaconIsSpent())
				two = Say(w.idle2Cost, "2.  CALL FOR HELP - costs one {beacon}.");
			else
				two = Say(w.idle2Free, "2.  CALL FOR HELP - a responder comes to you.");

			three = Say(w.idle3, "3.  RESPAWN - Esc > Respawn. You lose your gear.");

			if (BeaconIsSpent() && RefundsOnSelfRecovery())
				hint = Say(w.idleHintRefund, "Wake up first and your {beacon} is returned.");
			else
				hint = Say(w.idleHint, "Calling sends your location to the response team.");
		}

		FitText(m_Opt1, one, 19);
		FitText(m_Opt2, two, 19);
		FitText(m_Opt3, three, 19);
		FitText(m_Hint, hint, 16);
	}

	//--------------------------------------------------------------------------
	// Operator wording.
	//
	// Every visible line goes through Say(): the configured string if the
	// operator set one, otherwise the built-in. Empty means "use the built-in"
	// rather than "show nothing", which is what lets an existing config.json -
	// which has none of these keys - behave exactly as it did before.
	//--------------------------------------------------------------------------

	protected MMER_OverlayText Words()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (cs && cs.overlayText)
			return cs.overlayText;

		if (!m_NoWords)
			m_NoWords = new MMER_OverlayText;

		return m_NoWords;
	}

	protected string Say(string configured, string builtin)
	{
		string text = configured;
		if (text == "")
			text = builtin;

		return Fill(text);
	}

	// Token substitution.
	//
	// string.Replace MUTATES IN PLACE and returns an int (the number of hits),
	// exactly like ToLower - so it cannot be used inline and the caller's string
	// must not be touched. Assignment copies, so everything below works on a
	// local and the config value stays intact for the next frame.
	//
	// An unknown token is simply not matched and stays on screen as typed, which
	// is how a typo announces itself instead of silently eating a word.
	protected string Fill(string text)
	{
		if (text == "")
			return "";

		// Most lines carry no token at all; skip the work.
		if (text.IndexOf("{") < 0)
			return text;

		string t = text;

		MMER_StatePayload s = MMER_ClientState.Get().GetState();

		string medic = "A responder";
		if (s && s.myMedicName != "")
			medic = s.myMedicName;

		string team = "";
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (cs)
			team = cs.teamName;

		t.Replace("{beacon}", BeaconName());
		t.Replace("{medic}", medic);
		t.Replace("{cooldown}", MMER_Time.Duration(MMER_ClientState.Get().CooldownRemaining()));
		t.Replace("{hold}", HoldWindow());
		t.Replace("{cancel}", MMER_Time.Duration(QuickCancelWindow()));
		t.Replace("{team}", team);

		return t;
	}

	//--------------------------------------------------------------------------
	// Set the text, then make sure it actually fits.
	//
	// DayZ text does NOT shrink to fit its widget - it CLIPS, which is how the
	// first version of this panel ended up showing "your distr" and cutting the
	// rest off the right edge. The layout now pins a size with "exact text" +
	// "exact text size" rather than letting it derive from the box, and this
	// steps that size down until the rendered text fits in both directions.
	//
	// Both directions matter: the option rows wrap, so text too long for one
	// line grows downward instead of sideways and would be cut off vertically
	// instead. Measuring height catches that.
	//
	// This also covers two things a fixed size cannot: buttonScale, which
	// resizes the panel without touching the font, and longer strings from an
	// operator's own callItemLabel or a long player name.
	//--------------------------------------------------------------------------

	protected void FitText(TextWidget widget, string text, int baseSize)
	{
		if (!widget)
			return;

		widget.SetTextExactSize(baseSize);
		widget.SetText(text);

		if (text == "")
			return;

		float boxW, boxH;
		widget.GetScreenSize(boxW, boxH);

		if (boxW <= 0 || boxH <= 0)
			return;		// not laid out yet; next refresh will catch it

		int size = baseSize;

		// A floor, not a loop that runs forever. Below about 11px the text is
		// unreadable anyway, and stopping there is better than shrinking a
		// pathological string into nothing.
		while (size > 11)
		{
			int textW, textH;
			widget.GetTextSize(textW, textH);

			if (textW <= boxW && textH <= boxH)
				return;

			size = size - 1;
			widget.SetTextExactSize(size);
			widget.SetText(text);
		}
	}

	//--------------------------------------------------------------------------
	// Every one of these reads the server's own settings rather than assuming a
	// default, so the overlay cannot describe a rule this server does not run.
	//--------------------------------------------------------------------------

	protected string BeaconName()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (!cs || cs.callItemLabel == "")
			return "beacon";
		return cs.callItemLabel;
	}

	// Only true when a beacon is BOTH required and actually consumed. With
	// consumeCallItem 0 it is a carry requirement, nothing is spent, and every
	// refund line below would be describing something that never happens.
	protected bool BeaconIsSpent()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (!cs)
			return false;
		return cs.requireCallItem == 1 && cs.consumeCallItem == 1;
	}

	protected bool RefundsOnSelfRecovery()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (!cs)
			return false;
		return cs.refundOnSelfRecovery == 1;
	}

	protected bool RefundsOnQuickCancel()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (!cs)
			return false;
		return cs.refundCancelSeconds > 0;
	}

	protected int QuickCancelWindow()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();
		if (!cs)
			return 0;
		return cs.refundCancelSeconds;
	}

	// Just the duration - "15 min" - because it is a {hold} token now and the
	// surrounding words belong to whoever wrote the line. The server floors
	// respawnBlockMaxSeconds at 900 when it is set to zero or less, so this
	// mirrors that rather than returning something the server will not honour.
	protected string HoldWindow()
	{
		MMER_ClientSettings cs = MMER_ClientState.Get().GetSettings();

		int seconds = 900;
		if (cs && cs.respawnBlockMaxSeconds > 0)
			seconds = cs.respawnBlockMaxSeconds;

		int minutes = seconds / 60;
		if (minutes < 1)
			return "under a minute";

		return minutes.ToString() + " min";
	}

	// The built-in defaults here resolve their stringtable key FIRST and then
	// append a token, rather than being written out as literals. That keeps the
	// vanilla wording localisable while still letting the token work - a bare
	// "#KEY {medic}" would resolve to nothing, because Enforce only translates a
	// string that is entirely a key.
	protected string StatusLine(MMER_StatePayload s)
	{
		MMER_OverlayText w = Words();

		if (s.myCallId > 0)
		{
			if (s.myCallState == MMER_CallState.IN_PROGRESS)
				return Say(w.statusEnroute,
					MMER_WidgetUtil.Tr("#STR_MMER_STATUS_ENROUTE") + " {medic}");

			return Say(w.statusQueued, "#STR_MMER_STATUS_QUEUED");
		}

		if (MMER_ClientState.Get().CooldownRemaining() > 0)
			return Say(w.statusCooldown,
				MMER_WidgetUtil.Tr("#STR_MMER_STATUS_COOLDOWN") + " {cooldown}");

		if (s.canCall == 0)
			return Say(w.statusUnavailable, "#STR_MMER_STATUS_UNAVAILABLE");

		return Say(w.statusDown, "#STR_MMER_STATUS_READY");
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
