# Changelog — MM Emergency Response

---

## 1.8.3 — 2026-09-18

### Added

- **The whole downed-player overlay is now config.** 28 keys under
  `overlayText` in `config.json` — title, status line, three button captions,
  and every option and hint line in all four states.

  **An empty string means "use the built-in."** That is the design, not a
  shortcut: an existing `config.json` has none of these keys, reads them all as
  empty, and behaves exactly as it did before. Override the one line you care
  about and the other twenty-seven keep tracking the defaults, including when a
  later version improves them.

- **Six tokens**: `{beacon}`, `{medic}`, `{cooldown}`, `{hold}`, `{cancel}` and
  `{team}`. An unknown token is left on screen as typed rather than blanked, so
  a typo shows up as `{beacn}` instead of quietly eating a word.

  `string.Replace` mutates in place and returns an int — the same trap as
  `ToLower`, which this mod has been bitten by before. Substitution works on a
  local copy so the operator's configured string survives to the next frame.

### Changed

- **The built-in status lines now resolve their stringtable key first and then
  append a token**, rather than being rewritten as literals. Enforce only
  translates a string that is *entirely* a `#KEY`, so `"#KEY {medic}"` would
  resolve to nothing. Doing it in this order keeps the default wording
  localisable while still letting the token work.

- **`{hold}` is the duration alone** ("15 min"), not " for up to 15 min". The
  surrounding words belong to whoever writes the line. It also mirrors the
  server's own floor of 900 seconds rather than reporting a ceiling the server
  will not honour.

- **The en-route hint only appears when a beacon was genuinely spent.** It read
  "Your beacon is spent" on servers with no beacon requirement at all.

### Note

Which *variant* of a line appears is still decided by the server's real
settings, never by which keys you filled in. `idle2Cost` shows only when a
beacon is both required and consumed; the refund lines only when a refund
actually happens. Writing a refund line on a server that does not refund does
not conjure one — same rule as 1.8.1.

---
## 1.8.2 — 2026-09-18

### Fixed

- **The overlay text was cut off.** DayZ text does **not** shrink to fit its
  widget — it **clips**, which 1.8.1 assumed the opposite of. That is why the
  panel showed "your distr" and lost the rest off the right edge.

  Three changes, because one alone would not be safe:
  - The layout now pins a size with `"exact text" 1` + `"exact text size"`
    rather than letting the engine derive it from the box height. A panel that
    gets scaled no longer scales its font out of range.
  - The option lines, status and hint are `MultilineTextWidgetClass` with
    `wrap 1`, so a line too long for the width goes to a second line instead of
    being sliced off.
  - `FitText()` measures the rendered text with `GetTextSize()` against the
    widget's real screen size and steps the point size down until it fits, in
    **both** directions — wrapping turns an overflow from horizontal to
    vertical, and a clipped second line is no better than a clipped sentence.
    It bottoms out at 11px rather than looping forever.

  The panel is also wider (0.34 → 0.42) and the copy is shorter. Those help;
  they are not the fix.

- **An Expansion server marker could never be removed.** `CreateServerMarker`
  with an empty uid generates its own internally — `name + RandomInt` — and does
  not hand it back. The adapter stored the *name* and called
  `RemoveServerMarker(name)`, which never matched anything, so a marker in
  `markerMode: 2` stayed on every player's map for the rest of the server's
  uptime. It now passes an explicit uid.

### Added

- **`markerMode: 3` — a precise marker for responders only.**

  The existing mode 2 is an Expansion **server** marker, and those are global:
  every player on the server sees them. For a MEDEVAC that is a public
  announcement that somebody is lying helpless at a known grid — the same
  disclosure `rosterMode: 2` exists to prevent.

  Mode 3 sends a **personal** Expansion marker to each responder individually
  and to nobody else. It follows the pattern Expansion uses for its own death
  marker: the server RPCs one client, and that client builds the marker
  locally. Placed when the call opens, refreshed on re-mark, cleared when the
  case closes.

  Two rules it obeys:
  - **Never sent to a redacted viewer** under `rosterMode: 2` unless they have
    accepted the case. A marker *is* the position, so sending one would undo
    that mode completely.
  - **Cleared, not filtered, on the way out.** The clear goes to everyone
    online, not just current responders — somebody who stowed their radio or
    left the roster still has the pin, and a stale marker pointing at a closed
    case is worse than a redundant packet.

  Markers are non-persistent by design. Expansion defaults personal markers to
  persistent, which would bring a pin back on every login for a case that ended
  hours ago while the player was offline.

