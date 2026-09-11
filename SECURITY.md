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
| Responder / admin role | Server | Re-checked in every handler |
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
