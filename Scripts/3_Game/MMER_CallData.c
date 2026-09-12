//------------------------------------------------------------------------------
// MM Emergency Response - the data that moves between server and client.
// Everything here is plain JSON-serialisable so it survives both the RPC hop
// (as a string) and disk persistence without a second representation.
//------------------------------------------------------------------------------

class MMER_DiagValue
{
	string	label	= "";
	string	value	= "";
	int		flag	= 0;		// 0 normal, 1 warning, 2 unavailable
}

class MMER_Call
{
	int		id				= 0;
	int		state			= 0;

	string	patientUid		= "";
	string	patientName		= "";
	float	posX			= 0;
	float	posY			= 0;
	float	posZ			= 0;

	string	medicUid		= "";
	string	medicName		= "";

	int		createdAt		= 0;	// unix UTC
	int		acceptedAt		= 0;
	int		closedAt		= 0;

	string	note			= "";
	string	closeReason		= "";

	// Whether the patient was actually unconscious when the call was made.
	// Auto-close-on-revive keys off this: regaining consciousness only means
	// "recovered" for someone who was unconscious to begin with.
	int		wasUnconscious	= 0;

	// Class name of the beacon spent to open this call, so it can be handed back
	// if nobody ever answers. Server-side bookkeeping: Slim() does not copy it,
	// so it never reaches a client, but SaveActive() writes whole calls, so it
	// survives a restart mid-case.
	string	consumedItem	= "";

	// Ref members are built in the constructor rather than inline. Enforce is
	// inconsistent about member initialisers that allocate, and a constructor
	// is unambiguous everywhere.
	ref array<ref MMER_DiagValue> diagnostics;

	void MMER_Call()
	{
		diagnostics = new array<ref MMER_DiagValue>;
	}

	vector Position()
	{
		return Vector(posX, posY, posZ);
	}

	void SetPosition(vector v)
	{
		posX = v[0];
		posY = v[1];
		posZ = v[2];
	}

	string GridRef()
	{
		return MMER_Grid.Ref(posX, posZ);
	}

	int AgeSeconds()
	{
		int endTime = closedAt;
		if (endTime <= 0)
			endTime = MMER_Time.NowUnix();
		return endTime - createdAt;
	}

	bool IsOpen()
	{
		return !MMER_CallState.IsClosed(state);
	}

	// A copy without the diagnostics payload - used for the list view so the
	// sync packet stays small.
	MMER_Call Slim()
	{
		MMER_Call c		= new MMER_Call;
		c.id			= id;
		c.state			= state;
		c.patientUid	= patientUid;
		c.patientName	= patientName;
		c.posX			= posX;
		c.posY			= posY;
		c.posZ			= posZ;
		c.medicUid		= medicUid;
		c.medicName		= medicName;
		c.createdAt		= createdAt;
		c.acceptedAt	= acceptedAt;
		c.closedAt		= closedAt;
		c.note			= note;
		c.closeReason	= closeReason;
		c.wasUnconscious = wasUnconscious;
		return c;
	}

	// A copy safe to put on the wire for one specific viewer.
	//
	// patientUid is stripped outright. Nothing on the client ever reads it, and
	// sending it handed every responder a name-to-Steam64 table for anyone who
	// has ever been downed - the archive delivered hundreds of rows of that in
	// one request. medicUid survives only when the viewer IS that medic, which
	// is the single thing the client uses it for (PanelMenu.IsMine); "claimed
	// by someone else" is read off the state, not the uid.
	//
	// showNames honours the operator's showPatientNames setting HERE rather
	// than at the widget: enforcing anonymity in the client is no anonymity at
	// all against a modified one.
	MMER_Call ForViewer(string viewerUid, bool showNames, bool withDiagnostics)
	{
		MMER_Call c = Slim();

		c.patientUid = "";

		if (medicUid != viewerUid)
			c.medicUid = "";

		// The client already falls back to "Survivor" on an empty name.
		if (!showNames)
			c.patientName = "";

		if (withDiagnostics && diagnostics)
		{
			for (int i = 0; i < diagnostics.Count(); i++)
				c.diagnostics.Insert(diagnostics.Get(i));
		}

		return c;
	}
}

//------------------------------------------------------------------------------
// RPC payload wrappers. Each of these is serialised to a JSON string and sent
// as a Param1<string>, which sidesteps the Param arity limits and keeps the
// wire format easy to log and diff.
//------------------------------------------------------------------------------

class MMER_CallListPayload
{
	int		serverTime	= 0;
	int		openCount	= 0;
	ref array<ref MMER_Call> calls;

	void MMER_CallListPayload()
	{
		calls = new array<ref MMER_Call>;
	}

