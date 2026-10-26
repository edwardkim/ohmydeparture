# Third-party notices

Oh My Departure is derived from [R Departures](https://github.com/rainygirl/renkuos-rdeparture)
by rainygirl. The split-flap renderer, travel boards, timetable generation, original
icon, and bundled travel screenshots originate there. The original MIT copyright
notice is retained in [LICENSE](LICENSE). See [UPSTREAM.md](UPSTREAM.md) for provenance.

The unmodified **TeX Gyre Heros Cn Bold** font (`fonts/texgyreheroscn-bold.otf`) is
provided under the **GUST Font License**. Its license is included in [fonts/GUST-FONT-LICENSE.txt](fonts/GUST-FONT-LICENSE.txt).

Qt 6 is a system dependency, not redistributed by this bundle. Qt is available
under commercial and open-source licenses; see <https://www.qt.io/licensing/>.
Omarchy, Quickshell, and Codex are separately installed applications and are not
bundled. Oh My Departure is an independent community project, with no affiliation
or endorsement implied by those names.

The bundled airport code reference (`data/airport-codes.json`, embedded at
`:/data/airport-codes.json`) is a 2026-10-26 snapshot supplied by **Korea Airports
Corporation**, through [Airport Code Information_GW](https://www.data.go.kr/data/15158948/openapi.do)
(`한국공항공사_공항 코드 정보_GW`). The source portal lists its usage terms as
**이용허락범위 제한 없음** (no restrictions on use), checked on that date. These
provider records retain their source terms; this project's MIT license applies
to its code. Retrieval metadata is included in the embedded JSON; the source
checkout also provides format and update notes in [data/README.md](data/README.md).

The detailed airport catalog (`data/airport-details.json`, embedded at
`:/data/airport-details.json`) is derived from the user-supplied
`IATA_20251231.csv`, attributed to **Ministry of Land, Infrastructure and
Transport, Republic of Korea (대한민국 국토교통부)** by the dataset contributor.
The original CSV is preserved in `data/sources/` in the source checkout, and its
SHA-256 is recorded in the derived JSON. The catalog URL and usage terms are
recorded as unspecified. The project's source code license does not assign
a license to those provider records.

The ICN departure snapshot (`data/icn-departures.json`, embedded at
`:/data/icn-departures.json`) is supplied by **Incheon International Airport
Corporation**, through [Detailed Flight Operation Status Inquiry](https://www.data.go.kr/data/15140153/openapi.do).
The source portal lists its usage terms as **이용허락범위 제한 없음** (no restrictions
on use), checked on 2026-10-26. The JSON retains the original returned records
and their query/fetch metadata. Board translations and row selection are display
transformations. Service keys are excluded from the snapshot and package.
