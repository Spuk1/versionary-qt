# Versionary-Qt

Native C++ / Qt6 port of [Versionary](https://github.com/spuk1/versionary),
a free open-source Git GUI client. Same features as the Electron/React
client, without the Chromium runtime: faster startup, lower memory use,
and native look and feel.

## Download

Grab the latest installer for Linux (AppImage), Windows (zip), or macOS
(DMG) from the
[Releases page](https://github.com/Spuk1/versionary-qt/releases).
Maintainers: see [docs/RELEASING.md](docs/RELEASING.md) for how releases
are built and versioned.

## Features (parity with the Electron client)

- Multi-repository tabs with drag-reorder, rename, close
- Workspaces grouping repositories
- Fetch / pull / push (with set-upstream prompt), clone with account +
  protocol selection and repository search
- Branches: create, checkout, rename (local + remote), delete, merge,
  fast-forward, ahead/behind, pull-request links (GitHub/Bitbucket)
- Staging: stage/unstage/discard per file or all, partial line-level
  stage/discard/unstage via clickable diff margin, commit
- History: paginated searchable log, commit details, per-file diffs,
  create tag, checkout commit (detached HEAD)
- Stash: push with file checklist, apply/pop/drop/rename
- Submodules: list, reset to recorded commit, auto sync on fetch/pull/clone
- OAuth for GitHub, Bitbucket, GitLab (PKCE loopback, token refresh)
- Global + per-repo SSH keys, ssh-agent detection
- Git identity management, system/light/dark themes, onboarding wizard

## Build

Prerequisites: CMake 3.21+, Qt 6.4+ (Widgets, Network, Concurrent, Test),
a C++17 compiler, and `git` on PATH.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/Versionary
```

Copy `.env` next to the binary (or export the variables) to enable OAuth:

```bash
cp .env build/.env
```

## Install (Linux)

```bash
sudo scripts/install.sh          # installs to /usr/local
sudo scripts/install.sh /usr     # installs to /usr
```

This installs the binary, desktop entry, icons, and AppStream metadata.
OAuth keys are read from `~/.Versionary/.env` (same names as `.env`).

### OAuth credentials: env, baked, or file

Resolution order (first hit wins):

1. Process environment (`VERSIONARY_GITHUB_CLIENT_ID`, …) — highest priority.
   Legacy pre-1.0 names (`GITHUB_CLIENT_ID`, `CLIENT_GITHUB_SECRET`, …)
   still work as a fallback.
2. Build-time baked values for packaged builds:
   `cmake -DVERSIONARY_GITHUB_CLIENT_ID=… […]`. The AUR `PKGBUILD`
   reads them from the environment or a dotenv-style `.env` placed next
   to it (never packaged). Never commit real secrets.
3. `.env` lookup: next to the binary, current directory, then
   `~/.Versionary/.env` — this is how dev builds and the Flatpak find
   keys (`install.sh` seeds `~/.Versionary/.env` from `./.env` once).

### Arch Linux (AUR)

```bash
git clone https://aur.archlinux.org/versionary-qt-git.git
cd versionary-qt-git
makepkg -si
```

The `PKGBUILD` lives in `packaging/` (bump `pkgver` via the `pkgver()`
function automatically on build). See [docs/PACKAGING.md](docs/PACKAGING.md)
for publishing to AUR and Flathub.

### Flatpak

```bash
flatpak-builder --install build-dir packaging/io.github.spuk1.Versionary.yml
```

Requires the `org.kde.Platform//6.8` runtime. The manifest is validated
for structure locally; a full `flatpak-builder` run was not possible in
this environment (no builder installed).

## Layout

- `src/core` — process runner, JSON stores, identity
- `src/git` — git operations, auth planning, diff/patch helpers
- `src/net` — OAuth + REST API clients
- `src/models` — commit data types and paginated log model
- `src/ui` — main window, panels, dialogs, theme
- `tests` — QtTest unit tests

See `Agents.md` for contributor rules.
