//------------------------------------------------------------------------------
// MM Emergency Response - shared chat-tag lookup.
//
// The decision "does this sender get a tag, and which one" is identical whether
// vanilla or Expansion is drawing the line, so it lives here and both hooks
// call it. Deliberately free of any Expansion type, so this file compiles on a
// server that has never heard of Expansion.
//
// Chat carries a display NAME and no Steam64 (ChatMessageEventParams.param2 in
// 3_Game/gameplay.c), so the match is by name out of necessity. That is fine
// precisely because the tag is cosmetic: nothing in this mod trusts a name for
// anything, so the worst a player who renames themselves to match a responder
// achieves is a coloured prefix.
//------------------------------------------------------------------------------

class MMER_ChatTag
{
	// True when this sender should be tagged. tag and colour are only
	// meaningful when it returns true.
	static bool Lookup(string senderName, out string tag, out int colour)
	{
		tag = "";
		colour = 0;

		if (senderName == "")
			return false;

		MMER_ClientState state = MMER_ClientState.Get();
		if (!state)
			return false;

		MMER_ClientSettings cfg = state.GetSettings();
		if (!cfg || cfg.chatTagEnabled == 0)
			return false;

		int role = state.TagRoleFor(senderName);
		if (role == MMER_Role.NONE)
			return false;

		string text = cfg.chatTagText;
		string hex = cfg.chatTagColor;

		if (role == MMER_Role.ADMIN)
		{
			text = cfg.chatTagAdminText;
			hex = cfg.chatTagAdminColor;
		}

		if (text == "")
			return false;

		tag = text;
		colour = MMER_Color.Parse(hex, 0xFFFFFFFF);

		// 0 would be read as "no tag" by the callers, and a fully transparent
		// tag is not something anyone asked for.
		if (colour == 0)
			colour = 0xFFFFFFFF;

		return true;
	}
}
