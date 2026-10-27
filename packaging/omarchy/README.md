# Oh My Departure — Omarchy bundle

An English-first, read-only Codex session and agent board using a split-flap display.
Airport and railway boards are also available. ICN uses a dated provider snapshot;
other travel timetables are generated examples.

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

Click the bar icon to open or focus the board. Double-click a session or child
agent row for its split-flap recent history in the same window. **← BACK** or
the keyboard **Left arrow** returns to the board; **Right arrow / Space** pages
through history. **Esc** returns from history and closes the main session board.
The gear menu contains **Codex sessions & agents**, **Session browser & search…**,
**Display settings…**, and travel destinations. The browser offers search,
archived sessions, older turns and full recorded item text. **F11** toggles fullscreen.

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

ICN displays the bundled departure snapshot with its fetch time in KST. Operating
passenger flights appear once; codeshare duplicates are excluded from the board.
The snapshot does not refresh automatically. An old day displays `REFRESH REQUIRED`
with empty rows. During its day, the visible window follows the system clock:
changed time (or scheduled time if absent) at or after 30 minutes ago, including
all later flights that fit the board. This filters existing data without API calls.
Other airport and railway boards use generated timetables.

Downloading or refreshing ICN flight data requires **each user** to register at
[data.go.kr](https://www.data.go.kr/), apply for
[Detailed Flight Operation Status Inquiry](https://www.data.go.kr/data/15140153/openapi.do),
receive approval, and obtain their own service key. No shared key is included.
See the [source setup guide](https://github.com/edwardkim/ohmydeparture#icn-live-data-api-prerequisites).
Viewing the bundled snapshot requires no key. From a source checkout, run
`python3 tools/fetch-icn-departures.py`, rebuild with `make package-omarchy`, and
run the new installer to update the embedded data. Restart an already running app
after updating. The bundle excludes `.env` and all service keys.

Source and documentation: <https://github.com/edwardkim/ohmydeparture>.
MIT source; original R Departures attribution, SIL OFL 1.1 and GUST font licenses are included
in the plugin directory. This is an extracted binary bundle, not an Omarchy plugin
registry URL; use this installer rather than `omarchy plugin add` on the source repo.
