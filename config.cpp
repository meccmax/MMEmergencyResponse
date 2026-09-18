class CfgPatches
{
	class MMEmergencyResponse
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		// Every entry here is a hard load-order dependency, and each one is
		// paired with a build flag. Turning a flag off WITHOUT removing its
		// addons leaves a dependency the mod no longer needs; turning a flag
		// on WITHOUT adding them fails at build time with "unknown type",
		// which is the intended loud failure.
		//
		//   TerjeCore, TerjeMedicine  -> MMER_TERJE
		//                                (Scripts/4_World/MMER_00_Defines.c)
		//   DayZExpansion_Chat_Scripts -> MMER_EXPANSION_CHAT
		//                                (Scripts/5_Mission/MMER_00_MissionDefines.c)
		//   DayZExpansion_Navigation_Scripts -> MMER_EXPANSION (map markers)
		//
		// Both flags are ON in the Steam Workshop build. A source build that
		// wants no dependencies turns both off and trims this list back to
		// {"DZ_Data", "DZ_Scripts"}.
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "TerjeCore", "TerjeMedicine", "DayZExpansion_Chat_Scripts", "DayZExpansion_Navigation_Scripts"};
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
		version = "1.8.3";
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
