# Versionary

[![CI](https://github.com/Spuk1/versionary-qt/actions/workflows/ci.yml/badge.svg)](https://github.com/Spuk1/versionary-qt/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/Spuk1/versionary-qt)](https://github.com/Spuk1/versionary-qt/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L71NX36P)

A fast, native Git GUI client built with C++ and Qt 6. Free and
open-source (GPL-3.0-or-later), with no Chromium runtime: fast startup,
low memory use, and a native look and feel on Linux, Windows, and macOS.

## Download

Get the latest build for your platform from the
[Releases page](https://github.com/Spuk1/versionary-qt/releases):

| Platform | File | How to run |
|----------|------|------------|
| Linux | `Versionary-<ver>-Linux-x86_64.AppImage` | `chmod +x *.AppImage && ./Versionary-*.AppImage` (Qt bundled, no install needed) |
| Windows | `Versionary-<ver>-Windows-x86_64.zip` | Unzip and run `bin/Versionary.exe` (Qt DLLs included) |
| macOS (Apple Silicon) | `Versionary-<ver>-macOS-arm64.dmg` | Open the DMG, drag to Applications. First launch: right-click → Open (unsigned build) |

Requires `git` on PATH.

## Features

- Multi-repository tabs with drag-reorder, rename, close, and workspaces
- Fetch / pull / push (with set-upstream prompt), clone with account,
  protocol selection, and repository search
- Branches: create, checkout, rename (local + remote), delete, merge,
  fast-forward, ahead/behind, pull-request links (GitHub/Bitbucket)
- Staging: stage/unstage/discard per file or all, partial line-level
  stage/discard/unstage via clickable diff margin, commit, delete
  untracked files from disk (to the system trash)
- History: paginated searchable log, commit details, per-file diffs,
  tags, detached-HEAD checkout, blame, file history
- Stash: push with file checklist, apply/pop/drop/rename, stash viewer
- Submodules: list, reset to recorded commit, auto sync on fetch/pull/clone
- OAuth login for GitHub, Bitbucket, and GitLab (works out of the box in
  official builds), global + per-repo SSH keys, ssh-agent detection
- Git identity management, system/light/dark themes, onboarding wizard,
  quick-open (`Ctrl+P`), layout persistence

## Build from source

Prerequisites: CMake 3.21+, Qt 6.4+ (Widgets, Network, Concurrent, Test),
a C++17 compiler, and `git` on PATH.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/Versionary
```

### API keys for local builds

Official builds ship with OAuth API keys baked in. For self-compiled
builds, logins stay disabled until you provide your own — either via
environment variables, a `.env` file next to the binary (or at
`~/.Versionary/.env`), or build-time defines:

```bash
cmake -S . -B build -DVERSIONARY_GITHUB_CLIENT_ID=… \
  -DVERSIONARY_GITHUB_CLIENT_SECRET=… […]
```

### Install on Linux

```bash
sudo scripts/install.sh          # installs to /usr/local
sudo scripts/install.sh /usr     # installs to /usr
```

This installs the binary, desktop entry, icons, and AppStream metadata.
A `PKGBUILD` (`packaging/`) and Flatpak manifest
(`packaging/io.github.spuk1.Versionary.yml`) are included for distro
packaging.

## Project layout

- `src/core` — process runner, JSON stores, identity, app config
- `src/git` — git operations, auth planning, diff/patch helpers
- `src/net` — OAuth + REST API clients
- `src/models` — commit data types and paginated log model
- `src/ui` — main window, panels, dialogs, theme
- `tests` — QtTest unit tests
- `packaging` — desktop entry, icons, AppStream metadata, PKGBUILD,
  Flatpak manifest
- `scripts` — build and install helpers

## Contributing

Bug reports and pull requests are welcome — please open an issue first to
discuss substantial changes. By contributing you agree your work is
released under the same license (GPL-3.0-or-later).

## Support

Versionary is free software maintained in spare time. If it saves you
time, consider supporting development:

[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L71NX36P)

## License

[GPL-3.0-or-later](LICENSE).
