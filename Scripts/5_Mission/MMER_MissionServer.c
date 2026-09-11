//------------------------------------------------------------------------------
// MM Emergency Response - server mission hooks.
//------------------------------------------------------------------------------

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		MMER_Manager.Get().Init();
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		MMER_Manager.Get().OnUpdate();
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);
		MMER_Manager.Get().OnPlayerConnected(player);
	}

	override void InvokeOnDisconnect(PlayerBase player)
	{
		MMER_Manager.Get().OnPlayerDisconnected(player);
		super.InvokeOnDisconnect(player);
	}
}
