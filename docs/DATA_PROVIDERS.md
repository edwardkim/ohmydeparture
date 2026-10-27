# Departure provider architecture

[User guide](REAL_DATA_GUIDE.md) · [한국어 사용자 안내](REAL_DATA_GUIDE.ko.md)

The application consumes normalized departure rows. Provider adapters validate
their own formats and expose metadata; Python tools handle explicit network
requests and activation. A new API integration does not need an airport branch
inside the flap renderer.

```mermaid
flowchart LR
    CLI[flight-data.py] --> Download[Python download adapter]
    Download --> Candidate[Candidate JSON]
    Candidate --> Validate[Qt provider validator]
    Validate --> Activate[Atomic activation and backup]
    Activate --> Storage[Embedded snapshot or private cache]
    Storage --> Registry[DepartureSources registry]
    Registry --> Rows[Departure rows]
    Rows --> Board[Split-flap board]
    CLI --> Validate
```

## Application components

| Component | Responsibility |
| --- | --- |
| [Departure.h](../platform/qt/Departure.h) | Normalized fields, translations, scheduled/estimated times and display tone |
| [DepartureProvider.h](../platform/qt/DepartureProvider.h) | Provider interface and inspectable metadata |
| [DepartureSources.cpp](../platform/qt/DepartureSources.cpp) | Adapter registration and lazy loading; the only application registry |
| [DepartureSupport.cpp](../platform/qt/DepartureSupport.cpp) | Shared airport reference, stable ordering and the 30-minute window |
| [IcnDepartures.cpp](../platform/qt/IcnDepartures.cpp) | Complete ICN snapshot validation and passenger/master selection |
| [AviationstackDepartures.cpp](../platform/qt/AviationstackDepartures.cpp) | Partial cached-page validation, codeshare selection and reported-time preview |
| [Board.cpp](../src/Board.cpp) | Rendering and interaction using the common interface |

Implement `refresh`, `load`, `configured`, `visibleRows`, `notice` and `info`
when adding an adapter. Keep a `loadData` entry point for offline fixtures.
Register it with `DepartureSources::add` for a supported airport code. Declare
the provider ID, timezone, language cycle, storage path and time basis in `info`.
The renderer uses those values instead of checking whether the airport is ICN
or NRT. Adding a new airport to the travel menu still requires its normal entry
in `src/Data.cpp`.

An absent optional cache returns `configured=false`, allowing the existing
generated example. A present invalid cache remains configured and reports an
error with empty rows. A stale day is reported separately. Ordinary rendering
reuses loaded data; a running app must be restarted after activation.

Provider-specific parsing stays in the adapter. The common reference resolves
unambiguous airport names; adapters choose their provider-name fallbacks.
The common window uses an estimate when present, otherwise the scheduled time,
and includes the exact 30-minute boundary. It never fetches data or invents
operating statuses. ICN and NRT retain their distinct passenger/cargo and time
interpretation rules.

## Headless validation contract

```sh
./ohmydeparture --data-providers
./ohmydeparture --data-status --airport NRT
./ohmydeparture --data-status --airport ICN --data-file data/icn-departures.json
```

These commands use the offscreen Qt backend and return JSON without opening a
window, forwarding a UI request, reading Codex histories or writing preferences.
Provider listing returns static capabilities. Status loads active data; a
`--data-file` override validates that file through the same adapter as the board.
Exit status is 0 for valid data, including structurally valid stale data, 1 for
missing/invalid data, and 2 for unsupported command usage.

| Field | Meaning |
| --- | --- |
| `state` | `ready`, `stale`, `invalid` or `missing` |
| `record_count`, `total_count`, `offset`, `partial` | Coverage of the provider response, before display filtering |
| `display_count` | Normalized rows after codeshare/cargo/deduplication rules, before clock filtering |
| `visible_count` | Rows eligible under the current clock filter; the actual window may fit fewer |
| `date`, `fetched_at`, `timezone`, `time_basis` | Loaded day, provenance and interpretation of times |
| `sha256` | Fingerprint of the validated stored bytes |
| `notice`, `error` | Display notice and validation outcome; no raw credential-bearing errors |

The management utility validates a normalized temporary candidate. Its
`normalized_sha256` identifies those prospective installed bytes, which may
differ from an older diagnostic envelope or JSON formatting. Status inspection
uses the selected binary, not the in-memory state of an already running GUI.

## Management utility

[tools/flight-data.py](../tools/flight-data.py) delegates to:

| Module | Responsibility |
| --- | --- |
| [flightdata/common.py](../tools/flightdata/common.py) | Plain-data configuration, redaction, credential rejection, atomic storage and request budgets |
| [flightdata/providers.py](../tools/flightdata/providers.py) | Download adapters and candidate preparation |
| [flightdata/cli.py](../tools/flightdata/cli.py) | Discovery, status, validation, fetching, activation and backups |

The Qt registry supplies the airport/provider profiles. Implement a
`DownloadAdapter` and register it in `ADAPTERS` when introducing another vendor.
Its page limits, default request budget and date/offset capabilities are read
by the common CLI, so vendor-specific branches are not needed there.
The existing ICN and Aviationstack entry points continue to work and reuse
common configuration and storage helpers.

`fetch` writes a candidate only after successful collection and Qt validation.
It never activates that candidate. Matching cached candidates are reused unless
`--refresh` is explicit. A missing cache with `--max-requests 0` fails without
reading credentials or sending a request. The default upper bounds are 10
attempts for complete ICN pagination and 1 for the single Aviationstack page.
Increasing a budget does not enable Aviationstack pagination or page merging.

`install` rejects credential-bearing snapshots, validates through the selected
binary, and refuses stale data unless `--allow-stale` is explicit. It creates an
owner-only backup before an atomic replacement. Identical bytes produce no
write or extra backup. A validation or write failure preserves the last good
active file. `--dry-run` validates and reports the intended activation without
changing the active file or making a backup.

ICN activation updates the source snapshot and requires rebuilding/reinstalling
the binary. `status` compares that source fingerprint with the selected binary's
embedded fingerprint and reports `needs_rebuild`. NRT activation updates the
private cache and requires only an app restart. Real Aviationstack responses
remain outside source releases and bundles.

## Extending and checking a provider

Keep original response fields and collection metadata. Define complete versus
partial coverage and an explicit time basis before implementing row selection.
Use nullable gates and unknown-airline fallbacks instead of synthesizing facts.
Account for every request before sending it, and stop without publishing an
incomplete candidate when the budget is exhausted.

Add fixtures for invalid formats, changing pagination totals, duplicate IDs,
codeshares, missing fields, timezone/date boundaries and stale data. Exercise
the adapter through the registry and the headless validator, then test staging,
activation, backups and failure preservation with mocked transports. Do not use
personal API keys in tests or require external API access in CI.

```sh
make test-data test-icn test-aviationstack
```

The complete regression suite and package lifecycle checks are:

```sh
make -j2 test
make test-omarchy
```
