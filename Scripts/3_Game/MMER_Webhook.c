//------------------------------------------------------------------------------
// MM Emergency Response - Discord webhook posting via the engine REST API.
// Fire-and-forget: a failed webhook must never affect gameplay.
//
// The request body is built with JsonSerializer rather than string
// concatenation. Enforce's parser rejects stacked backslash escapes (an
// escaped backslash followed by an escaped quote), and hand-rolled JSON
// escaping of attacker-controlled player names is exactly the kind of thing
// you get subtly wrong once and never notice. Letting the engine write the
// JSON solves both at once.
//------------------------------------------------------------------------------

class MMER_DiscordFooter
{
	string text = "";
}

class MMER_DiscordEmbed
{
	string	title		= "";
	string	description	= "";
	int		color		= 0;
	ref MMER_DiscordFooter footer;
}

//------------------------------------------------------------------------------
// An empty "parse" array tells Discord to resolve NO mentions in this message.
// Without it, a player whose Steam persona is "@everyone" pings the whole
// server the moment they press CALL FOR HELP: embed descriptions do render
// mentions, and the name goes straight into the description. JSON-escaping the
// name does not help - the string is syntactically fine, it is Discord's own
// content layer that acts on it.
//------------------------------------------------------------------------------
class MMER_AllowedMentions
{
	ref TStringArray parse;

	void MMER_AllowedMentions()
	{
		parse = new TStringArray;
	}
}

class MMER_DiscordPayload
{
	string	username	= "";
	ref MMER_AllowedMentions allowed_mentions;
	ref array<ref MMER_DiscordEmbed> embeds;

	void MMER_DiscordPayload()
	{
		embeds = new array<ref MMER_DiscordEmbed>;
		allowed_mentions = new MMER_AllowedMentions;
	}
	string ToJson()
	{
		string json;
		string err;
		if (!JsonFileLoader<MMER_DiscordPayload>.MakeData(this, json, err, false))
			return "";
		return json;
	}

}

//------------------------------------------------------------------------------

class MMER_RestCallback extends RestCallback
{
	override void OnSuccess(string data, int dataSize)
	{
		MMER_Log.Debug("webhook ok");
	}

	override void OnError(int errorCode)
	{
		MMER_Log.Warn("Discord webhook failed: " + Explain(errorCode));
	}

	// errorCode is an ERestResultState, not an HTTP status.
	static string Explain(int code)
	{
		switch (code)
		{
			case 5:		// EREST_ERROR / EREST_ERROR_CLIENTERROR
				return "client error (HTTP 4xx) - Discord refused the request. Usually a wrong webhook URL, a deleted webhook, or a malformed body.";
			case 6:		// EREST_ERROR_SERVERERROR
				return "server error (HTTP 5xx) - Discord had a problem. Transient; it should recover on its own.";
			case 7:		// EREST_ERROR_APPERROR
				return "application error inside the REST layer.";
			case 8:		// EREST_ERROR_TIMEOUT
				return "timed out - the server could not reach discord.com. Check outbound HTTPS from the host.";
			case 9:
				return "not implemented.";
		}
		return "unknown REST state " + code.ToString();
	}

	override void OnTimeout()
	{
		MMER_Log.Warn("webhook timed out");
	}
}

//------------------------------------------------------------------------------

class MMER_Webhook
{
	static ref MMER_RestCallback	s_Callback;

	// NOT a ref: RestContext is owned by the RestApi and has a private
	// destructor, so a ref makes the compiler try to generate destruction code
	// it is not allowed to call. We only borrow the pointer.
	static RestContext				s_Context;

	static string					s_Path;			// "<id>/<token>"
	static bool						s_Ready;
	static bool						s_Tried;

	// Outbound rate budget. Discord starts 429ing a webhook around 5/s and will
	// disable one that keeps hammering, and each failure costs a log write.
	static const int				MAX_POSTS_PER_SECOND = 4;
	static int						s_BudgetWindow;
	static int						s_BudgetUsed;
	static int						s_Dropped;

