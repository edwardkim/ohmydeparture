#!/usr/bin/env python3
"""Check the configured ICN API in JSON and XML without logging credentials.

Reads ICN_EP and ICN_KEY from .env as plain data; never executes the file.
Results contain response metadata and schema fields, not raw payloads or keys.
"""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import shlex
import socket
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qsl, unquote, urlencode, urlsplit, urlunsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener
import xml.etree.ElementTree as ET
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parent.parent
OPERATIONS = {'departures': 'getFltDeparturesDeOdp', 'arrivals': 'getFltArrivalsDeOdp'}
MAX_BYTES = 4 * 1024 * 1024


def load_config(path):
    values = {}
    for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
        match = re.match(r'^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$', line)
        if not match:
            continue
        try:
            parts = shlex.split(match[2], comments=True)
        except ValueError:
            raise ValueError(f'Invalid quoting in environment file at line {number}') from None
        if match[1] in ('ICN_EP', 'ICN_KEY'):
            if len(parts) != 1 or not parts[0]:
                raise ValueError(f'{match[1]} must have one nonempty value')
            values[match[1]] = parts[0]
    if not all(values.get(name) for name in ('ICN_EP', 'ICN_KEY')):
        raise ValueError('Configure ICN_EP and ICN_KEY in the environment file')
    endpoint = urlsplit(values['ICN_EP'])
    if (endpoint.scheme != 'https' or endpoint.hostname != 'apis.data.go.kr'
            or endpoint.port not in (None, 443) or endpoint.username or endpoint.password or endpoint.fragment):
        raise ValueError('ICN_EP must be an HTTPS endpoint on apis.data.go.kr')
    path = endpoint.path.rstrip('/')
    for operation in OPERATIONS.values():
        if path.endswith('/' + operation):
            path = path[:-(len(operation) + 1)]
    if path != '/B551177/statusOfAllFltDeOdp':
        raise ValueError('ICN_EP must select the statusOfAllFltDeOdp service')
    return values, endpoint._replace(path=path)


def as_items(value):
    if isinstance(value, dict) and 'item' in value:
        value = value['item']
    if isinstance(value, list):
        return [item for item in value if isinstance(item, dict)]
    return [value] if isinstance(value, dict) and value else []


def describe_payload(payload):
    text = payload.decode('utf-8-sig')
    if text.lstrip().startswith('<'):
        root = ET.fromstring(text)
        # Allow a namespace without depending on a particular prefix.
        for element in root.iter():
            element.tag = element.tag.rsplit('}', 1)[-1]
        gateway = root.find('.//cmmMsgHeader')
        if gateway is not None:
            return dict(format='xml', result_code=gateway.findtext('returnReasonCode'),
                        result_message=gateway.findtext('returnAuthMsg') or gateway.findtext('errMsg'),
                        total_count=None, returned_items=0, item_fields=[])
        body = root.find('body')
        if body is None:
            body = root.find('.//body')
        items = [] if body is None else body.findall('./items/item')
        return dict(format='xml', result_code=root.findtext('.//header/resultCode'),
                    result_message=root.findtext('.//header/resultMsg'),
                    total_count=None if body is None else body.findtext('totalCount'),
                    page_no=None if body is None else body.findtext('pageNo'),
                    page_size=None if body is None else body.findtext('numOfRows'),
                    returned_items=len(items), item_fields=sorted({e.tag for item in items for e in item}))
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError('Expected a JSON response object')
    envelope = value.get('response', value)
    if not isinstance(envelope, dict):
        raise ValueError('Expected a response object')
    header = envelope.get('header') or {}
    body = envelope.get('body') or {}
    if not isinstance(header, dict) or not isinstance(body, dict):
        raise ValueError('Expected header and body objects')
    items = as_items(body.get('items'))
    return dict(format='json', result_code=header.get('resultCode'), result_message=header.get('resultMsg'),
                total_count=body.get('totalCount'), returned_items=len(items),
                page_no=body.get('pageNo'), page_size=body.get('numOfRows'),
                item_fields=sorted({key for item in items for key in item}))


