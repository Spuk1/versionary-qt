# Distributing via AUR and Flathub

## AUR (Arch Linux)

The `packaging/PKGBUILD` is a `-git` package: it always builds the latest
`main`, and `pkgver()` derives the version automatically (from the newest
`vX.Y.Z` tag once releases exist). You publish it once; no per-release
updates needed. Both `versionary-qt-git` and `versionary-qt` names are
currently free on AUR.

1. Create an account at https://aur.archlinux.org and upload an SSH key
   (My Account → SSH Keys).
2. On an Arch machine, from the repo root:
   ```bash
   cd packaging
   makepkg -si        # verifies the build + installs locally
   makepkg --printsrcinfo > .SRCINFO
   ```
3. Push to AUR:
   ```bash
   git clone ssh://aur@aur.archlinux.org/versionary-qt-git.git aur-push
   cp packaging/PKGBUILD packaging/.SRCINFO aur-push/
   cd aur-push && git add PKGBUILD .SRCINFO \
     && git commit -m "Initial import" && git push
   ```
   Users then install with `yay -S versionary-qt-git` (or any AUR helper).
4. Optional: set a notification on your GitHub repo (watch → custom →
   releases) or add co-maintainers on the AUR package page. No rebuild
   action is needed when you release — `-git` tracks `main` continuously.

## Flathub (Flatpak, all distros)

The in-repo `packaging/io.github.spuk1.Versionary.yml` builds locally, but
Flathub has two extra requirements — prepare these before submitting:

1. **Tarball source, not git.** Flathub's build servers forbid VCS
   checkouts for stable apps. After your first GitHub Release, point the
   module at it:
   ```yaml
   sources:
     - type: archive
       url: https://github.com/Spuk1/versionary-qt/archive/refs/tags/v1.0.1.tar.gz
       sha256: <sha256 of the tarball>
   ```
   (Keep the git-based manifest in this repo for local `flatpak-builder`
   testing; maintain the tarball variant for the Flathub submission.)
2. **Latest runtime.** Flathub requires the newest `org.kde.Platform` at
   submission time. Check with
   `flatpak remote-ls --runtime flathub org.kde.Platform` and bump
   `runtime-version` if needed (repo manifest currently pins `6.8`).
3. Test the exact submission manifest locally:
   ```bash
   flatpak-builder --force-clean --user --install-deps-from=flathub \
     --install build-dir packaging/io.github.spuk1.Versionary.yml
   flatpak run io.github.spuk1.Versionary
   ```
4. Submit: fork https://github.com/flathub/flathub, branch off `new-pr`,
   add your manifest (+ `flathub.json` if you need arch restrictions —
   default builds x86_64 and aarch64, which is what you want), and open
   a PR. Reviewers run automated checks (appstream/metainfo validation,
   sandbox permissions) — `finish-args` in the manifest are already
   minimal and justified, which is what they look for.
   Full process: https://docs.flathub.org/docs/for-app-authors/submission

## One caveat for both: OAuth keys

AUR and Flathub builds run on **other people's machines**, so your
`VERSIONARY_*` secrets can't be injected the way the GitHub Release
workflow does. Two options:

- **Ship without baked keys (recommended to start).** OAuth login buttons
  stay inert, but SSH, HTTPS + personal access tokens, and all local
  features work. Zero secret exposure.
- **Embed the IDs publicly.** The GitHub release binaries already ship
  these values to everyone, so putting them in the AUR `PKGBUILD` /
  Flathub manifest is no additional exposure — but it does print them in
  public build logs/packaging files, so it's a conscious choice.

The app already handles missing keys gracefully (empty value = that
provider's login disabled), so no code changes are needed either way.
