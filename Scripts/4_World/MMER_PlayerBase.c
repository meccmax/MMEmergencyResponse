//------------------------------------------------------------------------------
// MM Emergency Response - PlayerBase hooks.
//
// All traffic rides on the vanilla per-entity RPC channel, so no Community
// Framework dependency is needed. On the server the sender identity comes from
// the engine and is the only thing we trust; nothing in the payload identifies
// the caller.
//------------------------------------------------------------------------------

modded class PlayerBase
{
	protected bool m_MMER_WasUnconscious;

	//--------------------------------------------------------------------------

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		// super is called unconditionally. Skipping it for our own id range
		// still breaks any other mod that happens to share an id, and the
		// documented guidance is that a modded OnRPC must always chain.
		super.OnRPC(sender, rpc_type, ctx);

		if (rpc_type < MMER_RPC.FIRST || rpc_type > MMER_RPC.LAST)
			return;

		Param1<string> data = new Param1<string>("");
		if (!ctx.Read(data))
		{
			// Behind DEBUG: this runs before any identity check, so a client
			// sending malformed bodies in a loop could otherwise spam the RPT.
			if (MMER_Const.DEBUG)
				Print("[MMER] RPC " + rpc_type.ToString() + " arrived but the payload could not be read.");
			return;
		}

		if (MMER_Const.DEBUG)
		{
			int shown = Math.Min(160, data.param1.Length());
			string preview = data.param1.Substring(0, shown);
			Print("[MMER] RPC " + rpc_type.ToString() + " (" + data.param1.Length().ToString() + " bytes): " + preview);
		}

		string payload = data.param1;

		if (GetGame().IsServer())
			MMER_HandleServerRPC(sender, rpc_type, payload);
		else
			MMER_HandleClientRPC(rpc_type, payload);
	}

	//--------------------------------------------------------------------------
	// Server side
	//--------------------------------------------------------------------------

	// Last accepted inbound RPC, in engine ms, and the last archive request.
	// Per-entity rather than a server-wide map: the identity guard below means
	// this entity only ever handles its own owner's traffic.
	protected int m_MMER_LastRpcMs;
	protected int m_MMER_LastArchiveMs;

	protected void MMER_HandleServerRPC(PlayerIdentity sender, int rpc_type, string payload)
	{
		if (!sender)
			return;

		// Nothing this mod sends from a client is large. Anything bigger is a
		// client trying to make the server parse for it, and the parse used to
		// happen before any role check.
		if (payload.Length() > MMER_Const.MAX_CLIENT_PAYLOAD)
			return;

		// Rate limit before the switch, not inside each handler. Without this,
		// one client could loop any request and drive JSON parses, disk writes
		// and outbound RPC amplification as fast as it could send.
		int nowMs = GetGame().GetTime();

		if (nowMs - m_MMER_LastRpcMs < MMER_Const.RPC_MIN_INTERVAL_MS)
			return;
		m_MMER_LastRpcMs = nowMs;

		// The two handlers that cost the most per call get their own floor:
		// HELLO fans out three payloads, an archive page up to ~38 RPCs.
		bool expensive = (rpc_type == MMER_RPC.CLIENT_REQ_ARCHIVE || rpc_type == MMER_RPC.CLIENT_HELLO);
		if (expensive)
		{
			if (nowMs - m_MMER_LastArchiveMs < MMER_Const.RPC_EXPENSIVE_INTERVAL_MS)
				return;
			m_MMER_LastArchiveMs = nowMs;
		}

		// The RPC must have arrived on the sender's own player entity.
		// Rejecting the mismatch stops a client aiming a request at someone
		// else's character to act on their behalf.
		if (!GetIdentity() || GetIdentity().GetPlainId() != sender.GetPlainId())
			return;

		MMER_Manager mgr = MMER_Manager.Get();
		if (!mgr)
			return;

		MMER_IntPayload intData;
		MMER_StringPayload strData;

		switch (rpc_type)
		{
			case MMER_RPC.CLIENT_HELLO:
				mgr.SendSettings(this);
				mgr.SendState(this);
				mgr.SendCallList(this);
				break;

			case MMER_RPC.CLIENT_CALL_REQUEST:
				mgr.RequestCall(this, sender);
				break;

			case MMER_RPC.CLIENT_CALL_CANCEL:
				mgr.CancelCall(this, sender);
				break;

			case MMER_RPC.CLIENT_ACCEPT:
				intData = MMER_IntPayload.FromJson(payload);
				if (intData)
					mgr.AcceptCall(this, sender, intData.value);
				break;

			case MMER_RPC.CLIENT_COMPLETE:
				intData = MMER_IntPayload.FromJson(payload);
				if (intData)
					mgr.CompleteCall(this, sender, intData.value);
				break;

			case MMER_RPC.CLIENT_ABANDON:
				intData = MMER_IntPayload.FromJson(payload);
				if (intData)
					mgr.AbandonCall(this, sender, intData.value);
				break;

			case MMER_RPC.CLIENT_MARK:
				intData = MMER_IntPayload.FromJson(payload);
				if (intData)
					mgr.RemarkCall(this, sender, intData.value);
				break;

			case MMER_RPC.CLIENT_ADMIN_LIST:
				mgr.SendRoster(this);
				break;

			case MMER_RPC.CLIENT_ADMIN_ADD:
				strData = MMER_StringPayload.FromJson(payload);
				if (strData)
					mgr.AdminAddMember(this, sender, strData.value);
				break;

			case MMER_RPC.CLIENT_ADMIN_REMOVE:
				strData = MMER_StringPayload.FromJson(payload);
				if (strData)
					mgr.AdminRemoveMember(this, sender, strData.value);
				break;

			case MMER_RPC.CLIENT_REQ_ARCHIVE:
				intData = MMER_IntPayload.FromJson(payload);
				if (intData)
					mgr.SendArchivePage(this, sender, intData.value);
				break;
		}
	}

	//--------------------------------------------------------------------------
	// Client side - forwarded to the UI layer through the sink.
	//--------------------------------------------------------------------------

	// One reassembly slot per rpc id. A message that arrives whole is dispatched
	// immediately; a chunked one is held here until every part is in.
	protected ref map<int, ref MMER_ChunkBuffer> m_MMER_Chunks;

	protected void MMER_HandleClientRPC(int rpc_type, string raw)
	{
		string payload = raw;

		if (MMER_Chunk.IsChunk(raw))
		{
			int msg, seq, total;
			string body;

			if (!MMER_Chunk.Unwrap(raw, msg, seq, total, body))
			{
				if (MMER_Const.DEBUG)
					Print("[MMER] Dropped a malformed RPC chunk header on " + rpc_type.ToString());
				return;
			}

			if (!m_MMER_Chunks)
				m_MMER_Chunks = new map<int, ref MMER_ChunkBuffer>;

			MMER_ChunkBuffer buf;
			if (!m_MMER_Chunks.Find(rpc_type, buf) || !buf)
			{
				buf = new MMER_ChunkBuffer;
				m_MMER_Chunks.Set(rpc_type, buf);
			}

			// A new message id means the server moved on before the previous
			// one finished arriving. Start over rather than letting the
			// survivors of two payloads splice into one corrupt string -
			// comparing chunk counts alone missed the case where consecutive
			// messages happened to be the same length.
			if (buf.msgId != msg || buf.total != total)
				buf.Begin(rpc_type, msg, total);

			if (!buf.Put(seq, body))
				return;		// still waiting on other parts

			payload = buf.Join();
			buf.Begin(rpc_type, -1, 0);
		}

		MMER_ClientSink sink = MMER_ClientSink.Active();
		if (!sink)
			return;

		switch (rpc_type)
		{
			case MMER_RPC.SERVER_SETTINGS:	sink.OnSettings(payload);	break;
			case MMER_RPC.SERVER_STATE:		sink.OnState(payload);		break;
			case MMER_RPC.SERVER_CALLLIST:	sink.OnCallList(payload);	break;
			case MMER_RPC.SERVER_ROSTER:	sink.OnRoster(payload);		break;
			case MMER_RPC.SERVER_ARCHIVE:	sink.OnArchive(payload);	break;
			case MMER_RPC.SERVER_TAGS:		sink.OnTags(payload);		break;
			case MMER_RPC.SERVER_TOAST:		sink.OnToast(payload);		break;
		}
	}

	//--------------------------------------------------------------------------
	// Consciousness and death
	//--------------------------------------------------------------------------

	override void OnUnconsciousStart()
	{
		super.OnUnconsciousStart();

		m_MMER_WasUnconscious = true;

		if (GetGame().IsServer() && GetIdentity())
			MMER_Manager.Get().OnPlayerUnconscious(this);
	}

	override void OnUnconsciousStop(int pCurrentCommandID)
	{
		super.OnUnconsciousStop(pCurrentCommandID);

		m_MMER_WasUnconscious = false;

		if (GetGame().IsServer() && GetIdentity())
			MMER_Manager.Get().OnPlayerConscious(this);
	}

	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer() && GetIdentity())
			MMER_Manager.Get().OnPlayerDeath(this);

		super.EEKilled(killer);
	}

	//--------------------------------------------------------------------------
	// Client helpers used by the UI
	//--------------------------------------------------------------------------

	void MMER_SendSimple(int rpcId)
	{
		GetGame().RPCSingleParam(this, rpcId, new Param1<string>("{}"), true, null);
	}

	void MMER_SendInt(int rpcId, int value)
	{
		MMER_IntPayload p = new MMER_IntPayload;
		p.value = value;
		GetGame().RPCSingleParam(this, rpcId, new Param1<string>(p.ToJson()), true, null);
	}

	void MMER_SendString(int rpcId, string value)
	{
		MMER_StringPayload p = new MMER_StringPayload;
		p.value = value;
		GetGame().RPCSingleParam(this, rpcId, new Param1<string>(p.ToJson()), true, null);
	}
}
