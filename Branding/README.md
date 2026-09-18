# Branding

Built from the Misfit Mercenaries logo. The skull is the original artwork
composited in, not a redraw — it is cut out of its black background with a
flood fill from the frame edges, so the eye sockets stay black instead of
being punched through the way a luminance key would have done.

| File | Use |
|---|---|
| `logo-1024.png` | Steam Workshop preview, Discord, anywhere square |
| `logo-dev-1024.png` | The same, marked **DEVELOPMENT BUILD** |
| `banner-1280x640.png` | GitHub social preview (Settings → Social preview) |
| `banner-dev-1280x640.png` | The same, marked **DEVELOPMENT BUILD** |
| `serverlogo-256.png` | `serverLogo` in config.json — transparent, no wordmark |

The dev variants swap the accent from red to amber and add a hazard band, so
the two are distinguishable at thumbnail size rather than only up close.

**DayZ will not load a PNG.** `serverLogo` in `config.json` and `picture` in
`config.cpp` both need `.edds` or `.paa` — convert with ImageToPAA from DayZ
Tools. The PNGs here are the masters.

Colours match the mod: alert red `#E04B4B`, admin amber `#E0A94B`, responder
green `#4BE07A` — the same values the chat tags and markers use.
