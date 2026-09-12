//------------------------------------------------------------------------------
// MM Emergency Response - the beacon a patient spends to call for help.
//
// Server side only. Every function here reads or writes a player's real
// inventory, so none of it may ever run from a client's assertion that it has
// something: the caller is always the manager, acting on the entity the RPC
// guard already matched to the sender.
//------------------------------------------------------------------------------

class MMER_Items
{
	// The first item in the player's inventory whose class name is in `types`.
	// Null when they are carrying none of them.
	//
	// PREORDER walks attachments and cargo as well as hands, so a flare in a
	// backpack counts - which is what a player would expect, and refusing it
	// would look like a bug.
	static EntityAI FindFirst(PlayerBase player, TStringArray types)
	{
		if (!player || !types || types.Count() == 0)
			return null;

		array<EntityAI> items = new array<EntityAI>;
		if (!player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items))
			return null;

		for (int i = 0; i < items.Count(); i++)
		{
			EntityAI item = items.Get(i);
			if (!item)
				continue;

			if (types.Find(item.GetType()) > -1)
				return item;
		}

		return null;
	}

	static bool Has(PlayerBase player, TStringArray types)
	{
		EntityAI found = MMER_Items.FindFirst(player, types);
		if (found)
			return true;
		return false;
	}

	// Does this player carry a radio that satisfies the responder requirement?
	//
	// `reason` comes back filled in on failure and is meant to be shown to the
	// player verbatim. A responder who cannot accept a case must be told WHY -
	// silently refusing and letting them wonder is the failure mode that makes
	// a mechanic like this feel broken rather than demanding. They are conscious
	// and can act on the answer, which is exactly why a frequency requirement is
	// reasonable here and would not be on the patient's side.
	//
	// Every matching radio is checked, not just the first: carrying a spare
	// tuned correctly should count.
	static bool HasTunedRadio(PlayerBase player, TStringArray types, float wantFreq, bool mustBeOn, string label, out string reason)
	{
		reason = "";

		if (!player || !types || types.Count() == 0)
			return true;		// nothing configured to require

		array<EntityAI> items = new array<EntityAI>;
		if (!player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items))
		{
			reason = string.Format("You need a %1 on you to take a case.", label);
			return false;
		}

		bool sawRadio = false;
		bool sawPowered = false;
		float nearestFreq = 0;

		for (int i = 0; i < items.Count(); i++)
		{
			EntityAI item = items.Get(i);
			if (!item || types.Find(item.GetType()) < 0)
				continue;

			sawRadio = true;

			// IsWorking() rather than IsSwitchedOn(): a radio flicked on with a
			// dead battery is off as far as anyone using it is concerned.
			if (mustBeOn)
			{
				ComponentEnergyManager em = item.GetCompEM();
				if (!em || !em.IsWorking())
					continue;
			}

			sawPowered = true;

			if (wantFreq <= 0)
				return true;		// any frequency accepted

			ItemTransmitter radio = ItemTransmitter.Cast(item);
			if (!radio)
				continue;

			float freq = radio.GetTunedFrequency();
			nearestFreq = freq;

			// Tolerance, not equality: these are floats off a stepped dial.
			if (Math.AbsFloat(freq - wantFreq) < 0.05)
				return true;
		}

		if (!sawRadio)
		{
			reason = string.Format("You need a %1 on you to take a case.", label);
			return false;
		}

		if (!sawPowered)
		{
			reason = string.Format("Your %1 is off or out of battery.", label);
			return false;
		}

		reason = string.Format("Your %1 is on %2 - dispatch monitors %3.",
			label, nearestFreq.ToString(), wantFreq.ToString());
		return false;
	}

	// Spends exactly one.
	//
	// The test is IsSplitable(), NOT HasQuantity(). DayZ overloads "quantity"
	// for two unrelated things: on genuinely stackable items (ammo, nails) it is
	// a COUNT, but on plenty of others it is a RESOURCE - a Roadflare's quantity
	// is its remaining burn time, a canteen's is millilitres, a fuel can's is
	// litres. Keying off HasQuantity() meant a flare lost one second of burn and
	// stayed in the player's hands instead of being spent.
	//
	// IsSplitable() reads the item's own canBeSplit config flag (ItemBase.c:243),
	// which is the only reliable way to tell the two apart.
	static bool ConsumeOne(EntityAI item)
	{
		if (!item)
			return false;

		ItemBase ib = ItemBase.Cast(item);
		if (ib && ib.IsSplitable() && ib.GetQuantity() > 1)
		{
			// A true stack: take one unit, leave the rest.
			ib.AddQuantity(-1);
			return true;
		}

		// Everything else - a flare, a radio, a single item off a stack - goes
		// whole. Half a beacon is not a thing.
		GetGame().ObjectDelete(item);
		return true;
	}

	// Hands one back. Into the inventory if there is room, otherwise onto the
	// ground at their feet - a refund that silently evaporates because the
	// player is full is worse than no refund at all, because they never learn
	// it happened.
	static bool GiveBack(PlayerBase player, string type)
	{
		if (!player || type == "")
			return false;

		// PlayerBase's own overload rather than GameInventory's: it runs
		// FindFirstFreeLocationForNewEntity first and returns null when there is
		// genuinely no room, which is the case we need to detect.
		EntityAI intoInventory = player.CreateInInventory(type);
		if (intoInventory)
			return true;

		Object onGround = GetGame().CreateObjectEx(type, player.GetPosition(), ECE_PLACE_ON_SURFACE);
		if (onGround)
			return true;

		return false;
	}
}