- **`marker3D`** (default `0`). Draws the marker in the world as well as on the
  map. Off, because a pin floating over a downed player is visible to anyone
  looking that way, which quietly undoes the point of mode 3.

- **`Markers:` in the boot summary**, as a **warning** rather than an info line
  when the mode publishes a position globally or when the build cannot honour
  the mode at all.

### Changed

- **`MMER_EXPANSION` is on**, with `DayZExpansion_Navigation_Scripts` in
  `requiredAddons[]` — that is the addon holding `ExpansionMarkerModule`, not
  `DayZExpansion_Core` as the old comment claimed. `markerMode` still defaults
  to `0`, so the flag compiles the code in and changes nothing else.

---
## 1.8.1 — 2026-09-13

**The downed-player overlay now says what your options are.**

Players were confused by the unconscious screen, and looking at it that is not
surprising: it showed a title, a one-line status, and a button. It never said
that waiting and respawning were also choices, what pressing the button costs,
or whether you get the beacon back. A player who does not know those things
reads the whole system as "a button that might do something".

### Added

- **Three numbered options on the overlay**, rewritten per state:
  - **Down, able to call** — wait / call (naming the cost) / respawn, plus a
    line saying the beacon comes back if you come round first.
  - **Call out, unclaimed** — nobody has taken it yet, the self-recovery refund,
    and that you can still respawn or cancel. The quick-cancel window is quoted
    by its real value.
  - **Responder en route** — who has the case, that respawn is held and roughly
    for how long, and that cancelling is still there.
  - **On cooldown** — the same three options with the remaining time in place of
    the cost.

- **Every line is driven by the server's own settings.** `requireCallItem`,
  `consumeCallItem`, `refundOnExpire`, `refundOnSelfRecovery`,
  `refundCancelSeconds` and `respawnBlockMaxSeconds` are now in the client
  settings projection, so the overlay describes the rules *this* server runs.
  With `consumeCallItem: 0` nothing is spent, so no refund line appears at all.
  Copy that promises a refund the server does not give is worse than no copy.

- **`refundOnSelfRecovery`** (default `1`). **This behaviour did not previously
  exist.** A player who spent a beacon, had nobody take the case, and got
  themselves back up was charged for it. That is the same situation as an
  expiry from the player's side — they paid, nobody came, they saved
  themselves — and it is the outcome most likely to stop someone ever calling
  again. Only applies to an *unclaimed* call: once a responder has accepted,
  someone is running across the map for you and the beacon is spent whatever
  happens next.

### Changed

- **"YOU ARE DOWN" → "YOU ARE UNCONSCIOUS"**, and the title switches to
  "EMERGENCY CALL ACTIVE" when the player is conscious with a call still open.
  The panel lingers for a moment after someone comes round, and on a
  `requireUnconscious: 0` server a conscious player can call at all — the old
  title was wrong in both cases.

- **Status line rewritten** from "Dispatch is listening." to "You cannot move or
  speak until you come round." The first was atmosphere; the second is the fact
  a confused player actually needs.

- **`mmer_call_button.layout`** is taller and carries four new text widgets
  (`mmer_opt1`, `mmer_opt2`, `mmer_opt3`, `mmer_hint`). All four are optional
  and null-checked, so a server running the old layout against new scripts keeps
  a working button rather than losing the overlay.

---
## 1.8.0 — 2026-09-13

**`rosterMode` — the radio can be the licence instead of the roster.**

The ask was "anyone with the radio set is an EMT, no whitelist." The thing worth
saying out loud before the feature exists: **the roster is not only who may
accept, it is who may SEE.** The dispatch panel carries every open call's
patient name, grid reference and live vitals. Opened up without thought, that is
a live feed of who on the server is helpless and precisely where — which is a
raid tool, not a medic system. So this ships as three modes, defaulting to the
one you already have.

### Added

