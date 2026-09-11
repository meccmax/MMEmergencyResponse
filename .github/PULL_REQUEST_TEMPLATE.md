## What this changes

<!-- One or two sentences. If it fixes an issue, say "Fixes #123". -->

## Why

<!-- What was wrong, or what this makes possible. -->

## How it was tested

<!-- Be specific. "Built and ran on a live server, two clients, confirmed X"
     beats "looks right". If you could not test something, say which part. -->

- [ ] Built with `build.bat` without errors
- [ ] Server started with no script errors in the RPT
- [ ] Tested in live play

## Checklist

- [ ] No Discord webhook URL, Steam64 ID, or other live credential in the diff
- [ ] Any new `config.json` key is added to `Data/config.example.json` **and** documented in the README
- [ ] Any new client→server RPC handler re-derives identity from `sender`, never from the payload
- [ ] Any new server→client payload was checked against the RPC size limit (see `MMER_Const.RPC_CHUNK`)
- [ ] `CHANGELOG.md` updated

## Notes for the reviewer

<!-- Anything surprising, any tradeoff you made, anything you are unsure about. -->
