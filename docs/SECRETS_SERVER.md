# OAuth key server (`secrets.fairyland.freemyip.com`)

Distro builds (AUR, Flathub) run on other people's machines, so secrets
can't be baked in at build time the way GitHub Release builds do. Instead,
when the client has no local configuration at all, it fetches the public
OAuth credentials from this server as a last resort.

## How it works

- Tiny Python 3 server (stdlib only, `server.py`), bound to
  `127.0.0.1:8765`, run as `nobody` via
  `versionary-secrets.service`. Reads values from
  `/opt/versionary-secrets/oauth.env` (root-owned, mode 600).
- `GET /v1/oauth-keys` returns JSON with the non-empty
  `VERSIONARY_*` values; `GET /healthz` returns `{"ok": true}`.
- nginx (`/etc/nginx/conf.d/secrets.conf`) reverse-proxies
  `https://secrets.fairyland.freemyip.com` → localhost:8765, TLS via
  Let's Encrypt (`certbot --nginx`, auto-renewed by `certbot-renew.timer`).
- Client (`src/core/AppConfig.cpp`): precedence is env > legacy env >
  baked > `.env` > legacy `.env` > **remote**. The fetch is a single
  blocking HTTPS GET with a 3 s timeout, result cached in memory.
  `VERSIONARY_NO_REMOTE_CONFIG=1` disables it (used by the test suite);
  `VERSIONARY_SECRETS_URL` overrides the URL (testing, self-hosting).

## Threat model (read this)

The endpoint is **public by design**: anyone can
`curl https://secrets.fairyland.freemyip.com/v1/oauth-keys`. A desktop app
cannot authenticate itself, so this provides **zero confidentiality** —
exactly equivalent to baking the keys into the binary (recoverable via
`strings`). What it buys is operations: keys can be **rotated server-side
without cutting new releases**, and distro packaging stays clean.

## Maintenance

```bash
# check health (from anywhere)
curl https://secrets.fairyland.freemyip.com/healthz

# on the server: restart after changing oauth.env
sudo systemctl restart versionary-secrets
sudo systemctl status versionary-secrets

# check certificate / renewal
sudo certbot certificates
systemctl list-timers | grep certbot
```

### Rotating a key

1. Regenerate it in the provider dashboard (GitHub app settings /
   Bitbucket workspace OAuth consumer).
2. Update `/opt/versionary-secrets/oauth.env` on the server (mode 600).
3. `sudo systemctl restart versionary-secrets`.
4. Rebuild + release the apps that **bake** the old value (GitHub Release
   binaries embed it; server-fallback clients pick up the new value
   automatically).

### If the server is down / unreachable

Clients behave exactly as if no keys were configured: that provider's
login is disabled, everything else works. The 3 s timeout applies once
per process (result cached), only when no local configuration exists.
