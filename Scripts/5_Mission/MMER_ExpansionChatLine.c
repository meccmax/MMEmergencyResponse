//------------------------------------------------------------------------------
// MM Emergency Response - chat tags on a DayZ Expansion server.
//
// Expansion Chat replaces the chat renderer wholesale: it draws its own line
// as "HH:MM [Channel] Name: text" from ExpansionChatLineBase, and vanilla's
// ChatLine is never instantiated. The hook in MMER_ChatLine.c is therefore
// dead on an Expansion server - correct code attached to a class that does not
// draw anything. This file is the same feature attached to the class that does.
//
// Gated behind MMER_EXPANSION_CHAT (see MMER_00_MissionDefines.c) because
// "modded class ExpansionChatLineBase" will not compile without Expansion
// present. Both hooks can be enabled at once: they target different classes,
// only one of which is ever live.
//
// Verified against salutesh/DayZ-Expansion-Scripts,
// DayZExpansion/Chat/Scripts/5_Mission/DayZExpansion_Chat/ExpansionChatLine.c.
//------------------------------------------------------------------------------

#ifdef MMER_EXPANSION_CHAT

modded class ExpansionChatLineBase
{
	// Colour for the line currently being drawn; 0 means "not one of ours".
	// Expansion's Set() calls SetSenderName() and then SenderSetColour(white),
	// so the colour has to be applied in the second call or it gets overwritten
	// by the first.
	protected int m_MMER_TagColour;

	override protected void SetSenderName(ExpansionChatMessage message, string fallback = " ")
	{
		m_MMER_TagColour = 0;

		if (message)
		{
			string tag;
			int colour;

			if (MMER_ChatTag.Lookup(message.From, tag, colour))
			{
				// Expansion already renders PlayerTag ahead of the name, so use
				// its own slot rather than fighting it. SenderName is private on
				// the base class and cannot be touched from here anyway.
				//
				// Prepend rather than assign, and only when our tag is not
				// already at the front: a line can be re-rendered (chat resize
				// redraws every row), and this must not stack up copies of the
				// tag or wipe a tag Expansion set for its own reasons.
				if (message.PlayerTag.IndexOf(tag) != 0)
					message.PlayerTag = tag + " " + message.PlayerTag;

				m_MMER_TagColour = colour;
			}
		}

		super.SetSenderName(message, fallback);
	}

	override protected void SenderSetColour(int colour)
	{
		if (m_MMER_TagColour != 0)
		{
			super.SenderSetColour(m_MMER_TagColour);
			return;
		}

		super.SenderSetColour(colour);
	}
}

#endif