- **`rosterMode: 0` — roster only.** `adminIds` + `teamIds`, exactly as before.
  The default, and unchanged behaviour for every existing server.

- **`rosterMode: 1` — open.** Anyone carrying a qualifying radio is a responder,
  with the full panel. Right for PvE and heavy-RP servers.

- **`rosterMode: 2` — open, need-to-know.** Anyone carrying a qualifying radio
  is a responder, but one who is *not* on the roster sees no patient name, no
  position and no vitals until they **accept** the case. They can still see that
  a call exists and how old it is, which is all they need in order to volunteer.
  Accepting reveals everything — and puts their name on the record and in
  Discord, so the reveal costs an identity.

  Redaction happens in `ForViewer()`, on the server, before the payload exists.
  Hiding it at the widget would be no protection at all against a modified
  client, which is the same rule `showPatientNames` has followed since 1.4.0.

- **The radio is the same radio.** Modes 1 and 2 qualify on `responderItemTypes`
  / `responderFrequency` / `responderRadioMustBeOn` — the settings
  `requireItemForResponder` already uses. An operator who has tuned that gate
  should not describe the same radio twice, and a medic should not carry two.

- **An open mode with an empty `responderItemTypes` is refused at boot.**
  `HasTunedRadio()` returns true for an empty list, on the reasoning that
  nothing was asked for — so an empty list in an open mode would qualify *every
  player on the server*. The mode falls back to roster only and says so as a
  warning. A silently wide-open queue is the one failure this feature must not
  have.

- **`Roster mode:` in the boot summary**, naming the mode, what qualifies
  someone, and the refusal above when it applies.

### Changed

- **`IsResponder()` now takes the player, not the Steam64.** In an open mode
  membership depends on what is in someone's hands, which a uid cannot answer.
  The durable question kept its own method, `IsRostered()`.

- **Chat tags stay roster-only, deliberately.** A tag driven by the radio would
  flicker as people stow and draw, and would quietly turn a recognition marker
  into a live "who is holding the panel right now" broadcast. The tag says the
  server vouches for this person; the queue says who can help today.

- **Transient membership is pushed, not polled.** The client decides whether the
  panel opens from the role in its state packet, so picking up a radio has to
  reach it. A sweep on the existing sync interval re-derives each online
  player's role and pushes state, the call list and a toast when it changes. It
  runs only in the open modes, and on the sync interval rather than every tick,
  because it walks each player's inventory.

### Fixed

- **The incoming-call toast leaked everything the panel was hiding.** It names
  the patient and their grid square, and it goes to every responder. Without
  per-recipient redaction, mode 2 would have withheld the location in the panel
  and then popped it up unprompted. `NotifyResponders()` now takes a redacted
  body alongside the full one and picks per recipient. Same for the patient-lost
  toast.

- **The archive is the same leak with a longer memory** — a roll of every player
  who has ever been downed, where, and how badly. A need-to-know viewer now sees
  only the cases they worked themselves.

- **A redacted call renders as withheld, not as zero.** The position is zeroed
  on the server, so an unguarded client would have drawn a perfectly plausible
  `000 000` and sent someone to the corner of the map. The payload carries an
  explicit `redacted` flag and the panel renders placeholders off that.

### Known limitation

In mode 2, a responder can accept a case purely to reveal the position and then
release it. That is deliberate rather than unsolved: the deterrent is
attribution, not prevention. Accepting writes their name to the call, the log
and Discord. A server that needs prevention wants mode 0.

---
## 1.7.4 — 2026-09-12

**The Terje Medicine readout is on, and it is built against Terje's published
interfaces instead of a guess.**

Up to 1.7.3 the Workshop listing said Terje was required while `MMER_TERJE` sat
commented out in the build, so every `terje:` row read `--` and the server log
said so plainly on boot. The listing was promising something the build did not
deliver. Worse, the one line the adapter would have used had it been switched on
was wrong.

### Changed

- **`MMER_TERJE` is now on, and `TerjeCore` / `TerjeMedicine` are in
  `requiredAddons[]`.** The flag and its addons are a pair; a flag on without
  its addons fails at build time with "unknown type", which is the loud failure
  the flags exist to produce. `config.cpp` now states that pairing for all three
  flags rather than leaving it in a comment on one of them.

