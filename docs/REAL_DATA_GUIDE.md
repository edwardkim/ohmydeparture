# Real data integration guide

[한국어](REAL_DATA_GUIDE.ko.md) · [Project README](../README.md)

Use this guide to obtain flight data, apply it to an airport board, and verify
what the installed application is displaying. Run source commands from the
repository root. The plugin bundle does not contain the download tools, so keep
a source checkout for these commands. Use an application built from this checkout,
including its NRT cache adapter; copying a cache cannot add support to an older binary.

Flight data updates are manual. The clock filter and flip animations operate
on downloaded data and make no API requests.

## Choose a data source

| Board | Source and coverage | Data location | How an update reaches the screen |
| --- | --- | --- | --- |
| ICN | Incheon Airport API, complete requested departure day | `data/icn-departures.json`, embedded in the binary | Download, rebuild, reinstall if using the plugin, then quit and reopen the app |
| NRT | Aviationstack, one cached response page | `~/.local/share/ohmydeparture/aviationstack/nrt.json` | Activate the cached page, then quit and reopen the app; no rebuild for later data-only changes |
| Other airport and railway boards | Generated examples | Built-in timetable data | No live-data adapter currently connected |

NRT uses generated examples until a local cache is installed. Once a cache is
installed, an invalid or stale cache produces an explicit notice instead of
silently substituting generated flights. The NRT location respects an absolute
`XDG_DATA_HOME`; its default is `~/.local/share`.

## Use the common data utility

Build the app once so that management operations use the board's actual validators:

```sh
make -j2
python3 tools/flight-data.py providers
python3 tools/flight-data.py status --airport NRT
```

The commands return JSON. `providers`, `status`, `validate` and `install` make no
API requests. `status` describes the selected binary and files, not a running
GUI's in-memory copy. Use `--app /path/to/ohmydeparture` to inspect an installed
binary instead of the source build. A missing optional cache returns state
`missing` and exit status 1.

Check and apply an existing NRT report through the common validator:

```sh
python3 tools/flight-data.py validate --airport NRT --file test-output/aviationstack-nrt-check.json
python3 tools/flight-data.py install --airport NRT --file test-output/aviationstack-nrt-check.json --dry-run
python3 tools/flight-data.py install --airport NRT --file test-output/aviationstack-nrt-check.json
```

Activation creates a backup when the active bytes change. Stale candidates are
refused by default; `--allow-stale` explicitly permits an archival replacement.
Backups are written under `~/.local/state/ohmydeparture/flight-data/backups/`
(respecting `XDG_STATE_HOME`). To restore one, validate and install its returned
backup path with the same command. Quit and reopen the GUI after activation.

To plan or perform collection, after configuring your key:

```sh
python3 tools/flight-data.py fetch --airport NRT --dry-run
python3 tools/flight-data.py fetch --airport NRT --max-requests 1
```

`fetch` reuses a matching candidate; use `--refresh` for a new request or
`--max-requests 0` for cache-only operation. It stages data without activating
it. Aviationstack remains a single page; budgets do not merge or collect extra
pages. ICN defaults to at most 10 requests to collect its complete result:

```sh
python3 tools/flight-data.py fetch --airport ICN --output test-output/icn-candidate.json --max-requests 10
python3 tools/flight-data.py install --airport ICN --file test-output/icn-candidate.json
python3 tools/flight-data.py status --airport ICN
```

If a fixed candidate filename contains another day's ICN data, explicitly use
`--refresh` to replace it. ICN `status` reports `needs_rebuild` when the source
snapshot differs from the binary. Continue with the rebuild/reinstall steps
below after activation. A failed collection or validation preserves previous
candidates and active data. The provider-specific commands below remain supported.
See [provider architecture](DATA_PROVIDERS.md) when adding another API.

## Configure your own credentials

Create `.env` from [.env.example](../.env.example) if it does not exist. Edit
an existing `.env` without overwriting other service settings. The required
settings depend on the source:

```dotenv
ICN_EP=https://apis.data.go.kr/B551177/statusOfAllFltDeOdp
ICN_KEY=YOUR_OWN_DATA_GO_KR_SERVICE_KEY
AVIATIONSTACK_KEY=YOUR_OWN_AVIATIONSTACK_ACCESS_KEY
```

Keep real keys in the local `.env`, which Git ignores. The tools read this file
as data; do not execute it with `source`. Only Aviationstack also accepts its
key from the process environment; an exported `AVIATIONSTACK_KEY` takes
precedence over the file. Both download tools accept `--env /path/to/file` for
a different configuration file. Keys are used by download tools, not by the board.

