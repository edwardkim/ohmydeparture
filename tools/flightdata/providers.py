"""Download adapters. The Qt registry supplies airport and storage profiles."""
from datetime import datetime
from functools import lru_cache
import importlib.util
import json
from pathlib import Path
from zoneinfo import ZoneInfo
from .common import DataError, reject_credentials

ROOT = Path(__file__).resolve().parents[2]


class DownloadAdapter:
    default_limit = maximum_limit = 100
    default_budget = 1
    supports_date = False
    supports_offset = True
    script = ''

    def prepare(self, profile, report):
        if not isinstance(report, dict):
            raise DataError('Expected a snapshot object')
        reject_credentials(report)
        return dict(report)

    def request_date(self, profile, value=None):
        if value and not self.supports_date:
            raise DataError('This live-feed adapter does not support a date query')
        if not value:
            return datetime.now(ZoneInfo(profile['timezone'])).strftime('%Y%m%d')
        try:
            return datetime.strptime(value.replace('-', ''), '%Y%m%d').strftime('%Y%m%d')
        except ValueError:
            raise DataError('Date must be YYYY-MM-DD or YYYYMMDD') from None

    def candidate_path(self, profile, date, offset):
        raise NotImplementedError

    def check_query(self, profile, report, date, limit, offset):
        raise NotImplementedError

    def fetch(self, profile, env, date, limit, offset, budget):
        raise NotImplementedError


class IcnAdapter(DownloadAdapter):
    script = 'fetch-icn-departures.py'
    default_limit = maximum_limit = 1000
    default_budget = 10
    supports_date = True
    supports_offset = False

    def candidate_path(self, profile, date, offset):
        return ROOT / f'test-output/flight-data/icn-{date}.json'

    def check_query(self, profile, report, date, limit, offset):
        if report.get('date', '').replace('-', '') != date or report.get('airport') != profile['airport']:
            raise DataError('Cached snapshot date or airport differs; choose another file or use --refresh')

    def fetch(self, profile, env, date, limit, offset, budget):
        module = legacy(profile['provider'])
        values, endpoint = module.api.load_config(env)
        result = module.collect(endpoint, values['ICN_KEY'], date, rows=limit,
            probe=lambda *a, **kw: budget.call(module.api.probe, *a, **kw))
        return json.loads(module.api.redact(result, values['ICN_KEY']))


class AviationstackAdapter(DownloadAdapter):
    script = 'check-aviationstack-api.py'

    def prepare(self, profile, report):
        result = super().prepare(profile, report)
        result.setdefault('schema_version', 1)
        result.setdefault('display_time_basis', profile['time_basis'])
        return result

    def candidate_path(self, profile, date, offset):
        suffix = f'-offset-{offset}' if offset else ''
        return ROOT / f"test-output/aviationstack-{profile['airport'].lower()}{suffix}-check.json"

    def check_query(self, profile, report, date, limit, offset):
        if report.get('query') != dict(dep_iata=profile['airport'], limit=limit, offset=offset):
            raise DataError('Cached query differs; choose another file or use --refresh')

    def fetch(self, profile, env, date, limit, offset, budget):
        module = legacy(profile['provider'])
        key = module.load_key(env)
        query = dict(dep_iata=profile['airport'], limit=limit, offset=offset)
        return module.collect(key, query, transport=lambda *a: budget.call(module.request_page, *a))


ADAPTERS = {'icn': IcnAdapter(), 'aviationstack': AviationstackAdapter()}


def adapter(provider):
    try:
        return ADAPTERS[provider]
    except KeyError:
        raise DataError('No downloader registered for this provider') from None


@lru_cache(maxsize=None)
def legacy(provider):
    spec = importlib.util.spec_from_file_location('flightdata_' + provider, ROOT / 'tools' / adapter(provider).script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def prepare(profile, report):
    return adapter(profile['provider']).prepare(profile, report)


def request_date(profile, value=None):
    return adapter(profile['provider']).request_date(profile, value)


def candidate_path(profile, date, offset=0):
    return adapter(profile['provider']).candidate_path(profile, date, offset)


def check_query(profile, report, date, limit, offset):
    return adapter(profile['provider']).check_query(profile, report, date, limit, offset)


def fetch(profile, env, date, limit, offset, budget):
    return adapter(profile['provider']).fetch(profile, env, date, limit, offset, budget)
