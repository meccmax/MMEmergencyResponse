//------------------------------------------------------------------------------
// MM Emergency Response - builds the patient readout the responder sees.
//
// Rows are driven entirely by the "diagnostics" array in config.json, so the
// operator can add, remove or relabel readings without touching script. Ids are
// namespaced:
//     vanilla:<name>   resolved below, always available
//     terje:<statId>   forwarded to MMER_TerjeAdapter
// Unknown ids are reported as unavailable rather than dropped, so a typo in the
// config is visible instead of silent.
//------------------------------------------------------------------------------

class MMER_Diagnostics
{
	static void Build(PlayerBase player, MMER_Settings settings, out array<ref MMER_DiagValue> outRows)
	{
		if (!outRows)
			outRows = new array<ref MMER_DiagValue>;
		outRows.Clear();

		if (!player || !settings || !settings.diagnostics)
			return;

		for (int i = 0; i < settings.diagnostics.Count(); i++)
		{
			MMER_DiagRow def = settings.diagnostics.Get(i);
			if (!def || def.id == "")
				continue;

			float raw;
			bool ok = Resolve(player, def.id, raw);

			MMER_DiagValue val = new MMER_DiagValue;
			val.label = def.label;

			if (!ok)
			{
				val.value	= "--";
				val.flag	= 2;
				outRows.Insert(val);
				continue;
			}

			float shown = raw * def.scale;
			val.value	= Format(shown, def.decimals) + def.unit;
			val.flag	= 0;

			if (def.warnAbove >= 0 && shown > def.warnAbove)
				val.flag = 1;
			if (def.warnBelow >= 0 && shown < def.warnBelow)
				val.flag = 1;

			outRows.Insert(val);
		}
	}

	//--------------------------------------------------------------------------

	static bool Resolve(PlayerBase player, string id, out float value)
	{
		value = 0;

		if (id.IndexOf("terje:") == 0)
		{
			string statId = id.Substring(6, id.Length() - 6);
			return MMER_TerjeAdapter.GetStat(player, statId, value);
		}

		if (id.IndexOf("vanilla:") != 0)
			return false;

		string key = id.Substring(8, id.Length() - 8);

		// An if/else chain rather than a switch: Enforce's support for
		// switching on strings varies by branch, and this is not hot code.
		if (key == "blood")
		{
			value = player.GetHealth("", "Blood");
			return true;
		}

		if (key == "health")
		{
			value = player.GetHealth("", "Health");
			return true;
		}

		if (key == "shock")
		{
			value = player.GetHealth("", "Shock");
			return true;
		}

		if (key == "bleeding")
		{
			value = CountBits(player.GetBleedingBits());
			return true;
		}

		if (key == "energy")
		{
			if (!player.GetStatEnergy())
				return false;

			value = player.GetStatEnergy().Get();
			return true;
		}

		if (key == "water")
		{
			if (!player.GetStatWater())
				return false;

			value = player.GetStatWater().Get();
			return true;
		}

		if (key == "temperature" || key == "heatcomfort")
		{
			if (!player.GetStatHeatComfort())
				return false;

			value = player.GetStatHeatComfort().Get();
			return true;
		}

		if (key == "unconscious")
		{
			if (player.IsUnconscious())
				value = 1;
			return true;
		}

		if (key == "alive")
		{
			if (player.IsAlive())
				value = 1;
			return true;
		}

		return false;
	}

	//--------------------------------------------------------------------------

	static int CountBits(int bits)
	{
		int n = 0;
		for (int i = 0; i < 32; i++)
		{
			if ((bits & (1 << i)) != 0)
				n++;
		}
		return n;
	}

	static string Format(float v, int decimals)
	{
		if (decimals <= 0)
			return Math.Round(v).ToString();

		float mult = Math.Pow(10, decimals);
		float rounded = Math.Round(v * mult) / mult;
		string txt = rounded.ToString();

		// EnScript float-to-string is inconsistent about trailing digits;
		// trim to the requested precision so columns line up.
		int dot = txt.IndexOf(".");
		if (dot < 0)
			return txt + "." + Zeros(decimals);

		int want = dot + 1 + decimals;
		if (txt.Length() > want)
			return txt.Substring(0, want);

		while (txt.Length() < want)
			txt += "0";

		return txt;
	}

	static string Zeros(int n)
	{
		string t = "";
		for (int i = 0; i < n; i++)
			t += "0";
		return t;
	}
}
