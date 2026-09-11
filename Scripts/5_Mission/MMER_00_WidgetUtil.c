//------------------------------------------------------------------------------
// MM Emergency Response - small widget helpers shared by the menus.
//------------------------------------------------------------------------------

class MMER_WidgetUtil
{
	// Walks siblings before unlinking, so the list can't be mutated out from
	// under the iterator.
	static void ClearChildren(Widget holder)
	{
		if (!holder)
			return;

		Widget child = holder.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.Unlink();
			child = next;
		}
	}

	// SetText resolves "#KEY" against the stringtable only when the key is the
	// entire string, so text that interpolates a runtime value has to be
	// translated up front.
	//
	// This is Widget.TranslateString - the QUALIFIED static. An earlier version
	// of this file called a bare TranslateString() on the belief that it was a
	// global; it is not, and the failure surfaces as an undefined method on
	// whatever class the call sits in. The qualified form is the documented one
	// and compiles.
	static string Tr(string key)
	{
		if (key == "" || key.Get(0) != "#")
			return key;

		return Widget.TranslateString(key);
	}
}
