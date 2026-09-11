//------------------------------------------------------------------------------
// MM Emergency Response - respawn blocking.
//
// Client side only, and deliberately so: this is a "don't waste your responder's
// time" nudge, not an anti-cheat measure. The server never trusts it, and the
// block lifts on its own after respawnBlockMaxSeconds so nobody is ever stuck.
//------------------------------------------------------------------------------

modded class InGameMenu
{
	// Vanilla exposes three respawn entry points depending on the server's
	// respawn mode, so all three have to be locked or the block leaks.
	protected ButtonWidget	m_MMER_RespawnBtn;
	protected ButtonWidget	m_MMER_RespawnRandomBtn;
	protected ButtonWidget	m_MMER_RespawnCustomBtn;
	// Built at runtime, not looked up: this is vanilla's layout and there is no
	// mmer_respawn_notice in it. FindAnyWidget() returned null every time, so
	// the player was blocked from respawning with nothing on screen saying why.
	protected TextWidget	m_MMER_Notice;

	override Widget Init()
	{
		Widget root = super.Init();
		if (!root)
			return root;

		m_MMER_RespawnBtn		= ButtonWidget.Cast(root.FindAnyWidget("respawn_button"));
		m_MMER_RespawnRandomBtn	= ButtonWidget.Cast(root.FindAnyWidget("respawn_button_random"));
		m_MMER_RespawnCustomBtn	= ButtonWidget.Cast(root.FindAnyWidget("respawn_button_custom"));

		m_MMER_Notice = TextWidget.Cast(
			GetGame().GetWorkspace().CreateWidgets("MMEmergencyResponse/GUI/layouts/mmer_respawn_notice.layout", root));

		if (m_MMER_Notice)
			m_MMER_Notice.Show(false);

		return root;
	}

	override void OnShow()
	{
		super.OnShow();

		// Belt and braces for the respawn lock: ask the server for fresh state
		// every time this menu opens, so a client holding a stale "blocked"
		// flag can never be stuck staring at a disabled button.
		MMER_ClientState.Get().RequestHello();

		MMER_ApplyRespawnBlock();
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		MMER_ApplyRespawnBlock();
	}

	void MMER_ApplyRespawnBlock()
	{
		MMER_ClientState state = MMER_ClientState.Get();
		if (state.GetSettings().blockRespawn == 0)
			return;

		bool blocked = state.GetState().respawnBlocked == 1;

		MMER_SetRespawnBtn(m_MMER_RespawnBtn, blocked);
		MMER_SetRespawnBtn(m_MMER_RespawnRandomBtn, blocked);
		MMER_SetRespawnBtn(m_MMER_RespawnCustomBtn, blocked);

		if (m_MMER_Notice)
		{
			m_MMER_Notice.Show(blocked);
			if (blocked)
				m_MMER_Notice.SetText("#STR_MMER_RESPAWN_BLOCKED");
		}
	}

	protected void MMER_SetRespawnBtn(ButtonWidget btn, bool blocked)
	{
		if (!btn)
			return;

		btn.Enable(!blocked);

		if (blocked)
			btn.SetAlpha(0.35);
		else
			btn.SetAlpha(1.0);
	}
}