### Fixed

- **The Terje accessor was wrong.** The adapter called
  `stats.GetStatValue(statId, value)`. No such method exists. `TerjePlayerStats`
  is a `TerjePlayerRecordsBase` of *named, typed* records, read with
  `TryGetIntValue` / `TryGetFloatValue` / `TryGetBoolValue` — each returning
  `false` for an id that was never registered.

  That `false` turns out to be exactly the right signal: it is Terje telling us
  the reading does not exist on this player, which is what `--` means. A reading
  that is off in Terje's own settings, or misspelled in `config.json`, now shows
  as unavailable rather than as a confident `0.00` that a responder would read
  as "no sepsis".

  The record is typed and there is no way to ask which type an id is, so the
  adapter tries all three accessors. Only one can match a registered id, and all
  three miss an unregistered one.

- **Radiation was looked up in the wrong place.** It is not a Medicine record.
  `PlayerBase.GetTerjeRadiation()` is a Terje Core interface that TerjeRadiation
  implements and that returns 0 without it, so the row is honest either way.

### Added

- **Friendly stat names.** `config.json` says `terje:sepsis`; the adapter maps
  it to Terje's `tm.sep_l`. The table covers 12 conditions, 7 wound counts and
  12 treatments-on-board, and any raw `tm.` id passes straight through, so the
  table is a convenience and never a ceiling.

- **Terje state is in the boot summary.** `Terje diagnostics: ON` /
  `OFF (built without MMER_TERJE)` / `OFF (terjeEnabled is 0 in config.json)`.
  Three different reasons a row can read `--`, previously indistinguishable from
  the chair — the same invisible-state problem that 1.7.1 fixed for the gates.

- Default `terje:` rows retuned: severity levels are 0–3 steps and wound
  readings are counts, so both now show with no decimal places instead of
  `1.00`. Treatments already on board are never flagged amber — they are not
  problems, and a responder reads them to decide what *not* to give a second
  dose of. Existing `config.json` files are untouched; this only changes what a
  fresh config is written with.

---
## 1.7.3 — 2026-09-12

**Every feature in the 1.5.0-1.7.x line is now verified in live play.** A review
of the full server log set (548 files, 42 server runs) found:

- Zero MMER script errors in every run from 1.5.0 onward. The compile errors in
  the archive are all from runs on 9 September, against builds superseded on the
  10th.
- Zero crashes since 9 September. All eight crash dumps predate the
  `TranslateString` fix and none reference current code.
- The complete call lifecycle observed end to end: beacon consumed whole, call
  dispatched to online responders, responder accepted with the radio gate live
  at 91.9 MHz, case closed on recovery, and a beacon correctly refunded on a
  cancel inside the 30-second window.

Two log entries are expected and are not defects:

- `ANIMATION (E): Can't load @<mod>/Anims/cfg/skeletons.anim.xml` — the engine
  logs this for every script-only mod. Two other mods on the same server produce
  it identically.
- `Discord webhook failed: timed out` — the host's outbound HTTPS does not
  return the response. The posts themselves arrive in Discord.

### Fixed

- The beacon refund log read `...your beacon was returned..` — the reason string
  already ends in a period and the format added a second.

---

## 1.7.2 — 2026-09-12

### Fixed

- **A consumed beacon was only partly consumed.** Spending a `Roadflare` took
  one unit off it and left the flare in the player's hands.

  DayZ overloads `quantity` for two unrelated concepts. On genuinely stackable
  items — ammo, nails, rags — it is a COUNT, and taking one means removing one
  item. On many others it is a RESOURCE: a Roadflare's quantity is its remaining
  **burn time**, a canteen's is millilitres, a fuel can's is litres. The 1.6.0
  code keyed off `HasQuantity()`, which is true for both, so consuming a flare
  shaved a second off its burn instead of spending it.

  Now keyed off `IsSplitable()`, which reads the item's own `canBeSplit` config
  flag (`ItemBase.c:243`) and is the only reliable way to tell a stack from a
  resource. A real stack still loses exactly one unit; everything else is
  removed whole.

  This matters for any item you configure, not just flares — a chemlight, a
  canteen or a custom beacon with a durability bar would all have behaved the
  same way.

