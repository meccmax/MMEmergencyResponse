//------------------------------------------------------------------------------
// MM Emergency Response - bridge between the RPC layer (4_World, where
// PlayerBase lives) and the UI layer (5_Mission, where menus live).
//
// PlayerBase cannot reference 5_Mission types, so the client half registers a
// subclass of this here and the RPC handler talks to the base type only.
//------------------------------------------------------------------------------

class MMER_ClientSink
{
	static ref MMER_ClientSink s_Active;

	static void Register(MMER_ClientSink sink)
	{
		s_Active = sink;
	}

	static MMER_ClientSink Active()
	{
		return s_Active;
	}

	void OnSettings(string json)	{}
	void OnState(string json)		{}
	void OnCallList(string json)	{}
	void OnRoster(string json)		{}
	void OnArchive(string json)		{}
	void OnToast(string json)		{}
	void OnTags(string json)		{}
}
