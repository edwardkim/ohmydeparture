# Oh My Departure — Omarchy bundle

An English-first, read-only Codex session and agent board using a split-flap display.
Airport and railway boards are also available. Timetables are generated examples.

## Install

Extract the archive, then run **as your desktop user, without sudo**:

```sh
python3 install.py
```

Requires Omarchy with the native `bar-widget` plugin API, Python 3, `qt6-base`, and
`qt6-wayland` for Wayland. Install `noto-fonts-cjk` for travel boards and non-Latin
session text. The archive targets the CPU architecture in its filename and uses
system Qt and libc. Check `BUILD-INFO.json`; a source build is preferable if your
system libraries are older than those used to build the archive.

The installer adds `edwardkim.ohmydeparture` to the right bar section, installs
an application launcher and icon, and backs up changed files under
`~/.local/state/ohmydeparture-omarchy/backups/`. XDG base directory overrides work.
It preserves other widgets and settings, including a symlinked `shell.json`.

Managed local installations of `rainygirl.rdepartures` are migrated to the new ID,
preserving their bar position and inline settings. Foreign plugins and launchers
are left alone. Close the older app once before opening Oh My Departure. On its
first interactive launch, the app copies missing airport and flip preferences
from `rainygirl/RDepartures.conf`; the original file remains intact.

No systemd user service, session bus, or accessibility configuration is changed.
The shell watches plugin files; the installer also requests a rescan when IPC is
available. Installation errors restore files from the backup.

## Use

Click the bar icon to open or focus the board. Click a row for session details,
search, child agents, turns, and tool history. The gear menu contains
**Codex sessions & agents**, **Session details & search…**, **Display settings…**,
and travel destinations. **F11** toggles fullscreen; **Esc** closes the window.

**Display settings…** controls the repeat interval (default 3 seconds; 0 disables
replay) and flip duration (default 0.65 seconds; range 0.20–2.00). Project plaques
have stable colours; parent sessions and their child agents share a colour.

```sh
python3 install.py --section left
omarchy bar set edwardkim.ohmydeparture view departures
omarchy bar set edwardkim.ohmydeparture airport SEOUL
omarchy bar set edwardkim.ohmydeparture view codex
python3 install.py --uninstall
```

Updating by running a newer installer preserves placement and settings. Uninstall
removes the managed plugin, bar entry and launcher; preferences and backups remain.

## Data and limitations

The standard-library Python reader queries an existing Codex daemon through
`codex app-server proxy`. Otherwise it reads local JSONL history under
`$CODEX_HOME` (default `~/.codex`). No API key or model inference is required.
It does not resume agents, execute history commands, respond to approvals, or
modify Codex configuration, hooks, databases or logs.

**HISTORY** means runtime unknown; **IDLE** means loaded with no current activity.
A completed turn does not imply project completion. Live state is limited to the
connected daemon. Polling is every 5 seconds, independent of the flip interval.
Local history is partial: latest 1 MiB, at most 30 turns and 200 items per turn.
Session titles, paths, and tool outputs may be private: inspect your board before
sharing screenshots. The repository demo uses synthetic data only.

Access to real ICN flight data requires **each user** to register at
[data.go.kr](https://www.data.go.kr/), apply for
[Detailed Flight Operation Status Inquiry](https://www.data.go.kr/data/15140153/openapi.do),
receive approval, and obtain their own service key. No shared key is included.
See the [source setup guide](https://github.com/edwardkim/ohmydeparture#icn-live-data-api-prerequisites).
The v1.2.0 bundle's travel boards use generated timetables; the source-level API
checker and its `.env` setup do not yet enable live rendering in this bundle.

Source and documentation: <https://github.com/edwardkim/ohmydeparture>.
MIT source; original R Departures attribution and GUST font license are included
in the plugin directory. This is an extracted binary bundle, not an Omarchy plugin
registry URL; use this installer rather than `omarchy plugin add` on the source repo.
