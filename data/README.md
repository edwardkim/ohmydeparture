# Static airport code reference

[`airport-codes.json`](airport-codes.json) is a checked-in snapshot of all 1,391
records returned by Korea Airports Corporation's **Airport Code Information_GW**
service on **2026-10-26**. Application builds and installations include this file
as a Qt resource at `:/data/airport-codes.json`. Reading it requires no network
connection, `.env` file or airport-code API key. No automatic refresh is performed.

Source: [한국공항공사_공항 코드 정보_GW](https://www.data.go.kr/data/15158948/openapi.do).
The portal lists its usage terms as **이용허락범위 제한 없음** (no restrictions on
use), checked on 2026-10-26. Provider data is recorded separately from this
project's source code license; see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## Format and lookup

The UTF-8 JSON object contains `schema_version`, `source`, `record_count` and
`items`. `source` records the provider, catalog, endpoint, retrieval timestamp,
usage terms and pagination parameters. Records are sorted by `cityCode`; field
names and values are preserved from the provider response.

| Field | Content |
| --- | --- |
| `cityCode` | Three-letter airport or city code; unique in this snapshot |
| `cityEnglish` | English display name |
| `cityKorean` | Korean display name |
| `cityJapan` | Provider's Japanese-name field; may contain English |
| `cityChina` | Provider's Chinese-name field; may contain English |

Join ICN flight `airportCode` to `items[].cityCode`. Default to `cityEnglish` for
the English board. Preserve the code when displaying a name: the source mixes
city names and individual airport names, and names such as `HEATHROW` are not
city names. If a future flight code is absent, retain the flight provider's name
or the original code instead of substituting a different airport.

For the 2026-10-26 ICN sample, all 137 distinct airport codes across 2,244
departure/arrival records matched this snapshot. All 1,391 reference records
have nonempty values in the five provider fields, with no duplicate codes.
The ICN provider-snapshot board uses this reference for destination names.

## Detailed airport catalog from CSV

[`airport-details.json`](airport-details.json) adds the 5,498 airport records
provided in [`sources/IATA_20251231.csv`](sources/IATA_20251231.csv). The original
CP949 CSV is preserved byte for byte. The derived UTF-8 JSON is bundled at
`:/data/airport-details.json`, with no runtime download. Its metadata includes
the source filename, encoding, SHA-256 and `date_from_filename: 2025-12-31`.
The filename supplies that date; it is not a separately verified publication date.
The provider is **Ministry of Land, Infrastructure and Transport, Republic of
Korea (대한민국 국토교통부)**, as identified by the dataset contributor. The
catalog URL and usage terms remain unspecified and are recorded as `null`.

The catalog keeps the original record order. `sourceRecord` is the one-based
data record number, excluding the header. Field mappings are:

| CSV column | JSON field |
| --- | --- |
| `영문공항명` | `nameEnglish` |
| `한글공항` | `nameKorean` |
| `공항코드1(IATA)` | `iataCode` |
| `공항코드2(ICAO)` | `icaoCode` |
| `공항운영상태구분` | `operatingStatus` |

Names and status values are preserved. All source rows are categorized as
`기존공항`; this category is retained as reference data. For lookup, only three
uppercase letters form an `iataCode`, and four uppercase letters form an
`icaoCode`. `n/a` and malformed codes become `null`, while the original values
remain available in the CSV. Malformed nonmissing values are also recorded in
`quality.invalid_codes`; they are not guessed or swapped automatically.

| Validation | Count |
| --- | ---: |
| Airport records | 5,498 |
| Distinct valid IATA codes | 5,305 |
| IATA codes assigned to multiple source records | 19 |
| Records with missing IATA (`n/a`) | 173 |
| Records with malformed IATA | 1 |
| Distinct valid ICAO codes | 4,753 |
| ICAO codes assigned to multiple source records | 8 |
| Records with missing ICAO (`n/a`) | 736 |
| Records with malformed ICAO | 1 |

`quality.duplicate_iata_codes` and `quality.duplicate_icao_codes` map each
ambiguous code to **all** its `sourceRecord` numbers. For example, `TRU` has two
different airport records. A consumer must retain all candidates and use an
independently supplied ICAO code to disambiguate when possible; otherwise retain
the original flight provider's name or code. Choosing the first or last record
would silently discard conflicting source data.

The CSV overlaps 1,240 codes in the city reference and adds 4,065 distinct IATA
codes. The city reference also has 151 codes absent from this CSV, including
city codes such as `TYO` and `LON`. Keep both catalogs: `cityEnglish` supplies a
compact destination label, while `nameEnglish` supplies a full airport name.
All 137 codes in the recorded ICN sample have exactly one matching CSV record.
Other codes with duplicate records require the handling described above.

To regenerate the JSON from the preserved CSV without network access:

```sh
python3 tools/import-airport-csv.py
python3 tests/airport-data.py
```

For another file, pass its path as the first argument and optionally specify
`--encoding`, `--provider` and `--source-url`. `--output` selects the destination
JSON. The bundled CSV retains its known provider when regenerated; other input
files require their own provider attribution. Review the quality report and
preserve the corresponding source CSV when updating the catalog. Both catalogs
are offline references used by the ICN board; other travel boards continue to use
their generated timetables.

## ICN daily departures

[`icn-departures.json`](icn-departures.json), embedded at
`:/data/icn-departures.json`, contains a complete departure-day response from
[Incheon International Airport Corporation](https://www.data.go.kr/data/15140153/openapi.do).
The first snapshot for 2026-10-26 contained 1,127 records: 540 operating `Master`
passenger flights and 587 `Slave` codeshare records. The portal lists the usage
terms as **이용허락범위 제한 없음**, checked on 2026-10-26.

The envelope records the requested KST date, timezone, fetch timestamp, source,
query parameters, page count and total record count. `items` preserves every
returned provider field and value. Only operating passenger flights are selected
for the board. Names come from the static city reference, then unambiguous
airport details; unknown codes retain their code or the provider's Korean name.
For slash-separated destination names that exceed the flap field, the board
uses the first city component; the complete source name remains in the data.
Known airlines use the original color badges; unknown carriers use their own
flight-number prefix and a stable color. Unknown carriers are never assigned
another airline's identity.

`scheduleDatetime` and `estimatedDatetime` retain their complete KST dates.
The latter is shown whenever it differs, including earlier times and changes
past midnight. Recorded statuses are translated for English; missing status
means `SCHEDULED`, rather than an inferred on-time or boarding status. The
destination-language phase falls back to English. No operating status is
calculated from the wall clock.

The board selects flights whose changed time (or scheduled time when absent)
is **at or after system time minus 30 minutes**. There is no future upper cutoff
within the downloaded day. Delayed flights use the same rule; their status does
not keep flights outside the window. The board reevaluates the existing snapshot
as the system clock advances, without downloading data. The footer identifies
the snapshot fetch time. A different calendar day
shows `REFRESH REQUIRED` with empty rows; an unreadable or invalid file shows
`ICN DATA UNAVAILABLE`. Neither case substitutes generated ICN flights.

With the per-user ICN approval and `.env` configuration described in the main
README, update the source snapshot manually:

```sh
python3 tools/fetch-icn-departures.py
make -j2
```

The default day is today in `Asia/Seoul`. Optional `--date YYYYMMDD`, `--rows`,
`--env` and `--output` select the request and destination. Every page is checked
for success, stable totals, unique identities and scheduled dates. Partial,
duplicate or inconsistent responses preserve the previous file. The service key
is read as plain data, redacted from output and never included in the bundle.
This is an explicit network operation; builds, tests and board rendering do not
fetch data. Rebuild/reinstall the package and restart a running app after updating.

## Updating the KAC city reference

Users do not need to apply for this API to use the bundled snapshot. A maintainer
who chooses to refresh it must obtain their own approval and service key for
the source API. Optional local settings are shown in [`.env.example`](../.env.example).

Request `https://apis.data.go.kr/B551178/airport-code/info` with `serviceKey`,
`type=json`, `pageNo=1` and `numOfRows=100`. Increment `pageNo` until the number
of collected records equals `body.totalCount`, checking the API result code on
every page. The initial snapshot used 14 pages (13 × 100 + 91 records).
`numOfRows=1000` returned `HTTP_ERROR` (`04`) despite HTTP status 200 during
verification, so it must not be treated as a successful empty result.

Preserve all five provider fields, reject missing or duplicate codes, sort by
`cityCode`, and update `source.retrieved_at`, `source.query` and `record_count`
with the replacement snapshot. Keep credentials and request URLs containing
keys out of the repository. Review the data diff and source terms before
publishing an update. Builds and normal application use must remain offline
for this reference.
