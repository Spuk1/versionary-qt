# Agents.md — Versionary-Qt

Qt6 Widgets port of the Versionary Electron/React git client. Lives in a
separate folder (`Versionary-Qt`) next to the original (`Versionary`).

## Build / test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/Versionary
```

Requires Qt 6.4+ with Widgets, Network, Concurrent, Test. No other
dependencies — git operations shell out to the system `git` binary.

## Architecture

- `src/core/` — process + persistence primitives. No UI, no network.
  - `GitRunner` — `git` QProcess wrapper with `index.lock` wait + retry
    (ports the execa wrapper in `main/ipc.ts`). Only shared code touching
    `QProcess`; everything git-related goes through it.
  - `AppConfig` — storage paths + `.env` OAuth credentials. Repositories and
    workspaces stay in `~/.Versionary/*.json` (same files as Electron client).
    Tokens live in `tokens.json` (plaintext fallback shape), so existing
    `tokens.json` vaults from the Electron client keep working.
  - `RepositoryStore` / `WorkspaceStore` / `AuthStore` / `SshStore` —
    JSON persistence, one concern each, all unit-tested where logic lives.
  - `IdentityStore` — global `user.name`/`user.email` via `git config`.
- `src/git/` — git domain logic, blocking calls, no UI.
  - `GitService` (+ `GitServiceHistory.cpp`) — every repo operation; mirrors
    the `ipcMain.handle(...)` handlers one-to-one. Throws `GitException`.
  - `RemoteUtils`, `StashUtils`, `DiffPatch` — pure functions, fully tested.
  - `AuthPlanner` — SSH-vs-OAuth transport planning with injected credential
    provider, so it is testable without network.
- `src/net/` — `OAuthClient` (PKCE loopback on fixed port 6969 — the
  registered redirect URI, do not change) and `ApiClient` (blocking REST
  for validate/refresh/search, same endpoints as `ipc-oauth.ts`).
- `src/models/` — `GitTypes.h` plain structs, `CommitLogModel` paginated log,
  `CommitGraph` lane layout (port of `layoutLanes`, tested).
- `src/ui/` — `RepoManager` (state, mirrors `GitContext`), `MainWindow`
  (composition + async dispatch only), one widget/dialog per file.
  `HistoryDelegate` paints the commit rows; keep row rendering there, not
  in `HistoryWidget`. Same split for the changes file lists:
  `FileListDelegate` owns file/folder row rendering, `StagingWidget` owns
  data and actions. Busy animation lives in `LoadingBarWidget` (driven by
  `RepoHeaderWidget::setBusy` via `MainWindow::setBusy`); keep all busy
  state flowing through there. The repo tab strip uses `FlowLayout` (wrapping pills);
  tab buttons carry `tab=true` + `repoPos` properties for styling and drag-
  reorder — keep both in `RepoTabBar` (never `pos`: reserved QWidget property).
- Merge/cherry-pick/rebase conflicts surface through `OperationState`
  (git-dir markers) with the `StagingWidget` banner + `ConflictDialog`;
  `Continue`/`Abort`/`Skip` map 1:1 to git verbs. Long network ops take an
  optional progress callback + cancel flag (`ProgressParse`, tested) shown
  in `OperationProgressDialog`, which must stay modeless (modal wedges the
  UI behind stalled remotes); capture it via `QPointer`, never raw.
  `MainWindow::setBusy` counting plus the stuck-busy watchdog backstops
  infinite spinners. Never let git open an editor:
  `GIT_EDITOR=true` is forced in `GitRunner`; finalize commits with
  `--no-edit`.

## Rules for changes

- Blocking git/network calls must never run on the GUI thread — dispatch
  via `runBackground`/`runBackgroundVoid` (`AsyncHelper.h`).
- Never capture locals or parameters by reference in any lambda that
  outlives its creating scope: background workers AND their completion
  handlers (`[this, value]` copies only), deferred `singleShot` calls, and
  `connect` slots. Dangling captures caused the stash/merge/theme
  segfaults — see `test_treeops` for the regression test.
- Never use reserved `QWidget` property names (`pos`, `geometry`, …) for
  custom data — `setProperty("pos", i)` silently fails and reads back 0.
  All tab actions then hit the first repository; use distinct names like
  `repoPos` (see `test_repotabbar`).
- Panels must not refresh on `RepoManager::selectionChanged` (the repo path
  is still stale at that point). Only `MainWindow` refreshes panels, after
  `GitService::setRepoPath`. Async completions must drop results when the
  captured repo path no longer matches the selected one.
- `GitService::setRepoPath` is called from the GUI thread only; workers
  only read the path.
- Keep functions small and single-purpose; add a pure helper + unit test
  instead of growing `GitService` or a widget.
- New user-visible behavior needs a QtTest case under `tests/`
  (register in `tests/CMakeLists.txt`). Every dialog must stay
  constructible in `test_dialogs` — it catches member-use-before-init
  crashes in constructors (e.g. the Settings identity-tab segfault).
- JSON stores must be written atomically (temp + rename) — the Electron
  client writes the same files, and torn concurrent writes corrupt them
  (seen live as `"group": 21852`). `RepoManager::load` clamps out-of-range
  groups so a corrupt file can never hide repositories.
- Packaging lives in `packaging/` (desktop file, metainfo, icons,
  `PKGBUILD`, Flatpak manifest). Install paths go through CMake
  `GNUInstallDirs` — never hardcode `/usr` in scripts (pass a prefix).
- OAuth credential precedence is env → baked (`-DVERSIONARY_*`, set by
  packagers only) → `.env` file. Never commit real secrets; the repo
  `.env` stays git-ignored.
- Theme colors come from `ThemeManager` (palette mirrors `App.css`);
  delegates and items must use `ThemeColors.h` (exact values), never
  derived/ad-hoc colors. `main.cpp` pins Fusion style + Sans Serif 10pt so
  system themes cannot override the look.
  System detection falls back to `GTK_THEME` (set `GTK_THEME=Adwaita:dark`
  on Hyprland without the Settings portal).
- Never restyle the whole application while a popup is open — hide it and
  defer `applyTo` with a queued/singleShot call (this segfaulted before).
- OAuth redirect port 6969 and `.env` key names (`GITHUB_CLIENT_ID`,
  `BITBUCKET_CLIENT_ID`, `CLIENT_GITHUB_SECRET`, `CLIENT_BITBUCKET_SECRET`,
  `GITLAB_CLIENT_ID`, `CLIENT_GITLAB_SECRET`) must stay compatible with
  the Electron client.
- The loopback handshake must stay browser-robust: read full headers
  before parsing, flush + graceful close so the browser renders the
  result page (no RST race), verify `state`, keep serving favicon/extra
  hits. No embedded browser: WebEngine would cost ~100MB Chromium and
  providers (esp. Atlassian) refuse logins from embedded webviews.
