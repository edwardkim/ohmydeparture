# Upstream provenance

Oh My Departure is a Linux / Omarchy derivative of
[R Departures by rainygirl](https://github.com/rainygirl/renkuos-rdeparture).
The imported baseline is upstream commit
[`0e21b22ef09e6ab0645da878aea9580637d48a24`](https://github.com/rainygirl/renkuos-rdeparture/commit/0e21b22ef09e6ab0645da878aea9580637d48a24),
plus the local Linux port and Codex board work prepared before this public release.

This repository starts with a new root commit rather than a copy of the original
shallow checkout's Git history. It is a derivative with explicit attribution;
GitHub's fork relationship is not assumed. Original copyright headers, MIT notice,
Haiku metadata and asset provenance remain intact.

The inherited parts include `src/` (renderer, travel data and station boards),
Haiku build/icon tools, `assets/rdepartures.hvif`, `docs/icon.png`, travel screenshots,
and the unmodified font. Qt-specific changes to the shared renderer are guarded
by `RDEPARTURES_QT` to keep the Haiku implementation identifiable.

The additions include the Qt compatibility backend, Linux CLI and desktop
integration, Codex session/agent view and read-only adapter, periodic animation
controls, Omarchy widget/installer, migration logic, tests and release automation.
The original Haiku target still builds as R Departures; it has not been validated
as part of the Linux release.

`upstream` may point to the original repository for reference; `origin` is reserved
for `edwardkim/ohmydeparture`. Keep attribution when importing later upstream work.
