# Releasing Versionary

Releases are fully automated. Pushing (or auto-creating) a `vX.Y.Z` tag
builds the app for **Linux, Windows, and macOS** with the OAuth keys baked
in, and publishes downloadable files on the GitHub Release page.

## 1. One-time setup: add 6 secrets

The app needs OAuth client IDs/secrets at build time so users don't have to
supply their own. They are baked into the binary via CMake
(`src/core/AppConfig.cpp`, `CMakeLists.txt`) — the `.env` file is only for
local development and is git-ignored.

Go to **GitHub repo → Settings → Secrets and variables → Actions → New
repository secret** and add these six (names must match exactly —
note they use a `VERSIONARY_` prefix because GitHub reserves the `GITHUB_`
prefix for its own variables):

| Secret name                        | Value source                                   |
|------------------------------------|------------------------------------------------|
| `VERSIONARY_GITHUB_CLIENT_ID`      | same value as in your `.env` (identical name)  |
| `VERSIONARY_GITHUB_CLIENT_SECRET`  | same value as in your `.env` (identical name)  |
| `VERSIONARY_BITBUCKET_CLIENT_ID`   | same value as in your `.env` (identical name)  |
| `VERSIONARY_BITBUCKET_CLIENT_SECRET` | same value as in your `.env` (identical name)|
| `VERSIONARY_GITLAB_CLIENT_ID`      | your GitLab OAuth app client ID (empty = GitLab login disabled) |
| `VERSIONARY_GITLAB_CLIENT_SECRET`  | your GitLab OAuth app secret (empty = disabled) |

Notes:

- These are **OAuth client IDs/secrets for installed/desktop apps** (public
  clients with loopback redirect + PKCE), not passwords. They ship inside
  the binary by design — same as every distributed desktop app.
- No secret is ever printed to logs: the workflow passes them only as
  `-DVERSIONARY_...=...` CMake flags.
- `CI` builds (`ci.yml`, runs on every push/PR) use dummy values and don't
  need any secrets.

## 2. Make a release

**Option A — auto-increment (recommended):**

1. Go to **Actions → Release → Run workflow**.
2. Choose `bump`: `patch` (1.0.0 → 1.0.1), `minor` (→ 1.1.0), or `major`
   (→ 2.0.0). The workflow finds the highest existing `vX.Y.Z` tag,
   creates the next one, builds all platforms, and publishes the Release.
3. The version is stamped into the binary via `-DVERSIONARY_VERSION=…`
   (`VERSIONARY_VERSION` compile definition).

**Option B — manual tag:**

```bash
git tag v1.2.3 && git push origin v1.2.3
```

Pushing the tag triggers the same build + publish, using the version from
the tag name.

## 3. What users download

| File                              | Platform | How to run                                            |
|-----------------------------------|----------|-------------------------------------------------------|
| `Versionary-<ver>-Linux-x86_64.AppImage` | Linux | `chmod +x *.AppImage && ./Versionary-*.AppImage` (portable, Qt bundled) |
| `Versionary-<ver>-Linux-x86_64.tar.gz`   | Linux | needs system Qt 6; binary + desktop file              |
| `Versionary-<ver>-Windows-x86_64.zip`    | Windows | unzip, run `bin/Versionary.exe` (Qt DLLs included via `windeployqt`) |
| `Versionary-<ver>-macOS-arm64.dmg`       | macOS (Apple Silicon) | open DMG, drag to Applications. First run: right-click → Open (unsigned build) |

## 4. Troubleshooting

- **Workflow fails at "Configure"**: one of the six secrets is missing.
  Empty GitLab values are fine (they just disable GitLab login); the other
  four must exist. Check Settings → Secrets and variables → Actions.
- **macOS "unidentified developer" warning**: expected — the DMG is not
  Apple-notarized. Right-click → Open on first launch.
- **No release created**: releases only run on `workflow_dispatch` or a
  `v*.*.*` tag push. Plain pushes to `main` only run `ci.yml`.