---

## 1.7.1 — 2026-09-12

### Added

- **The server now states at boot which optional gates are live, and on what.**
  Three lines: the patient beacon gate, the responder radio gate, and chat tags.
  Each prints the item list, whether the item is consumed, the refund settings,
  the required frequency and whether power is checked.

  This exists because "is the feature even switched on" is invisible state, and
  invisible state has cost more time on this mod than any actual bug. A config
  key sitting at `0` and a genuine defect are indistinguishable from in-game;
  now the log settles it in one line before any testing starts.

  An empty item list reads `NO TYPES CONFIGURED - nothing will ever satisfy
  this`, because an empty list silently refuses every call and looks exactly
  like a broken check.

- A log line when a beacon is actually consumed, naming the call, the item and
  the player. Once per call, so it costs the same as the existing open/close
  lines.

---

## 1.7.0 — 2026-09-12

### Added

- **Responder radios, optionally frequency-locked.** `requireItemForResponder`
  now checks `responderItemTypes` (default `PersonalRadio`) rather than reusing
  the patient's beacon list — a flare and a radio are different things. Two new
  conditions on top:

  - `responderRadioMustBeOn` requires the radio to be genuinely working, via
    the energy manager's `IsWorking()` rather than `IsSwitchedOn()`: a radio
    flicked on with a dead battery is off as far as anyone using it is
    concerned.
  - `responderFrequency` requires it tuned to a given MHz value, compared with
    a tolerance rather than exact equality because these are floats off a
    stepped dial. `0` accepts any frequency.

  A frequency requirement belongs on the responder side and nowhere else: a
  responder is conscious, can retune in seconds, and is someone you can tell the
  frequency to directly. An unconscious patient can do none of those things,
  which is why the patient's beacon has no frequency condition and will not get
  one.

  Every refusal names the actual problem — missing, off or flat, or tuned wrong
  **with both frequencies printed**. Carrying several radios is fine; if any one
  of them qualifies, the medic is through.

New keys: `responderItemTypes`, `responderItemLabel`, `responderFrequency`,
`responderRadioMustBeOn`.

---

## 1.6.0 — 2026-09-12

### Added

- **Call beacons.** With `requireCallItem` on, a patient needs one of
  `callItemTypes` in their inventory to call for help, and it is consumed when
  the call opens. Off by default: turning it on changes who can call at all,
  which is not something an upgrade should do silently.

  The order is deliberate — the item is checked *before* the call is created and
  taken *after* it exists, so a refusal further down the function can never cost
  someone a beacon for nothing. The class name is recorded on the call before
  the delete, and persists in `active.json`, so a restart mid-case still knows
  what to hand back.

  It is refunded when the call expires with nobody answering, and when the
  patient cancels within `refundCancelSeconds`. Both exist for a reason: burning
  a scarce item and getting no rescue is the most frustrating outcome this
  system can produce, and a misclick should never cost anything. It is not
  refunded on a completed case (it did its job) or on death (they lost
  everything anyway). A refund goes to the inventory, or to the ground at their
  feet if they are full, rather than silently evaporating.

  A stack loses one unit, not the whole stack.

- **`requireItemForResponder`** (default off) makes the requirement symmetric:
  responders must carry one to accept a case, but theirs is never consumed.
  They pay in kit, the patient pays in stock — charging the person volunteering
  to run across the map would tax exactly the behaviour the mod exists to
  encourage.

New keys: `requireCallItem`, `callItemTypes`, `callItemLabel`,
`consumeCallItem`, `refundOnExpire`, `refundCancelSeconds`,
`requireItemForResponder`. `Roadflare` is the seeded default because it exists
in vanilla and reads as a signal; point the list at your own item when you have
one.

### Fixed

- `LoadActive()` still used the deprecated `JsonLoadFile`, the same silent-failure
  bug fixed for `config.json` and `archive.json` in 1.4.0: it returns void and
  leaves its target untouched on a parse error, so a corrupt `active.json` was
  indistinguishable from an empty one and the next save wrote over it. Now uses
  `LoadFile` and never overwrites a file it could not read.

---

## 1.5.0 — 2026-09-12

### Fixed

