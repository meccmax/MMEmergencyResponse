//------------------------------------------------------------------------------
// MM Emergency Response - chat tags.
//
// Draws "[MEDEVAC] Name :" instead of "Name :" for players who are on the
// response roster. Purely cosmetic and purely client-side: the tag grants
// nothing and is checked nowhere. Chat carries a display name and no Steam64
// (see ChatMessageEventParams in 3_Game/gameplay.c - param2 is the sender's
// name), so this is a name match by necessity. A player who renames themselves
// to match a responder gets the tag too; that is a display quirk, not an
// access path, because nothing in this mod trusts a name.
//
// The name text is rebuilt rather than read back and prefixed: TextWidget has
// no GetText() in the engine API, only SetText().
//------------------------------------------------------------------------------

modded class ChatLine
{
	override void Set(ChatMessageEventParams params)
	{
		super.Set(params);

		if (!m_NameWidget)
			return;

		string sender = params.param2;

		// Continuation lines of a message Chat.Add() had to wrap carry an empty
		// sender - tagging those would print the tag twice.
		if (sender == "")
			return;

		int channel = params.param1;

		// System, admin and BattlEye lines are not people and already carry
		// their own prefixes.
		if (channel & CCSystem)
			return;
		if (channel & CCAdmin)
			return;
		if (channel & CCBattlEye)
			return;

		MMER_ClientState state = MMER_ClientState.Get();
		MMER_ClientSettings cfg = state.GetSettings();

		if (cfg.chatTagEnabled == 0)
			return;

		int role = state.TagRoleFor(sender);
		if (role == MMER_Role.NONE)
			return;

		string tag = cfg.chatTagText;
		string colour = cfg.chatTagColor;

		if (role == MMER_Role.ADMIN)
		{
			tag = cfg.chatTagAdminText;
			colour = cfg.chatTagAdminColor;
		}

		if (tag == "")
			return;

		// Rebuild exactly what vanilla writes for this channel, with the tag in
		// front. The radio prefix is a stringtable key the engine resolves.
		string line;
		if (channel & CCTransmitter)
			line = tag + " (#str_radio) " + sender + " : ";
		else
			line = tag + " " + sender + " : ";

		m_NameWidget.SetText(line);
		m_NameWidget.SetColor(MMER_Color.Parse(colour, 0xFFFFFFFF));
	}
}
