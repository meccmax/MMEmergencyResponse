# Steam Workshop listing

Paste-ready copy for https://steamcommunity.com/sharedfiles/filedetails/?id=3798953316

Steam's description editor accepts BBCode, so the tags below are intentional —
paste the block between the rules verbatim. Trim any section you don't want.

---

[h1]MM Emergency Response[/h1]

[b]A MEDEVAC dispatch system for DayZ.[/b] A downed player calls for help. Your
rostered response team gets the call with the patient's grid reference and live
vitals. One of them claims it, works the patient, and closes the case.

No whitelist. No IP lock. No phone-home. Configure it and it's yours.

[hr][/hr]

[h1]For the patient[/h1]

[list]
[*] An overlay appears while you're unconscious, with your server's logo and a
    [b]CALL FOR HELP[/b] button.
[*] Once a responder claims your case, the overlay shows their name.
[*] Cancel your own call any time.
[*] Respawn is softly held while a responder is actually en route — with a
    ceiling that can't be configured away, so nobody is ever stuck.
[*] [b]Optional:[/b] require a distress beacon to call at all. It's spent on the
    call and handed back if nobody answers.
[/list]

[h1]For the responder[/h1]

[list]
[*] A hotkey (default [b]K[/b]) opens the dispatch panel. Rostered players only.
[*] Live queue: call ID, patient, grid reference, state, time elapsed — colour
    coded so an unclaimed emergency is obvious at a glance.
[*] Detail pane: grid, range, bearing and altitude difference from exactly where
    you're standing.
[*] A vitals readout refreshed every second while the case is open — blood,
    health, shock, bleed sources, energy, water and body temperature, alongside
    Terje Medicine rows for sepsis, pain, painkillers, antibiotics, hematoma,
    contusion, influenza, Z-virus and radiation. Rows are data-driven: reorder,
    relabel, add and remove them in [b]config.json[/b], no script edit.
[*] Accept, Complete, Release, Re-mark.
[*] A paged archive of every closed case.
[*] [b]Optional:[/b] require a working radio, and optionally one tuned to your
    dispatch frequency, before a medic can take a case.
[/list]

[h1]For the admin[/h1]

[list]
[*] Roster management in-game: add a responder by name or Steam64, remove them
    again, see who's online and what access they have.
[*] Discord webhook reporting — new call, accepted, completed, patient lost,
    cancelled. Every event individually switchable.
[*] Chat tags so your medics are recognisable in chat. Works with DayZ Expansion
    Chat as well as vanilla.
[*] Automatic pre-restart stabilisation, so nobody bleeds out through a
    scheduled restart.
[*] Everything in one [b]config.json[/b], written with sane defaults on first
    boot.
[/list]

[hr][/hr]

[h1]Setup[/h1]

[olist]
[*] Add the mod to your server's [b]-mod=[/b] list. Clients need it too — this
    mod has UI.
[*] Start the server once. It writes
    [b]profiles/MMEmergency/config.json[/b], then stop it.
[*] Put your Steam64 in [b]adminIds[/b], your medics in [b]teamIds[/b], and set
    [b]restartTimesUTC[/b] to match your actual restart schedule.
[*] Paste a Discord webhook URL into [b]discordWebhookUrl[/b] if you want
    reporting.
[*] Restart. Press [b]K[/b] in game.
[/olist]

[b]Load order matters.[/b] Put this mod [b]after[/b] Expansion and Terje in your
[b]-mod=[/b] list, so their classes are loaded before this one extends them.

[h1]Optional gates[/h1]

Both ship [b]off[/b], so installing this never changes who can call. Turn them
on when you want them:

[list]
[*] [b]requireCallItem[/b] — the patient needs a beacon to call. It's consumed,
    and refunded if the call expires unanswered or they cancel within 30
    seconds. Burning a scarce item for no rescue is the worst outcome a system
    like this can produce, so it doesn't happen.
[*] [b]requireItemForResponder[/b] — medics need a working radio to accept.
    Theirs is never consumed. Add [b]responderFrequency[/b] and they'll need it
    tuned to your dispatch channel, with every refusal telling them exactly
    what's wrong.
[/list]

Point either at any item you like — vanilla or your own.

[h1]Requirements[/h1]

[b]This Workshop build requires the following. Load them before this mod.[/b]

[list]
[*] [b]DayZ Expansion[/b] — the Chat module specifically. This build draws the
    responder chat tag through Expansion's chat renderer, which replaces the
    vanilla one entirely.
[*] [b]Terje Core[/b] and [b]Terje Medicine[/b] — the diagnostics readout shows
    sepsis, pain, painkillers, antibiotics, radiation, hematoma, contusion,
    influenza and Z-virus alongside the vanilla vitals.
[*] [b]Clients need this mod too.[/b] It has UI, so it goes in your client mod
    list as well as the server's.
[/list]

[b]No Community Framework required.[/b]

[h1]Compatibility[/h1]

[list]
[*] Extends [b]PlayerBase[/b], [b]MissionServer[/b], [b]MissionGameplay[/b],
    [b]InGameMenu[/b] and the chat line via [b]modded class[/b], so it chains
    cleanly with other mods rather than replacing them.
[*] Tested live on Deer Isle and Sakhal alongside DayZ Expansion and Terje.
[*] Building from source? Both integrations are compile-time flags, each paired
    with its entries in [b]requiredAddons[/b]. Turn both off and the mod runs
    with [b]no dependencies at all[/b] — the Terje rows read "--" and the chat
    tag uses vanilla chat. See the GitHub README.
[*] The server states on every boot which optional gates are live and whether
    the Terje readout is on, so "is the feature even switched on" is never a
    guess.
[/list]

[h1]Built to be trusted on a live server[/h1]

[list]
[*] [b]Server-authoritative throughout.[/b] Every action re-derives who you are
    from the engine, never from anything the client sends. A modified client
    can't claim to be someone else, can't grant itself responder access, and
    can't act on another player's behalf.
[*] [b]Audited.[/b] A full adversarial security pass covering trust boundaries,
    data leakage, injection, resource exhaustion and logic flaws. The findings
    and the threat model are public in SECURITY.md.
[*] [b]No player Steam64s are sent to other players.[/b]
[*] [b]A corrupt config is never overwritten.[/b] Fix the file and restart —
    your admin list and webhook URL survive.
[/list]

[h1]Licence[/h1]

MIT. Use it, change it, ship it in your own server pack — just keep the
copyright notice. Source and full documentation on GitHub:
[url=https://github.com/meccmax/MMEmergencyResponse]github.com/meccmax/MMEmergencyResponse[/url]

Every line is original work written against the DayZ Enforce Script API and the
vanilla game scripts. No third-party mod was decompiled, unpacked, or borrowed
from.

[h1]Support[/h1]

Bug reports and feature requests on GitHub Issues. Attaching
[b]profiles/MMEmergency/emergency.log[/b] and your [b]config.json[/b] (with the
webhook URL redacted) gets things fixed fastest.

---

## Notes for you, not for Steam

- The Workshop listing needs screenshots to look right. The dispatch panel with
  a live call in the queue, the unconscious overlay with the CALL FOR HELP
  button, and a Discord embed are the three that sell it.
- Steam shows roughly the first two lines before "Read more", so the opening
  sentence is doing the work.
- If you'd rather lead with the beacon/radio mechanics than the core dispatch
  loop, move the "Optional gates" section directly under the intro.