	//--------------------------------------------------------------------------
	// Splits "https://discord.com/api/webhooks/<id>/<token>" into a base the
	// RestContext can hold and the path we POST to.
	//--------------------------------------------------------------------------
	static void Init(string url, bool testOnStart, string username)
	{
		s_Ready	= false;
		s_Tried	= true;

		if (url == "")
			return;

		string marker = "/api/webhooks/";
		int idx = url.IndexOf(marker);
		if (idx < 0)
		{
			MMER_Log.Warn("discordWebhookUrl does not look like a Discord webhook URL - webhooks disabled.");
			return;
		}

		int cut = idx + marker.Length();
		string baseUrl = url.Substring(0, cut);
		s_Path = url.Substring(cut, url.Length() - cut);

		RestApi api = GetRestApi();
		if (!api)
			api = CreateRestApi();

		if (!api)
		{
			MMER_Log.Warn("REST API unavailable - webhooks disabled.");
			return;
		}

		s_Context = api.GetRestContext(baseUrl);
		if (!s_Context)
		{
			MMER_Log.Warn("Could not create REST context - webhooks disabled.");
			return;
		}

		// SetHeader sets the Content-Type VALUE only - not a full header line.
		// From the engine docs: "default content type is application/octet-stream
		// but you can specify whatever you like, for example application/json".
		// Passing "Content-Type: application/json" produced a malformed header
		// and Discord rejected every POST.
		s_Context.SetHeader("application/json");
		s_Callback	= new MMER_RestCallback;
		s_Ready		= true;

		MMER_Log.Info("Discord webhook armed.");

		if (testOnStart)
		{
			Post("Dispatch online",
				"MM Emergency Response started and the webhook is configured correctly.",
				0x4B9BE0, username);
		}
	}

	// Defence in depth alongside allowed_mentions: strips the characters Discord
	// acts on in message content. Player names reach here verbatim, so a name
	// can otherwise carry a mention, a masked link to a phishing page, or
	// backticks that break how the channel reads. Newlines are stripped from
	// names at the call site, not here, because the bodies use them for layout.
	static string Defang(string s)
	{
		string clean = "";
		for (int i = 0; i < s.Length(); i++)
		{
			string c = s.Get(i);

			if (c == "@")		{ clean = clean + "(at)"; continue; }
			if (c == "`")		{ clean = clean + "'"; continue; }
			if (c == "|")		{ clean = clean + "/"; continue; }
			if (c == "[")		{ clean = clean + "("; continue; }
			if (c == "]")		{ clean = clean + ")"; continue; }

			clean = clean + c;
		}
		return clean;
	}

	// Names and other attacker-controlled fragments go through this before they
	// are formatted into a webhook body. Also clamps length so one absurd name
	// cannot push a whole embed over Discord's limits.
	static string SafeName(string s)
	{
		string clean = "";
		for (int i = 0; i < s.Length(); i++)
		{
			string c = s.Get(i);

			// Forged lines in the Discord embed and in emergency.log both start
			// with a newline in a name.
			if (c == "\n" || c == "\r" || c == "\t")
				continue;

			if (c == "*" || c == "_" || c == "~" || c == "#" || c == ">")
				continue;

			clean = clean + c;
		}

		clean = MMER_Webhook.Defang(clean);

		if (clean.Length() > 48)
			clean = clean.Substring(0, 48);

		if (clean == "")
			clean = "(unnamed)";

		return clean;
	}

	static void Post(string title, string description, int color, string username)
	{
		if (!s_Tried || !s_Ready || !s_Context)
			return;

		// A responder looping accept/abandon, or a name-flood, could otherwise
		// drive unbounded POSTs: Discord rate-limits the webhook, disables it,
		// and every failure logs a line to disk. Budget them.
		int now = MMER_Time.NowUnix();
		if (now != s_BudgetWindow)
		{
			s_BudgetWindow = now;
			s_BudgetUsed = 0;
		}

		if (s_BudgetUsed >= MAX_POSTS_PER_SECOND)
		{
			s_Dropped++;
			return;
		}
		s_BudgetUsed++;

		if (s_Dropped > 0)
		{
			MMER_Log.Warn(string.Format("%1 webhook post(s) dropped by the rate budget.", s_Dropped));
			s_Dropped = 0;
		}

		MMER_DiscordFooter footer = new MMER_DiscordFooter;
		footer.text = MMER_Time.StampUTC();

		MMER_DiscordEmbed embed = new MMER_DiscordEmbed;
		embed.title			= MMER_Webhook.Defang(title);
		embed.description	= MMER_Webhook.Defang(description);
		embed.color			= color;
		embed.footer		= footer;

		MMER_DiscordPayload payload = new MMER_DiscordPayload;
		payload.username = MMER_Webhook.Defang(username);
		payload.embeds.Insert(embed);

		s_Context.POST(s_Callback, s_Path, payload.ToJson());
	}
}