- **The chat tag never appeared on an Expansion server.** DayZ Expansion Chat
  (bundled in Expansion) replaces the chat renderer wholesale — it draws
  `HH:MM [Channel] Name: text` from its own `ExpansionChatLineBase` and never
  instantiates vanilla's `ChatLine`. The 1.4.0 hook was correct code attached to
  a class that was not drawing anything. Added a second hook against
  `ExpansionChatLineBase`, gated behind `MMER_EXPANSION_CHAT`, which fills
  Expansion's own `PlayerTag` slot rather than fighting its renderer — the
  sender widget is private on that class, so the supported field is also the
  only route. Both hooks can be enabled together; only one is ever live.
  Verified against `salutesh/DayZ-Expansion-Scripts`.
- The tag is prepended only when it is not already at the front of `PlayerTag`,
  so a redraw (Expansion re-renders every row on a chat resize) cannot stack
  duplicate tags or wipe a tag Expansion set itself.

### Added

- `Scripts/5_Mission/MMER_00_MissionDefines.c` — build flags for the mission
  module. A second defines file is necessary, not an oversight: Enforce compiles
  `3_Game`, `4_World` and `5_Mission` separately, so a `#define` in the 4_World
  file is invisible to UI code.
- Roster add and remove now confirm to the admin who did it, and tell the
  player concerned. Removal was previously silent on success, which is
  indistinguishable from a dead button.
- The tag decision is shared by both chat hooks via `MMER_ChatTag.Lookup()`,
  which carries no Expansion types so it compiles anywhere.

### Notes

Roster removal and admin-only enforcement were already present in 1.4.0 —
members show a REMOVE button, the ROSTER button only renders for admins, and
the server re-checks `IsAdmin` on add, on remove, and on the roster request.
This release makes the result visible rather than adding the capability.

---

## 1.4.0 — 2026-09-11

First public release. Chat tags, MIT licence, and the fixes from a full
adversarial security audit.

### Added

- **Chat tags.** Responders and admins get a configurable tag next to their
  name in chat, via `modded class ChatLine`. The server pushes the names of
  online responders and admins to every client (names only — chat carries a
  name and no Steam64, so a UID would be handing out identifiers to the whole
  server for no benefit). Keys: `chatTagEnabled`, `chatTagText`,
  `chatTagColor`, `chatTagAdminText`, `chatTagAdminColor`. Cosmetic only; it
  confers nothing and is checked nowhere.
- `LICENSE` (MIT), `SECURITY.md`, `.gitignore`, GitHub issue and PR templates,
  and a Workshop-ready description.

### Security — fixed

- **Discord mention injection through player names (high).** A player whose
  Steam persona was `@everyone` pinged the whole Discord every time they pressed
  CALL FOR HELP, repeatable on the call cooldown. JSON-escaping never helped:
  the string was syntactically fine and it was Discord's content layer acting on
  it. Now the payload sets `allowed_mentions` with an empty `parse` array, and
  names are stripped of mention, markdown, backtick and control characters and
  clamped to 48 characters before they reach a webhook body or the log file.
- **Unauthenticated disk write per RPC (high).** The archive handler logged
  every refusal — to a file, with a full open/append/close cycle — *before*
  checking whether the caller was on the roster. Any client could loop that RPC
  and drive sustained synchronous disk I/O plus hundreds of MB/hour of log
  growth, and once the volume filled, every JSON persist failed with it. Now
  silent, like every other refusal in the file.
- **No rate limiting anywhere (high).** Nothing bounded how fast a client could
  send, and the JSON parse happened before the role check. Added a 250 ms floor
  per player, a 2 s floor on the two handlers that fan out, and a 512-byte cap
  on inbound payloads — all before the parse.
- **A corrupt `config.json` was silently overwritten with defaults (high).**
  `JsonFileLoader.JsonLoadFile` is the deprecated variant: it returns `void` and
  never nulls its out-parameter on a parse error. The `if (!s_Settings)` guard
  was therefore dead code, the early return unreachable, and `Save()` ran
  regardless — so one malformed byte wiped `adminIds` (locking everyone out of
  roster administration, since admins are config-only), wiped `teamIds`, and
  discarded the live Discord webhook URL, while logging that it had not.
  Switched to `LoadFile`, which returns `bool`; a file that fails to parse is
  now never written over. Same fix for `archive.json`.
