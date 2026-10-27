#!/usr/bin/env python3
"""Inspect one Aviationstack flight page; reuse a local cache unless refreshed."""
import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import Request, build_opener
from flightdata.common import NoRedirect, atomic_write, data_home, json_text, read_env, redact

ROOT = Path(__file__).resolve().parent.parent
ENDPOINT = 'https://api.aviationstack.com/v1/flights'
MAX_BYTES = 4 * 1024 * 1024


def load_key(path):
    key = os.environ.get('AVIATIONSTACK_KEY', '')
    if not key:
        key = read_env(path, {'AVIATIONSTACK_KEY'}).get('AVIATIONSTACK_KEY', '')
    if not key or any(c.isspace() for c in key):
        raise ValueError('Missing or invalid key setting')
    return key


def request_page(key, query):
    # One request, without redirects, retries or automatic pagination.
    url = ENDPOINT + '?' + urlencode(dict(query, access_key=key))
    request = Request(url, headers={'Accept': 'application/json', 'User-Agent': 'OhMyDeparture/1.2.0'})
    try:
        response = build_opener(NoRedirect()).open(request, timeout=30)
    except HTTPError as error:
        response = error
    with response:
        raw = response.read(MAX_BYTES + 1)
        if len(raw) > MAX_BYTES:
            raise ValueError('Oversized response')
        return response.status, raw


def validate(report, query):
    if report.get('endpoint') != ENDPOINT or report.get('query') != query:
        raise ValueError('Cache query mismatch')
    payload = report.get('response')
    if (not isinstance(payload, dict) or payload.get('error')
            or report.get('http_status') != 200):
        raise ValueError('Unsuccessful provider response')
    pagination, data = payload.get('pagination'), payload.get('data')
    if not isinstance(pagination, dict) or not isinstance(data, list):
        raise ValueError('Missing pagination or data')
    for field in ('limit', 'offset', 'count', 'total'):
        if type(pagination.get(field)) is not int or pagination[field] < 0:
            raise ValueError('Invalid pagination')
    if (pagination['limit'] < 1 or pagination['limit'] > query['limit']
            or pagination['offset'] != query['offset'] or pagination['count'] != len(data)
            or len(data) > pagination['limit']
            or pagination['total'] < pagination['offset'] + len(data)):
        raise ValueError('Inconsistent pagination')
    for row in data:
        if not isinstance(row, dict) or not isinstance(row.get('departure'), dict):
            raise ValueError('Invalid flight record')
        if row['departure'].get('iata') != query['dep_iata']:
            raise ValueError('Departure filter mismatch')
    datetime.fromisoformat(report['fetched_at'])
    return report


def collect(key, query, transport=None):
    status, raw = (transport or request_page)(key, query)
    text = raw.decode('utf-8-sig')
    text = redact(text, key)
    report = dict(schema_version=1, provider='aviationstack', endpoint=ENDPOINT,
                  query=query, attempts=1, http_status=status,
                  fetched_at=datetime.now(timezone.utc).isoformat(timespec='seconds'),
                  response=json.loads(text))
    return validate(report, query)


def save(report, output):
    atomic_write(output, json_text(report), prefix='.aviationstack-')


def summary(report):
    payload = report['response']
    page, data = payload['pagination'], payload['data']
    return dict(airport=report['query']['dep_iata'], fetched_at=report['fetched_at'],
                returned=page['count'], total=page['total'], offset=page['offset'],
                partial=page['offset'] != 0 or page['count'] != page['total'],
                pages_at_this_limit=math.ceil(page['total'] / page['limit']),
                flight_dates=sorted({str(row.get('flight_date')) for row in data}),
                codeshares=sum(bool((row.get('flight') or {}).get('codeshared')) for row in data),
                departure_field_coverage={field: sum(bool(row['departure'].get(field)) for row in data)
                                          for field in ('scheduled', 'estimated', 'actual', 'terminal', 'gate')})


def activate(report):
    if report['query']['dep_iata'] != 'NRT':
        raise ValueError('Only the NRT board currently supports this cache')
    target = data_home() / 'ohmydeparture/aviationstack/nrt.json'
    installed = dict(report, schema_version=1, display_time_basis='reported_wall_time')
    save(installed, target)
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--env', type=Path, default=ROOT / '.env')
    parser.add_argument('--airport', default='NRT', type=str.upper)
    parser.add_argument('--limit', type=int, default=100)
    parser.add_argument('--offset', type=int, default=0)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--refresh', action='store_true', help='Make one HTTPS request instead of reusing the cache')
    parser.add_argument('--install', action='store_true', help='Use the cached NRT page in this user\'s board; restart the app afterwards')
    args = parser.parse_args(argv)
    if not re.fullmatch(r'[A-Z]{3}', args.airport):
        parser.error('--airport must be a three-letter IATA code')
    if not 1 <= args.limit <= 100 or args.offset < 0:
        parser.error('--limit must be 1–100 and --offset must be nonnegative')
    if args.install and args.airport != 'NRT':
        parser.error('--install currently supports NRT only')
    suffix = f'-offset-{args.offset}' if args.offset else ''
    output = args.output or ROOT / f'test-output/aviationstack-{args.airport.lower()}{suffix}-check.json'
    query = dict(dep_iata=args.airport, limit=args.limit, offset=args.offset)
    calls = 0
    try:
        if output.exists() and not args.refresh:
            report = validate(json.loads(output.read_text()), query)
        else:
            key = load_key(args.env)
            calls = 1
            report = collect(key, query)
            save(report, output)
        if args.install:
            print(f'Installed local NRT preview: {activate(report)}')
        print(json.dumps(summary(report), ensure_ascii=False, indent=2))
        print(f'API calls this run: {calls}; cache: {output}')
        return 0
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        print(f'Check failed after {calls} API call attempt(s); existing cache was preserved. '
              'Check the configuration, plan and response, or use --refresh for a mismatched cache. '
              'Credentials are not logged.')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