class NoRedirects(HTTPRedirectHandler):
    # Never forward a credential-bearing URL to another destination.
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def probe(endpoint, key, operation, format_name, date, *, page=1, rows=10,
          search_from='0000', search_to='2400', time_basis=None):
    query = {name: value for name, value in parse_qsl(endpoint.query) if name.lower() != 'servicekey'}
    query.update(serviceKey=unquote(key), pageNo=str(page), numOfRows=str(rows), type=format_name,
                 searchDate=date, searchFrom=search_from, searchTo=search_to)
    if time_basis is not None:
        query['searchdtCode'] = time_basis
    url = urlunsplit(endpoint._replace(path=endpoint.path + '/' + operation, query=urlencode(query)))
    request = Request(url, headers={'Accept': 'application/json' if format_name == 'json' else 'application/xml',
                                    'User-Agent': 'OhMyDeparture/1.2.0 API verification'})
    result = dict(operation=operation, requested_format=format_name, ok=False)
    started = time.monotonic()
    try:
        try:
            response = build_opener(NoRedirects()).open(request, timeout=20)
        except HTTPError as error:
            response = error
        with response:
            result.update(http_status=response.code, content_type=response.headers.get('Content-Type'))
            payload = response.read(MAX_BYTES + 1)
        result['bytes'] = len(payload)
        if len(payload) > MAX_BYTES:
            raise ValueError('Response exceeds the 4 MiB inspection limit')
        result.update(describe_payload(payload))
        result['ok'] = (result['http_status'] == 200 and result['format'] == format_name
                        and str(result['result_code']) in ('00', '0'))
    except URLError as error:
        result['error'] = 'DNS_LOOKUP_FAILED' if isinstance(error.reason, socket.gaierror) else 'NETWORK_ERROR'
    except (TimeoutError, OSError):
        result['error'] = 'NETWORK_ERROR'
    except (ValueError, UnicodeError, ET.ParseError):
        result['error'] = 'UNRECOGNIZED_RESPONSE'
    result['elapsed_seconds'] = round(time.monotonic() - started, 3)
    return result


def redact(report, key):
    # Defense in depth if the service ever echoes a key in its error message.
    text = json.dumps(report, ensure_ascii=False, indent=2)
    for value in sorted({key, unquote(key), urlencode({'serviceKey': unquote(key)}).split('=', 1)[1]}, key=len, reverse=True):
        if value:
            text = text.replace(value, '[REDACTED]')
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--env', type=Path, default=ROOT / '.env', help='Plain-data environment file')
    parser.add_argument('--date', default=datetime.now(ZoneInfo('Asia/Seoul')).strftime('%Y%m%d'), help='ICN local date, YYYYMMDD')
    parser.add_argument('--page', type=int, default=1, help='pageNo (default: 1)')
    parser.add_argument('--rows', type=int, default=10, help='numOfRows (default: 10)')
    parser.add_argument('--from-time', default='0000', help='searchFrom, HHMM (default: 0000)')
    parser.add_argument('--to-time', default='2400', help='searchTo, HHMM (default: 2400)')
    parser.add_argument('--time-basis', choices=('S', 'E'), help='searchdtCode: scheduled / changed time; omitted uses the API default')
    parser.add_argument('--http', action='store_true', help='Use HTTP for the provider endpoint during this check')
    parser.add_argument('--output', type=Path, default=ROOT / 'test-output/icn-api-check.json')
    args = parser.parse_args()
    def valid_time(value, allow_end=False):
        return value == '2400' and allow_end or bool(re.fullmatch(r'(?:[01][0-9]|2[0-3])[0-5][0-9]', value))
    if args.page < 1 or args.rows < 1:
        parser.error('--page and --rows must be positive')
    if (not valid_time(args.from_time) or not valid_time(args.to_time, True)
            or args.from_time > args.to_time):
        parser.error('Use an ordered HHMM range; 2400 is accepted only for --to-time')
    try:
        date = datetime.strptime(args.date, '%Y%m%d').strftime('%Y%m%d')
        values, endpoint = load_config(args.env)
    except (OSError, ValueError):
        print('Configuration failed: check ICN_EP, ICN_KEY, file quoting and --date; values are not logged.', file=sys.stderr)
        return 2
    if args.http:
        endpoint = endpoint._replace(scheme='http')
    results = []
    for operation in OPERATIONS.values():
        for format_name in ('json', 'xml'):
            result = probe(endpoint, values['ICN_KEY'], operation, format_name, date,
                           page=args.page, rows=args.rows, search_from=args.from_time,
                           search_to=args.to_time, time_basis=args.time_basis)
            results.append(result)
            # A DNS failure cannot validate any operation or credential. Stop here.
            if result.get('error') == 'DNS_LOOKUP_FAILED':
                break
        if results[-1].get('error') == 'DNS_LOOKUP_FAILED':
            break
    report = dict(service='ICN detailed flight operation status', endpoint_scheme=endpoint.scheme, endpoint_host=endpoint.hostname,
                  checked_at=datetime.now(ZoneInfo('Asia/Seoul')).isoformat(timespec='seconds'),
                  query_date=date, requested_page_size=args.rows,
                  query_parameters=dict(pageNo=args.page, numOfRows=args.rows, searchDate=date,
                                        searchFrom=args.from_time, searchTo=args.to_time,
                                        **({'searchdtCode': args.time_basis} if args.time_basis else {})),
                  planned_checks=4, completed_checks=len(results), all_passed=len(results) == 4 and all(r['ok'] for r in results),
                  checks=results)
    output = redact(report, values['ICN_KEY'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output + '\n', encoding='utf-8')
    print(output)
    return 0 if report['all_passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