- **A dead patient could not respawn until they relogged (high).** `CloseCall`
  never pushed fresh state, so a patient who died while a responder was en route
  kept a stale `respawnBlocked` flag and found all three respawn buttons
  disabled with nothing to refresh them. Expired calls had the same gap. Every
  closure now pushes state, and the respawn screen asks the server on open.
- **Every patient's Steam64 was broadcast to responders (medium).** `patientUid`
  was on the wire in both the call list and the archive, and read by nothing on
  the client — one archive request handed a responder a name-to-Steam64 table
  for everyone ever downed on the server. Stripped. `medicUid` now survives only
  for the viewer who is that medic.
- **`showPatientNames: 0` gave no anonymity (medium).** It was honoured at the
  widget, so a modified client saw every name anyway. Now enforced server-side.
- **A responder could spam the webhook and force disk writes on demand
  (medium).** Accept/abandon looped freely. Added a webhook rate budget that
  reports what it dropped.
- **Claimed calls never expired (medium).** A responder who accepted and walked
  away held the patient's respawn locked and their position streaming to the
  whole roster for the rest of the server's uptime. Claimed calls now expire on
  their own, longer ceiling.
- **The call list vanished entirely above ~24 concurrent open cases (medium).**
  Past that the payload exceeded what the chunker would send and nothing went at
  all — a blank panel for every responder, exactly when the server was busiest.
  It now sheds diagnostics, then oldest cases, so the queue degrades instead of
  disappearing, and the warning is throttled so it is not its own write loop.
- **Removing a responder left their claimed calls stranded (low).** Authority
  over a claimed call is checked against `medicUid`, which does not consult the
  roster, so a removed responder kept Complete/Abandon — and kept those patients'
  respawn locked — indefinitely. Their cases now return to the queue.
- Chunk reassembly carries a message id, so a message that loses its tail can no
  longer splice into a following one of the same length. Header parsing no
  longer aliases a variable as both source and out-parameter.
- `respawnBlockMaxSeconds: 0` and `archiveMaxEntries: 0` fall back to their
  defaults instead of meaning "no limit" — a safety ceiling set to zero is not a
  request to remove the ceiling.
- Steam64 validation tightened from "8 to 24 digits" to exactly 17 starting
  with 7, which is what the comment always claimed.
- Cooldown entries are pruned once they expire.
- Two unauthenticated `Print` paths moved behind the debug flag.

### Verified clean in the audit

The identity guard on every handler; the authorization check on every state
change; the absence of the webhook URL and the admin lists from every client
payload; the archive paging arithmetic at its boundary cases; the absence of any
hand-rolled JSON writer; and the bounds on the chunk reassembly buffer.

### Documentation

Third-party mod references removed. Three claims in the README that the audit
found to be inaccurate — "nothing traps a player", "calls expire", and "names
are escaped before they reach the Discord webhook body" — have been corrected
and are now true.

---

## 1.3.0 — GOLD  ·  2026-09-11

**First fully verified build.** Every subsystem tested live on the Misfit
Mercenaries server with two players and confirmed working end to end: patient
call, responder dispatch, grid references, Discord reporting, archive, roster.
This is the reference build. Branch from here, not from anything earlier.

### Verified working in live play

| Subsystem | State |
|---|---|
| Unconscious overlay + CALL FOR HELP | working |
| `K` dispatch panel, click routing, ESC | working |
| Per-player call cooldown with live countdown | working |
| Live call queue across multiple clients | working |
| Grid references matching the in-game map | working |
| Diagnostics readout (vanilla rows) | working |
| Archive view, paged | working |
| Roster menu, add by name or Steam64 | working |
| Discord webhook — new, accept, complete, death, cancel | working |
| Respawn soft-lock | working |
| Config round-trip, new keys added on upgrade | working |

### Fixed in this release

