#!/usr/bin/env python3
"""Download one complete ICN departure-day snapshot using the local service key."""
import argparse
from datetime import datetime
import importlib.util
import os
from pathlib import Path
import tempfile
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('icn_api', ROOT / 'tools/check-icn-api.py')
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)


def collect(endpoint, key, date, rows=1000, probe=None):
    probe = probe or api.probe
    items = []
    identifiers = set()
    total = None
    for page in range(1, 101):
        result = probe(endpoint, key, api.OPERATIONS['departures'], 'json', date,
                       page=page, rows=rows, search_from='0000', search_to='2400', time_basis='S')
        if not result.get('ok'):
            raise ValueError('ICN API request failed; existing snapshot was preserved')
        count = int(result['total_count'])
        if count < 0 or (total is not None and count != total):
            raise ValueError('ICN totalCount changed during pagination; retry the download')
        total = count
        batch = result['items']
        if not batch and len(items) < total:
            raise ValueError('Incomplete ICN response; existing snapshot was preserved')
        for item in batch:
            identifier = item.get('fid')
            if not isinstance(identifier, str) or not identifier or identifier in identifiers:
                raise ValueError('Missing or duplicate flight identifier in ICN response')
            scheduled = item.get('scheduleDatetime', '')
            if not isinstance(scheduled, str) or len(scheduled) != 12:
                raise ValueError('Invalid scheduled departure timestamp')
            parsed = datetime.strptime(scheduled, '%Y%m%d%H%M')
            if parsed.strftime('%Y%m%d') != date:
                raise ValueError('ICN returned a scheduled departure outside the requested day')
            identifiers.add(identifier)
            items.append(item)
        if len(items) == total:
            break
        if len(items) > total:
            raise ValueError('ICN returned more records than totalCount')
    else:
        raise ValueError('ICN pagination limit exceeded')
    return dict(schema_version=1, airport='ICN', direction='departures',
                date=datetime.strptime(date, '%Y%m%d').date().isoformat(), timezone='Asia/Seoul',
                fetched_at=datetime.now(ZoneInfo('Asia/Seoul')).isoformat(timespec='seconds'),
                source=dict(provider='Incheon International Airport Corporation',
                            catalog_url='https://www.data.go.kr/data/15140153/openapi.do',
                            usage_terms='이용허락범위 제한 없음',
                            endpoint='https://apis.data.go.kr/B551177/statusOfAllFltDeOdp/getFltDeparturesDeOdp',
                            query=dict(searchDate=date, searchFrom='0000', searchTo='2400',
                                       searchdtCode='S', type='json', numOfRows=rows), pages=page),
                record_count=len(items), items=items)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--env', type=Path, default=ROOT / '.env')
    parser.add_argument('--date', default=datetime.now(ZoneInfo('Asia/Seoul')).strftime('%Y%m%d'))
    parser.add_argument('--rows', type=int, default=1000)
    parser.add_argument('--output', type=Path, default=ROOT / 'data/icn-departures.json')
    args = parser.parse_args()
    if args.rows < 1 or args.rows > 1000:
        parser.error('--rows must be between 1 and 1000')
    temporary = None
    try:
        date = datetime.strptime(args.date, '%Y%m%d').strftime('%Y%m%d')
        values, endpoint = api.load_config(args.env)
        snapshot = collect(endpoint, values['ICN_KEY'], date, args.rows)
        text = api.redact(snapshot, values['ICN_KEY']) + '\n'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=args.output.parent,
                                         prefix='.icn-', suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(text)
        temporary.chmod(0o644)
        os.replace(temporary, args.output)
    except (OSError, ValueError, KeyError, TypeError):
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        print('Download failed: check the ICN configuration, date and API response; credentials are not logged.')
        return 1
    operating = sum(item.get('codeshare') == 'Master' and item.get('passengerOrCargo') == 'Passenger'
                    for item in snapshot['items'])
    print(f"Saved ICN {snapshot['date']}: {snapshot['record_count']} records, "
          f"{operating} operating passenger flights; fetched {snapshot['fetched_at']}")
    print(f'Output: {args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
