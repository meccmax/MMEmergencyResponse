//------------------------------------------------------------------------------
// MM Emergency Response - client mission hooks: menu registration, the panel
// hotkey, and the once-a-second refresh that drives the unconscious overlay.
//------------------------------------------------------------------------------

class MMER_MenuID
{
	static const int PANEL	= 27860;
	static const int ADMIN	= 27861;
}

modded class MissionGameplay
{
	protected float	m_MMER_Accum;
	protected bool	m_MMER_SaidHello;
	protected bool	m_MMER_WarnedNoInput;

	override void OnInit()
	{
		super.OnInit();
		MMER_ClientState.Get();		// registers the RPC sink
	}

	override UIScriptedMenu CreateScriptedMenu(int id)
	{
		switch (id)
		{
			case MMER_MenuID.PANEL:	return new MMER_PanelMenu();
			case MMER_MenuID.ADMIN:	return new MMER_AdminMenu();
		}
		return super.CreateScriptedMenu(id);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!GetGame() || !GetGame().GetPlayer())
			return;

		// The hotkey has to be polled every frame, not on the 1 Hz tick -
		// LocalPress is true only on the frame the key goes down.
		MMER_PollHotkey();

		m_MMER_Accum += timeslice;
		if (m_MMER_Accum < 1.0)
			return;
		m_MMER_Accum = 0;

		// One handshake once the character actually exists - the connect-time
		// push can land before the client has a player to attach it to.
		if (!m_MMER_SaidHello)
		{
			m_MMER_SaidHello = true;
			MMER_ClientState.Get().RequestHello();
		}

		MMER_CallButton.Get().Refresh();
	}

	//--------------------------------------------------------------------------
	// Hotkey.
	//
	// This goes through the registered UAInput from Scripts/Data/Inputs.xml
	// rather than Mission.OnKeyPress. OnKeyPress is documented as debug-only:
	// it is not reliably delivered for player-facing actions, the key cannot be
	// rebound, and it never appears in Settings > Controls. Using UAInput means
	// players can rebind it themselves and it survives collisions with other
	// mods' bindings.
	//--------------------------------------------------------------------------
	protected void MMER_PollHotkey()
	{
		UAInputAPI api = GetUApi();
		if (!api)
			return;

		UAInput input = api.GetInputByName("UAMMEROpenPanel");

		// NOTE: GetInputByName returns a stub for an unregistered action rather
		// than null, so a null check here proves nothing. If Inputs.xml did not
		// make it into the PBO this simply never fires - which is why the raw
		// key fallback below exists.
		if (input && input.LocalPress())
			MMER_OpenPanel("keybind");
	}

	//--------------------------------------------------------------------------
	// Raw-key fallback.
	//
	// The UAInput path above is the correct one - rebindable, visible in
	// Settings > Controls. But it depends on Inputs.xml surviving the PBO build,
	// and if it does not, the mod has no hotkey at all and nothing says why.
	// This keeps the panel reachable regardless. It is deliberately secondary:
	// if the registered input fires, this never runs.
	//--------------------------------------------------------------------------
	override void OnKeyPress(int key)
	{
		super.OnKeyPress(key);

		if (key == KeyCode.KC_K)
			MMER_OpenPanel("rawkey");
	}

	protected void MMER_OpenPanel(string source)
	{
		MMER_ClientState state = MMER_ClientState.Get();

		if (MMER_Const.DEBUG)
			Print("[MMER] Panel open requested via " + source + ", role=" + state.GetState().role.ToString());

		if (!state.IsResponder())
			return;

		// Never steal the key from a text field or an already-open menu.
		UIManager ui = GetGame().GetUIManager();
		if (!ui || ui.GetMenu() || ui.IsDialogVisible())
			return;

		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (!player || !player.IsAlive() || player.IsUnconscious())
			return;

		ui.EnterScriptedMenu(MMER_MenuID.PANEL, null);
	}

	override void OnMissionFinish()
	{
		MMER_CallButton.Get().Destroy();
		super.OnMissionFinish();
	}
}
