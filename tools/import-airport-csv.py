#!/usr/bin/env python3
"""Convert a local airport CSV to an offline JSON catalog; never fetch data."""
import argparse
import csv
from datetime import datetime
import hashlib
import io
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
BUNDLED_CSV = ROOT / 'data/sources/IATA_20251231.csv'
BUNDLED_PROVIDER = 'Ministry of Land, Infrastructure and Transport, Republic of Korea'
FIELDS = ('영문공항명', '한글공항', '공항코드1(IATA)', '공항코드2(ICAO)', '공항운영상태구분')


def convert(raw, filename, encoding, *, provider=None, source_url=None):
    reader = csv.DictReader(io.StringIO(raw.decode(encoding), newline=''))
    if reader.fieldnames != list(FIELDS):
        raise ValueError('Unexpected CSV columns')
    match = re.fullmatch(r'IATA_(\d{8})\.csv', filename)
    date = datetime.strptime(match[1], '%Y%m%d').date().isoformat() if match else None
    items = []
    indexes = {'iataCode': {}, 'icaoCode': {}}
    missing = {'iataCode': 0, 'icaoCode': 0}
    invalid = []
    statuses = {}
    for number, row in enumerate(reader, 1):
        if set(row) != set(FIELDS) or any(value is None or not value.strip() for value in row.values()):
            raise ValueError(f'Missing or extra CSV fields in record {number}')
        item = dict(sourceRecord=number, nameEnglish=row[FIELDS[0]], nameKorean=row[FIELDS[1]],
                    operatingStatus=row[FIELDS[4]])
        for field, column, length in (('iataCode', FIELDS[2], 3), ('icaoCode', FIELDS[3], 4)):
            value = row[column].strip()
            code = value if re.fullmatch(f'[A-Z]{{{length}}}', value) else None
            item[field] = code
            if code:
                indexes[field].setdefault(code, []).append(number)
            elif value.lower() == 'n/a':
                missing[field] += 1
            else:
                invalid.append(dict(sourceRecord=number, field=field, sourceValue=row[column]))
        items.append(item)
        statuses[item['operatingStatus']] = statuses.get(item['operatingStatus'], 0) + 1
    if not items:
        raise ValueError('CSV has no airport records')
    duplicates = {field: {code: records for code, records in sorted(index.items()) if len(records) > 1}
                  for field, index in indexes.items()}
    quality = dict(unique_iata_codes=len(indexes['iataCode']), unique_icao_codes=len(indexes['icaoCode']),
                   missing_iata_records=missing['iataCode'], missing_icao_records=missing['icaoCode'],
                   invalid_codes=invalid, duplicate_iata_codes=duplicates['iataCode'],
                   duplicate_icao_codes=duplicates['icaoCode'], operating_status_counts=statuses)
    source = dict(filename=filename, encoding=encoding, sha256=hashlib.sha256(raw).hexdigest(),
                  date_from_filename=date, provider=provider, catalog_url=source_url, usage_terms=None,
                  columns=list(FIELDS))
    return dict(schema_version=1, source=source, record_count=len(items), quality=quality, items=items)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', nargs='?', type=Path, default=BUNDLED_CSV)
    parser.add_argument('--encoding', default='cp949')
    parser.add_argument('--provider', help='Original provider, when known')
    parser.add_argument('--source-url', help='Original catalog URL, when known')
    parser.add_argument('--output', type=Path, default=ROOT / 'data/airport-details.json')
    args = parser.parse_args()
    if args.csv.resolve() == args.output.resolve():
        parser.error('Output must not overwrite the input CSV')
    provider = args.provider
    if provider is None and args.csv.resolve() == BUNDLED_CSV.resolve():
        provider = BUNDLED_PROVIDER
    try:
        catalog = convert(args.csv.read_bytes(), args.csv.name, args.encoding,
                          provider=provider, source_url=args.source_url)
    except (OSError, ValueError, LookupError, csv.Error) as error:
        parser.exit(2, f'CSV conversion failed: {error}\n')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    quality = catalog['quality']
    print(f"Prepared {catalog['record_count']} airport records, {quality['unique_iata_codes']} unique IATA codes, "
          f"{len(quality['duplicate_iata_codes'])} ambiguous IATA codes; output: {args.output}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