`ACODE_EP` and `ACODE_KEY` are optional settings for maintaining the static
airport-code reference. They are not flight-data credentials, and users of the
bundled airport names do not need them.

## Update the ICN departure snapshot

### Obtain portal approval

Each person downloading or refreshing ICN data needs their own
[data.go.kr account](https://www.data.go.kr/), approval for
[Detailed Flight Operation Status Inquiry](https://www.data.go.kr/data/15140153/openapi.do)
(`인천국제공항공사_항공기 운항 현황 상세 조회`), and a service key for that API.
Select **활용신청**, complete the application, and confirm approval before
using the key. A portal account or a key for another API is insufficient.
Viewing the already bundled snapshot requires no key.

### Download the current day

After setting `ICN_EP` and `ICN_KEY`, run:

```sh
python3 tools/fetch-icn-departures.py
```

The default date is today in `Asia/Seoul`. The downloader uses HTTPS and
`getFltDeparturesDeOdp`, requests the scheduled day with `searchdtCode=S`,
`searchFrom=0000` and `searchTo=2400`, and follows every page. It saves JSON to
`data/icn-departures.json` only after checking totals, unique flight identifiers
and scheduled dates. A failed or incomplete download preserves the previous file.
`fid` is the provider's returned flight identifier, not a request timestamp to
generate on the client. `--date YYYYMMDD` selects another supported service date.

An optional connection diagnostic checks departures and arrivals in JSON and
XML. It makes four requests and writes a redacted report under `test-output/`;
it does not replace the board's snapshot:

```sh
python3 tools/check-icn-api.py --time-basis S
```

### Apply the new snapshot

For a source build:

```sh
make -j2
```

Quit the running app from its gear menu, then reopen it explicitly in ICN mode:

```sh
./ohmydeparture --airport ICN --windowed
```

For an installed Omarchy plugin, follow **Update the installed plugin** below
instead. Rebuilding the repository executable alone does not update the
plugin's installed binary.

The ICN board shows `Master` passenger flights, excluding `Slave` codeshares
and cargo from the display while retaining the complete response in the JSON.
Times retain their KST dates. The footer identifies when the snapshot was fetched.

## Use an Aviationstack page in the NRT board

### Obtain a key and check the request budget

Create your own [Aviationstack account](https://aviationstack.com/signup/free)
and add its key as `AVIATIONSTACK_KEY`. The
[Free plan](https://aviationstack.com/pricing) lists 100 requests per month,
HTTPS and non-commercial use, checked on 2026-10-27. Confirm your account's
current plan and remaining usage in its dashboard.

The [provider specification](https://api.swaggerhub.com/apis/apilayer-863/AviationstackAPI/1.0.0/swagger.json)
limits pages below Professional to 100 records. A page consumes one request;
the allowance is not a limit of 100 flights.

### Download or inspect one page

```sh
python3 tools/check-aviationstack-api.py --airport NRT
```

The tool requests `https://api.aviationstack.com/v1/flights` with `dep_iata=NRT`,
`limit=100` and `offset=0`. If the report file is absent, it makes one request. A
matching cache is reused regardless of age, with its original fetch time shown
and `API calls this run: 0`. There are no automatic retries or extra pages.
The report is saved to `test-output/aviationstack-nrt-check.json` with owner-only
permissions. The current checker omits the `flight_date` parameter.
An existing invalid or mismatched report fails without a network request;
use `--refresh` explicitly to replace it.

### Activate the downloaded page

```sh
python3 tools/check-aviationstack-api.py --airport NRT --install
```

The report under `test-output/` is not automatically read by the board.
With a matching downloaded page, activation makes zero API calls. It writes a
validated, owner-only copy to the NRT data location listed above. Quit the
running app from its gear menu and reopen the intended binary:

```sh
./ohmydeparture --airport NRT --windowed
```

For an already updated plugin, use its installed launcher instead:

```sh
~/.config/omarchy/plugins/edwardkim.ohmydeparture/launch --view departures --airport NRT --windowed
```

If the installed app does not yet contain the NRT adapter, update the plugin
first. Later NRT cache changes need activation and an app restart, not a rebuild.

### Explicitly refresh the active page

```sh
python3 tools/check-aviationstack-api.py --airport NRT --refresh --install
```

This makes one new request and activates a successful response. Quit and reopen
the app afterwards. A failed request preserves the previous cache.

To inspect the next page separately:

```sh
python3 tools/check-aviationstack-api.py --airport NRT --offset 100
```

This uses a separate report file. **Pages are not merged.** Installing a page
at a different offset replaces the active page; it does not add records to it.
Only NRT currently supports `--install`.

On 2026-10-27, the first NRT request returned 100 of 1,418 matching records.
Fifty were codeshares, leaving 50 distinct non-codeshare flights in that page.
Fetching the whole matching query would require 15 requests at limit 100;
daily repetition for 30 days would require 450. These are query counts, not a
verified complete daily passenger schedule. The tool does not track calls made
by other clients or automatically collect the entire query.

### Understand the NRT preview

The board excludes codeshares and duplicate flight/time pairs but includes
cargo, because the response lacks ICN's explicit passenger/cargo classification.
It uses static airport translations, preserves reported gates and statuses,
and leaves absent gates blank. Languages cycle through English, Japanese and Korean.

The footer displays `PARTIAL`, cached/total record counts, the fetch time in
JST and **REPORTED TIMES**. The sampled timestamps contain `+00:00` alongside
`Asia/Tokyo`. This preview deliberately shows the returned date/time components
unchanged and uses them as Tokyo wall time for filtering; it does not add nine
hours. Original strings remain in the cache. Treat this as a reported-time
preview until the provider's timestamp semantics have been verified.

## Update the installed plugin

Use this after an ICN snapshot change or an application-code update. Run from
the repository root:

```sh
make package-omarchy
omd_version=$(cat VERSION)
omd_arch=$(uname -m)
python3 "dist/ohmydeparture-omarchy-${omd_version}-${omd_arch}/install.py"
```

The installer updates the plugin and keeps its existing bar settings. It also
backs up the prior installation under
`~/.local/state/ohmydeparture-omarchy/backups/`. Quit the old app from its gear
menu after installing, then launch the selected board:

```sh
~/.config/omarchy/plugins/edwardkim.ohmydeparture/launch --view departures --airport ICN --windowed
```

Replace `ICN` with `NRT` to view the local NRT cache. Launching while the old
process is still running switches that process's view; it does not load the
new binary or reread its cached data. The default view without an airport is Codex.

ICN data is embedded in the updated binary. NRT data stays in the user's data
directory, separate from the plugin. Aviationstack responses and service keys
are not included in public source releases or plugin bundles.

## Verify the data on screen

Both boards sort by scheduled time. They display flights whose estimated time,
or scheduled time when no estimate exists, is at or after **current airport
time minus 30 minutes**. There is no later-time cutoff within the loaded day;
the window shows as many rows as fit. The filter advances without API calls,
and delayed status does not exempt an old flight from the cutoff.

| What you see | Meaning or next action |
| --- | --- |
| `ICN SNAPSHOT … KST` | Check the fetch time against the snapshot you downloaded |
| `NRT AVIATIONSTACK 100/1418 PARTIAL … JST … REPORTED TIMES` | The installed response is one partial page, displayed with the preview time basis |
| `REFRESH REQUIRED` | The loaded day has passed; explicitly download and apply fresh data |
| `DATA UNAVAILABLE` | The selected data cannot be read or validated; check the file and update steps |
| No upcoming cached NRT flights | No records in this page satisfy the window; this does not establish that the airport has no departures |
| NRT still shows generated examples | Install the cache for the same Linux user and `XDG_DATA_HOME`, update an older binary, and restart the app |
| ICN still shows the old snapshot | Rebuild and reinstall the plugin, then quit the old process |
| A diagnostic returns HTTP 200 but no valid ICN data | Inspect its API result code and records; HTTP status alone is insufficient |
| Aviationstack refresh fails | Check the key, exported-variable precedence, plan and quota; a failed refresh leaves the old cache available |
| DNS or network failure | Run the download tool on a machine with working DNS and outbound HTTPS access to the provider |

Use the footer and recorded timestamps to distinguish a newly rendered screen
from newly downloaded flight information. A moving clock or flip does not mean
that provider statuses have been refreshed.

## Reference and provider development

[data/README.md](../data/README.md) describes the stored fields and provenance.
The normalized display row is [Departure.h](../platform/qt/Departure.h);
[IcnDepartures.cpp](../platform/qt/IcnDepartures.cpp) and
[AviationstackDepartures.cpp](../platform/qt/AviationstackDepartures.cpp) implement
validation and selection. [Board.cpp](../src/Board.cpp) renders their rows.

When adding another provider, define its timestamp and coverage rules before
normalizing rows. Preserve source metadata and nullable fields, map only
reported statuses, and keep credentials in the download tool. Add offline
fixtures for pagination, codeshares, unknown airports/airlines, midnight changes,
the 30-minute boundary and stale/invalid data. Respect the provider's data terms
when choosing between bundled snapshots and private user caches.

Existing provider checks run without external API requests:

```sh
make test-icn test-aviationstack
```