	// Serialisation MUST go through JsonFileLoader<MMER_CallListPayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_CallListPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_CallListPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_CallListPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_CallListPayload obj = new MMER_CallListPayload;
		string err;
		if (!JsonFileLoader<MMER_CallListPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_CallListPayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

class MMER_StatePayload
{
	// The client cannot read its own Steam64 reliably - PlayerIdentity is a
	// server-side object and GetIdentity() on the local player is routinely
	// null in multiplayer - so the server tells it.
	string	myUid			= "";
	int		role			= 0;	// MMER_Role
	int		canCall			= 0;
	int		cooldownLeft	= 0;	// seconds
	int		myCallId		= 0;	// 0 when the player has no open call
	int		myCallState		= 0;
	string	myMedicName		= "";
	int		respawnBlocked	= 0;

	// Serialisation MUST go through JsonFileLoader<MMER_StatePayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_StatePayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_StatePayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_StatePayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_StatePayload obj = new MMER_StatePayload;
		string err;
		if (!JsonFileLoader<MMER_StatePayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_StatePayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

class MMER_RosterPayload
{
	ref TStringArray admins;
	ref TStringArray members;
	ref TStringArray onlineUids;
	ref TStringArray onlineNames;

	void MMER_RosterPayload()
	{
		admins		= new TStringArray;
		members		= new TStringArray;
		onlineUids	= new TStringArray;
		onlineNames	= new TStringArray;
	}

	// Serialisation MUST go through JsonFileLoader<MMER_RosterPayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_RosterPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_RosterPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_RosterPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_RosterPayload obj = new MMER_RosterPayload;
		string err;
		if (!JsonFileLoader<MMER_RosterPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_RosterPayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

class MMER_ArchivePayload
{
	int		page		= 0;
	int		pageCount	= 0;
	int		total		= 0;
	ref array<ref MMER_Call> calls;

	void MMER_ArchivePayload()
	{
		calls = new array<ref MMER_Call>;
	}

	// Serialisation MUST go through JsonFileLoader<MMER_ArchivePayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_ArchivePayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_ArchivePayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_ArchivePayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_ArchivePayload obj = new MMER_ArchivePayload;
		string err;
		if (!JsonFileLoader<MMER_ArchivePayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_ArchivePayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

//------------------------------------------------------------------------------
// Names of the responders and admins currently online, pushed to EVERY client
// so chat can tag them. Names only - no Steam64s, because every client gets
// this and a UID is not something to hand out to the whole server.
//------------------------------------------------------------------------------
class MMER_TagPayload
{
	ref TStringArray responders;
	ref TStringArray admins;

	void MMER_TagPayload()
	{
		responders	= new TStringArray;
		admins		= new TStringArray;
	}

	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_TagPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_TagPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_TagPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_TagPayload obj = new MMER_TagPayload;
		string err;
		if (!JsonFileLoader<MMER_TagPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_TagPayload parse failed: " + err);
			return null;
		}
		return obj;
	}
}

class MMER_ArchiveFile
{
	int		nextId		= 1;
	ref array<ref MMER_Call> calls;

	void MMER_ArchiveFile()
	{
		calls = new array<ref MMER_Call>;
	}
}

class MMER_ToastPayload
{
	int		kind		= 0;	// MMER_Toast
	string	title		= "";
	string	body		= "";
	int		callId		= 0;

	// Serialisation MUST go through JsonFileLoader<MMER_ToastPayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_ToastPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_ToastPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_ToastPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_ToastPayload obj = new MMER_ToastPayload;
		string err;
		if (!JsonFileLoader<MMER_ToastPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_ToastPayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

class MMER_IntPayload
{
	int		value		= 0;

	// Serialisation MUST go through JsonFileLoader<MMER_IntPayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_IntPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_IntPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_IntPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_IntPayload obj = new MMER_IntPayload;
		string err;
		if (!JsonFileLoader<MMER_IntPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_IntPayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

class MMER_StringPayload
{
	string	value		= "";

	// Serialisation MUST go through JsonFileLoader<MMER_StringPayload> - the templated
	// form. Handing an object to JsonSerializer through a parameter typed as
	// the base "Class" produces "{}": the serializer works off the static
	// type, so it sees no members. That is what shipped in 1.1.1 and it made
	// every RPC arrive as an empty object.
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_StringPayload>.MakeData(this, json, err, false))
		{
			MMER_Log.Warn("MMER_StringPayload serialise failed: " + err);
			return "";
		}
		return json;
	}

	static MMER_StringPayload FromJson(string data)
	{
		if (data == "")
			return null;

		MMER_StringPayload obj = new MMER_StringPayload;
		string err;
		if (!JsonFileLoader<MMER_StringPayload>.LoadData(data, obj, err))
		{
			MMER_Log.Warn("MMER_StringPayload parse failed: " + err);
			return null;
		}
		return obj;
	}

}

//------------------------------------------------------------------------------
// NOTE: there is deliberately no generic MMER_Json.Write(Class obj) helper.
// See the comment on ToJson above - a Class-typed parameter serialises to "{}".
// Every payload carries its own typed ToJson/FromJson instead.
//------------------------------------------------------------------------------
