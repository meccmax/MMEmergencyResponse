//------------------------------------------------------------------------------
// MM Emergency Response - chat tags on a VANILLA chat renderer.
//
// Draws "[MEDEVAC] Name :" instead of "Name :" for players on the response
// roster. Purely cosmetic and purely client-side: the tag grants nothing and
// is checked nowhere.
//
// IMPORTANT: this hook only fires where vanilla is actually drawing the chat.
// A mod that replaces the chat renderer - DayZ Expansion Chat above all, which
// draws "HH:MM [Channel] Name: text" from its own class - never instantiates
// vanilla's ChatLine, so nothing here runs and no tag appears. That case is
// handled by MMER_ExpansionChatLine.c, behind MMER_EXPANSION_CHAT. Enabling
// both is fine; only one of the two classes is ever live on a given server.
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

		string tag;
		int colour;

		if (!MMER_ChatTag.Lookup(sender, tag, colour))
			return;

		// Rebuild exactly what vanilla writes for this channel, with the tag in
		// front. The radio prefix is a stringtable key the engine resolves.
		string line;
		if (channel & CCTransmitter)
			line = tag + " (#str_radio) " + sender + " : ";
		else
			line = tag + " " + sender + " : ";

		m_NameWidget.SetText(line);
		m_NameWidget.SetColor(colour);
	}
}
