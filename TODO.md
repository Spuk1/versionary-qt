# TODO — missing UI features

Tracked leftovers from the UX-improvement round. All items below were
planned but not implemented yet. Build with `cmake --build build`, test
with `ctest --test-dir build`.

## Quick open (`Ctrl+P`)
- New `ui/QuickOpenDialog.{h,cpp}`: `QLineEdit` filter + `QListWidget`
  results over all repositories (name + path), Enter/double-click to open.
- Needs a pure static filter helper
  (e.g. `filterRepos(repos, query) → QVector<int>`, rank name-starts-with,
  then name-contains, then path-contains) plus `tests/test_quickopen.cpp`.
- Wire into `MainWindow::setupMenuBar` General menu (`Ctrl+P`) and call
  `RepoManager::revealRepository` — currently **private**, make it public.
- Register dialog sources in `CMakeLists.txt`.

## Status bar awareness
- Add permanent `QLabel`s in `MainWindow` for ahead/behind (`↑n ↓m` /
  `✓ in sync`) and last fetch time (`Fetched HH:mm`).
- Extend `MainWindow::refreshHead` worker to also call
  `GitService::aheadBehind(branch)` and parse the `behind\tahead` result.
- Stamp last-fetch time in `doFetch` success path (covers focus fetches).

## Empty state (no repositories)
- Add an overlay widget in `MainWindow` center (message + Open/Clone
  buttons) shown when `repositories()` is empty; hide tabs + splitter.
- Toggle from `onRepoSelected` and `repositoriesChanged`; needs the center
  `QSplitter` as a member (`m_centerSplitter`).

## Layout persistence
- Save/restore in `MainWindow::closeEvent`/ctor via `QSettings`: window
  geometry, center splitter, staging + history splitters (make them
  members or `findChild` by object name).
- Already persisted: diff view mode, word wrap, ignore-whitespace,
  commit history, clone folder, theme, selected repo/workspace.

## Fetch all repositories
- General menu entry; one worker looping all repos with a local
  `GitService` instance (do **not** reuse `m_git` — path races), shared
  cancel flag, per-repo phase labels on the existing progress dialog,
  error summary at the end, then `refreshHead` + source tree refresh.

## Stash viewer upgrade
- Replace the patch-only dialog in `SourceTreeWidget::showStashDiff`
  with a file list (`commitDetails("stash@{N}")`) + diff pane.
- Per-file "Apply" via existing `GitService::applyStashFile`, plus
  Pop/Drop buttons reusing the current menu handlers.
