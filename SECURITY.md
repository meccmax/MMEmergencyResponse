# Security

## Reporting

Open a GitHub issue for anything that isn't exploitable. For something that
is — a way for a normal player to act as a responder, read data they shouldn't,
or take the server down — please report it privately through GitHub's
**Security → Report a vulnerability** rather than in a public issue.

## Threat model

The server is authoritative and trusted. **Every client is assumed to be
fully modified and hostile.** A hostile client can send any RPC id, with any
payload, at any rate, aimed at any entity. It follows that:

- No server handler may take an identity, a role, or a position from an RPC
  payload. All three are re-derived from the engine-supplied `sender`.
- No payload sent to a client may contain anything that client is not entitled
  to see, regardless of whether the stock UI would display it.
- Anything a client can trigger must be bounded — in rate, in allocation, and
  in disk writes.

Client-side checks in this mod (the `K` panel's role test, the respawn lock,
the chat tag) are **conveniences, not controls**. Every one of them is
re-checked on the server, and the respawn lock is deliberately advisory —
see below.

## What is enforced where

| Control | Where | Notes |
|---|---|---|
| Caller identity | Server | `sender.GetPlainId()`, never the payload |
| RPC arrived on the caller's own entity | Server | Blocks aiming a request at another player |
| Responder / admin role | Server | Re-checked in every handler, from the player entity — in an open `rosterMode` the answer depends on what they are carrying |
| Need-to-know redaction (`rosterMode: 2`) | Server | Fields are removed in `ForViewer()` before the payload exists, and from the toast bodies too |
| Map marker recipients (`markerMode: 3`) | Server | Each marker is addressed to one player; a redacted viewer gets none unless they accepted the case |
| Patient anonymity (`showPatientNames`) | Server | Blanked in the payload, not at the widget |
| Call cooldown | Server | Client countdown is display only |
| Inbound rate limit and payload size cap | Server | Before the JSON parse and before the role check |
| Discord mention suppression | Server | `allowed_mentions` plus content stripping |
| Respawn lock | **Client** | Advisory by design — see below |
| Chat tag | **Client** | Cosmetic; confers nothing |

## Deliberate non-controls

**The respawn lock is client-side and always will be.** It exists to stop an
honest player wasting their responder's time, not to trap anyone. A modified
client ignores it entirely, and that is the accepted tradeoff: the alternative
is server-side control over a player's ability to respawn, which is a far worse
failure mode the first time it goes wrong. The lock also lifts itself after
`respawnBlockMaxSeconds`, releases when the call closes, and releases when the
responder disconnects.

**The chat tag matches on display name, because chat carries a name and no
Steam64.** A player who renames themselves to match a responder will be tagged.
Nothing in the mod trusts a name for anything, so this is a cosmetic
impersonation only — the same one available to anyone in any game with a name
field. If that matters on your server, set `chatTagEnabled: 0`.

**`rosterMode: 2` deters, it does not prevent.** A non-roster responder can
accept a case purely to reveal the patient's position and then release it. That
is a deliberate trade, not an oversight: the control is attribution, not
prevention. Accepting writes their name to the call record, `emergency.log` and
the Discord post, so reading the queue costs an identity every time. A server
that needs this to be impossible should run `rosterMode: 0`, where the question
never arises.

What mode 2 *does* guarantee is that nothing identifying leaves the server for a
viewer who has not committed: the name, position and vitals are stripped in
`ForViewer()`, the popup gets a separate redacted body, and the archive returns
only cases that viewer worked. There is nothing on the wire for a modified
client to un-hide.

**An open `rosterMode` with an empty `responderItemTypes` is refused at boot.**
The inventory check returns true when nothing was asked for, so an empty list
would qualify every player on the server. The mode falls back to roster only and
logs a warning rather than running a wide-open queue the operator did not ask
for.