- **RPC payload size limit.** A `Param1<string>` above some undocumented size is
  dropped by the engine with no error at either end — `RPCSingleParam` returns
  normally and `OnRPC` never fires on the client. Measured on this server: the
  settings payload (323 B) and roster (197 B) always arrived; the archive page
  (2007 B, logged as sent) and the call list with diagnostics (~2300 B) never
  did. This is why the dispatch panel and the archive both looked empty while
  the server log confirmed it had sent them. Payloads over 240 bytes are now
  split into numbered chunks and reassembled client-side; chunks are placed by
  index so arrival order does not matter, and a new message for the same RPC id
  resets the buffer so a dropped tail cannot corrupt the next send. 240 is
  deliberately below the smallest payload proven to work rather than a guess at
  where the ceiling actually is.
- **Call list trimmed.** Open cases carry full diagnostics; closed ones are
  slimmed, and anything closed more than 90 s ago is not sent at all.
- **Roster showed nobody.** Admins are responders — `IsTeam()` returns true for
  them — but the Responders list only rendered `teamIds`, so a server whose
  responders are all admins showed an empty roster. Admins now appear, tagged,
  with no remove button since they are config-only.
- **Online players list was always empty.** It explicitly skipped admins and
  existing responders. It now lists everyone online with their access level.
- **Add-by-name did nothing.** The box only accepted a Steam64 ID;
  `SanitiseUid` rejected any non-digit and returned in silence. It now accepts a
  name (exact match, then unique prefix — an ambiguous prefix resolves to
  nothing rather than the wrong person) or an ID, and always sends a toast back.

---

## 1.2.4 — 2026-09-11

- **Grid references were wrong.** Northing was computed as `z/100`, but the
  in-game map numbers its northing down from the top edge, not up from world
  Z = 0. Now uses the engine's own `World.GetGridCoords()` with the square size
  read from `CfgWorlds <world> Grid Zoom1 stepX` — the same call vanilla's
  map-navigation code makes. Correct on any terrain without a config setting.
- **Discord never reported cancels or self-heals.** Only four paths posted;
  cancel, expire and auto-close-on-recovery all went through `CloseCall()`,
  which posted nothing. Every closure now reports. New `discordOnCancel` key,
  default 1. The two sites with richer embeds (medic pressed Complete, patient
  died) are flagged so they do not double-post, and the pre-restart sweep still
  sends one summary instead of one post per closed call.
- **Archive load hardened.** `LoadArchive()` passed the manager's own
  `m_Archive` member straight into `JsonLoadFile`'s `out` parameter; it now
  loads into a local and assigns. If the in-memory archive is empty while
  `archive.json` has records, the server re-reads the file once rather than
  serving an empty list forever.
- Panel and roster menus poll their dirty flags, so a reply that lands while the
  menu is not yet current still renders.
- `mmer_respawn_notice` was being looked up in vanilla's respawn layout, where
  it has never existed — the respawn block worked but silently. Now created at
  runtime from its own layout.
- Server log now states archive record counts at boot and per request.

---

## 1.2.3 — 2026-09-10

- Cooldown counts down locally instead of freezing at the configured value.
- Call button label restored.
- Panel title configurable via `panelTitle`.
- Discord `Content-Type` fixed — `SetHeader()` takes the value only.
- Webhook failures decode `ERestResultState` into a readable reason.
- Logs print plain state names instead of raw stringtable keys.

---

## 1.2.x and earlier — build-out

Layout format rewritten against the Enfusion `.layout` spec (all six files were
wrong in every particular). Keybind registration fixed — `Inputs.xml` needs an
`inputs =` entry in `CfgMods`. Input lockout fixed by letting
`super.OnShow/OnHide` own focus instead of calling `ChangeGameFocus` twice and
then `ResetGameFocus`. ESC switched to `UAUIBack`. Click routing fixed with
`PlayerControlDisable(INPUT_EXCLUDE_ALL)` plus `ignorepointer` on decorative
children. RPC serialisation moved off `JsonSerializer.WriteToString(Class, …)`,
which produces `{}` because it works off the static type, onto per-class
`ToJson()` via `JsonFileLoader<T>`. Stringtable rebuilt with the 15-column
header and `STR_` prefixes.

---

## Notes

Every line of this mod is original work, written against the DayZ Enforce Script
API and the vanilla 1.29 game scripts. No third-party mod was decompiled,
unpacked, or borrowed from at any point.
