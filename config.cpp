class CfgPatches
{
	class MMEmergencyResponse
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		// Add "TerjeCore","TerjeMedicine" here when you enable MMER_TERJE.
		// Add "DayZExpansion_Core" here when you enable MMER_EXPANSION.
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};

class CfgMods
{
	class MMEmergencyResponse
	{
		dir = "MMEmergencyResponse";
		picture = "";
		action = "";
		hideName = 0;
		hidePicture = 0;
		name = "MM Emergency Response";
		credits = "Misfit Mercenaries";
		author = "meccmax";
		authorID = "0";
		version = "1.4.0";
		extra = 0;
		type = "mod";
		inputs = "MMEmergencyResponse/Scripts/Data/Inputs.xml";

		dependencies[] = {"Game", "World", "Mission"};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"MMEmergencyResponse/Scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"MMEmergencyResponse/Scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"MMEmergencyResponse/Scripts/5_Mission"};
			};
		};
	};
};

//------------------------------------------------------------------------------
// Optional notification sounds.
//
// Drop two mono .ogg files into GUI/sounds/ and uncomment this block to get
// audible dispatch cues. Until then, "soundIncoming"/"soundFatal" in
// config.json resolve to nothing and the mod runs silently - which is a valid
// configuration, not an error.
//------------------------------------------------------------------------------
/*
class CfgSoundShaders
{
	class MMER_Incoming_SoundShader
	{
		samples[] = {{"MMEmergencyResponse\\GUI\\sounds\\incoming", 1}};
		volume = 1.0;
		range = 10;
	};
	class MMER_Fatal_SoundShader
	{
		samples[] = {{"MMEmergencyResponse\\GUI\\sounds\\fatal", 1}};
		volume = 1.0;
		range = 10;
	};
};

class CfgSoundSets
{
	class MMER_Incoming_SoundSet
	{
		soundShaders[] = {"MMER_Incoming_SoundShader"};
		volumeFactor = 1.0;
		spatial = 0;
		doppler = 0;
		loop = 0;
	};
	class MMER_Fatal_SoundSet
	{
		soundShaders[] = {"MMER_Fatal_SoundShader"};
		volumeFactor = 1.0;
		spatial = 0;
		doppler = 0;
		loop = 0;
	};
};
*/