**`markerMode: 2` publishes the patient's position to the entire server, by
design.** DayZ Expansion server markers are global; there is no per-player
variant of them. This is not a defect in the mod or in Expansion, but it is a
disclosure, and on a server running `rosterMode: 2` it silently defeats that
setting — the queue withholds the grid and the map hands it over. The boot
summary reports mode 2 as a warning for exactly this reason. Use `markerMode: 3`
unless a public call is what you want.

**A marker is a third channel for the position.** The panel, the incoming-call
toast and the map all carry it, so each one is filtered separately and by the
same rule. If you add a fourth, filter it too.

**Steam64 IDs appear in `emergency.log`.** They are what you need to add
someone to the roster, so they stay. Treat the log as containing player
identifiers when you share it — the bug report template says so too.

## Handling `config.json`

`config.json` holds a **live Discord webhook URL**. Anyone who reads it can
post to that channel as your bot.

- It is in `.gitignore`. Do not commit it.
- Redact it before pasting a config into an issue, a Discord message, or a
  support thread.
- If it leaks, delete and recreate the webhook in Discord
  (Channel → Integrations → Webhooks), then paste the new URL into
  `config.json`. Rotating is instant and costs nothing.

If the file fails to parse, the mod logs the error, runs on defaults for that
session, and **does not overwrite it** — so a bad edit costs you a restart, not
your admin list and your webhook.

## Audit history

| Version | Scope | Outcome |
|---|---|---|
| 1.4.0 | Full adversarial review of all client→server paths, payload contents, injection surfaces, resource limits and logic flaws | 17 findings; all fixed or documented above |
| 1.7.3 | Server log review across 42 server runs, and a check that the new item gates hold the same trust boundary | No findings. Zero script errors and zero crashes since 1.5.0 |
| 1.7.4 | Terje adapter reviewed against Terje's published interfaces before enabling it | One correctness finding, fixed — see below. No new trust-boundary surface |
| 1.8.0 | Open roster modes: what widening "who is a responder" exposes | Two leaks found and closed before release — the incoming-call toast and the archive. One accepted limitation, documented below |
| 1.8.2 | Map markers, as a third channel that carries the patient's position | No new findings. `markerMode: 2` re-documented as a deliberate global disclosure and demoted to a boot-time warning |

The 1.7.4 finding was not a security hole but it was the same class of mistake:
the Terje adapter called a method that does not exist, which would have failed
at build time rather than silently — but the *reason* it was unverified is that
it had never been compiled. Enabling a dependency you have not read is guessing,
and this mod does not guess about APIs. The replacement is documented in
`MMER_TerjeAdapter.c` with the three facts from Terje's public interfaces that
it relies on, so the next person to touch it can check them in a minute.

Nothing about the Terje readout crosses a trust boundary the mod did not already
have: it is read server-side from the patient's own entity, on a call the
responder was already authorised to make, and the values reach the client in the
same payload as the vanilla vitals.

The item gates added in 1.6.0 and 1.7.0 follow the same rule as everything else:
**the server reads the player's real inventory, never a client's claim to hold
something.** The check runs on the entity the RPC guard already matched to the
sender, and the item is consumed by the server. A modified client cannot
fabricate a beacon, cannot skip the gate, and cannot spend someone else's.

The 1.4.0 pass verified as clean: the identity guard on every handler, the
authorization check on every state change, the absence of the webhook URL and
the admin lists from every client payload, the archive paging arithmetic at its
boundary cases, the absence of any hand-rolled JSON writer, and the bounds on
the RPC chunk reassembly buffer.

It found and fixed, in severity order: Discord `@everyone` injection through
player names; an unauthenticated per-RPC disk write; the absence of any rate
limiting; a corrupt `config.json` being silently overwritten with defaults,
destroying the admin list and the webhook credential; a dead patient being left
unable to respawn until relog; every patient's Steam64 being broadcast to
responders who never used it; `showPatientNames` being enforced only on the
client; claimed calls never expiring; and several unbounded-growth paths.

See `CHANGELOG.md` for the per-item detail.
