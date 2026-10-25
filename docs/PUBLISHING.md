# Publishing Oh My Departure

Target: **edwardkim/ohmydeparture**, public, default branch **main**.
Default language: **English**. Initial release: **v1.2.0**.

Preparing a local repository or release bundle does not create or publish the
GitHub repository. Run the remote commands below only when publication is intended.

## Local release checks

1. Confirm `VERSION`, the plugin manifest and release notes match. Use synthetic
   preview data; retain the original MIT notice, UPSTREAM.md and font license.
2. Run `make test`, `make test-omarchy`, and the native GUI checks in CONTRIBUTING.md.
3. Review the staged file list. Exclude personal Codex logs, authentication files,
   private screenshots, generated binaries, build directories and installation backups.
4. Commit the reviewed source and tag that exact commit as `v1.2.0`.
5. Run `make package-omarchy` from the clean tagged tree. Inspect BUILD-INFO.json
   (sourceCommit should match the tag and sourceModified should be false).
6. Optional local source/mirror artifacts:

```sh
python3 tools/prepare-release.py
```

This creates a source tarball, full Git bundle, SHA256SUMS and a machine-readable
release manifest in the ignored `dist/` directory. The native binary archive is
architecture-specific and dynamically linked; it is not a universal Linux binary.
The Ubuntu GitHub build produces its own binary archive after the first push.

## First publication

The GitHub CLI must be authenticated as an account allowed to create the target.
The following commands create a public remote and push the prepared source:

```sh
gh repo create edwardkim/ohmydeparture --public --source=. --remote=origin --push --description 'Split-flap Codex session and agent board for Linux and Omarchy'
git push origin v1.2.0
```

Pushing the tag starts the release workflow. The build job has read permission;
only the tag-triggered release job gets contents-write permission. It publishes
the CI-built binary archive and SHA-256 file, with the prepared release notes.
GitHub supplies source archives automatically. The local full Git bundle is a
backup; it need not be uploaded.

Inspect both the main-branch and tag workflow results before announcing the
release. If Actions is disabled or checks fail, resolve that before publication
of a release asset; local passing checks do not establish remote CI success.

If the repository already exists, inspect its contents and access first, then add
its HTTPS URL as `origin` rather than blindly creating or overwriting anything.
Never force-push over another history. Keep `upstream` pointed at the original
R Departures repository, and retain the provenance documentation.

For later releases, update VERSION, manifest version, reader client version,
Branding.h's fallback version, changelog, release notes and versioned examples;
then run the same checks and tag the new reviewed commit.
