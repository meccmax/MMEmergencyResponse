# Steam Workshop listing

Paste-ready copy for the Workshop description. Steam's editor accepts BBCode,
so the tags below are intentional. Trim to taste.

---

[h1]MM Emergency Response[/h1]

A MEDEVAC dispatch system for DayZ. A downed player calls for help, your
rostered response team gets the call, one of them claims it, works the patient,
and closes the case.

No whitelist. No IP lock. No phone-home. Configure it and it is yours.

[hr][/hr]

[h2]For the patient[/h2]

[list]
[*] An overlay appears while you are unconscious, with your server's logo and a
    [b]CALL FOR HELP[/b] button.
[*] Once a responder claims your case, the overlay shows their name.
[*] Cancel your own call any time.
[*] Respawn is softly held while a responder is actually en route — with a
    ceiling, so nobody is ever stuck.
[/list]

[h2]For the responder[/h2]

[list]
[*] A hotkey (default [b]K[/b]) opens the dispatch panel. Rostered players only.
[*] Live queue: call id, patient, grid reference, state, time elapsed, colour
    coded.
[*] Detail pane: grid, range, bearing and altitude difference from where you
    are standing.
[*] A vitals readout that refreshes every second while the case is open —
    blood, health, shock, bleed sources, energy, water, body temperature, and
    optional Terje Medicine rows.
[*] Accept, Complete, Release, Re-mark.
[*] A paged archive of every closed case.
[/list]

[h2]For the admin[/h2]

[list]
[*] Roster management in-game: add a responder by name or Steam64, remove them
    again, see who is online.
[*] Discord webhook reporting — new call, accepted, completed, patient lost,
    cancelled. Each one configurable.
[*] Optional chat tags so your medics are recognisable in chat.
[*] Automatic pre-restart stabilisation, so nobody is left bleeding out through
    a scheduled restart.
[*] Everything in one [b]config.json[/b], written on first boot with sane
    defaults.
[/list]

[hr][/hr]

[h2]Setup[/h2]

[olist]
[*] Add the mod to your server's [b]-mod=[/b] list. Clients need it too.
[*] Start the server once. It writes
    [b]profiles/MMEmergency/config.json[/b].
[*] Put your Steam64 in [b]adminIds[/b] and paste a Discord webhook URL into
    [b]discordWebhookUrl[/b] if you want reporting.
[*] Restart. Press [b]K[/b] in game.
[/olist]

Full documentation, every config key explained, on GitHub.

[h2]Compatibility[/h2]

[list]
[*] Requires nothing. No Community Framework, no Expansion.
[*] Terje Medicine diagnostics are optional and off unless you build with them.
[*] Modifies [b]PlayerBase[/b], [b]MissionServer[/b], [b]MissionGameplay[/b],
    [b]InGameMenu[/b] and [b]ChatLine[/b] via [b]modded class[/b], so it chains
    cleanly with other mods rather than replacing them.
[/list]

[h2]Licence[/h2]

MIT. Use it, change it, ship it in your own server pack — just keep the
copyright notice. Source on GitHub.

Every line is original work written against the DayZ Enforce Script API and the
vanilla game scripts. No third-party mod was decompiled, unpacked, or borrowed
from.
