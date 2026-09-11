# Changelog — MM Emergency Response

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
