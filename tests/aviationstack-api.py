#!/usr/bin/env python3
"""Offline checks for single-request limits, caching and credential handling."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('aviationstack', ROOT / 'tools/check-aviationstack-api.py')
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)
KEY = 'private-test-credential-123456'
QUERY = dict(dep_iata='NRT', limit=100, offset=0)


def payload():
    return dict(pagination=dict(limit=100, offset=0, count=2, total=205), data=[
        dict(flight_date='2026-10-27', departure=dict(iata='NRT', scheduled='2026-10-27T12:00:00+00:00', gate=None),
             flight=dict(iata='AA1', codeshared=None), airline=dict(name=KEY)),
        dict(flight_date='2026-10-27', departure=dict(iata='NRT', scheduled='2026-10-27T12:00:00+00:00', gate='31'),
             flight=dict(iata='BB2', codeshared=dict(flight_iata='AA1'))),
    ])


def transport(*args):
    return 200, json.dumps(payload()).encode()


class ApiTests(unittest.TestCase):
    def setUp(self):
        self.environment = patch.dict(os.environ, {'AVIATIONSTACK_KEY': KEY})
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def test_one_page_is_explicitly_partial_and_retains_nulls_and_codeshares(self):
        with patch.object(api, 'request_page', side_effect=transport) as request:
            report = api.collect(KEY, QUERY)
        request.assert_called_once_with(KEY, QUERY)
        result = api.summary(report)
        self.assertTrue(result['partial'])
        self.assertEqual((result['returned'], result['total'], result['pages_at_this_limit']), (2, 205, 3))
        self.assertEqual(result['codeshares'], 1)
        self.assertIsNone(report['response']['data'][0]['departure']['gate'])
        self.assertNotIn(KEY, json.dumps(report))

    def test_invalid_pagination_wrong_airport_and_errors_are_rejected(self):
        wrong_count = payload(); wrong_count['pagination']['count'] = 3
        wrong_airport = payload(); wrong_airport['data'][0]['departure']['iata'] = 'SFO'
        for value in [wrong_count, wrong_airport, {'error': {'code': 'usage_limit_reached', 'info': KEY}}]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                api.collect(KEY, QUERY, transport=lambda *args: (200, json.dumps(value).encode()))

    def test_cache_reuse_requires_no_credentials_or_network(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'cache.json'
            api.save(api.collect(KEY, QUERY, transport), output)
            stdout = io.StringIO()
            with patch.object(api, 'load_key', side_effect=AssertionError('key read')), \
                 patch.object(api, 'request_page', side_effect=AssertionError('network call')), \
                 contextlib.redirect_stdout(stdout):
                self.assertEqual(api.main(['--output', str(output)]), 0)
            self.assertIn('API calls this run: 0', stdout.getvalue())

    def test_refresh_uses_one_call_and_writes_owner_only_redacted_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'cache.json'
            stdout = io.StringIO()
            with patch.object(api, 'request_page', side_effect=transport) as request, contextlib.redirect_stdout(stdout):
                self.assertEqual(api.main(['--output', str(output), '--refresh']), 0)
            self.assertEqual(request.call_count, 1)
            self.assertNotIn(KEY, output.read_text() + stdout.getvalue())
            self.assertEqual(output.stat().st_mode & 0o777, 0o600)
            self.assertEqual(list(Path(folder).glob('.aviationstack-*')), [])

    def test_failed_refresh_and_mismatched_cache_preserve_existing_file(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'cache.json'
            api.save(api.collect(KEY, QUERY, transport), output)
            original = output.read_bytes()
            stdout = io.StringIO()
            with patch.object(api, 'request_page', side_effect=OSError(KEY)), contextlib.redirect_stdout(stdout):
                self.assertEqual(api.main(['--output', str(output), '--refresh']), 1)
            self.assertEqual(output.read_bytes(), original)
            self.assertNotIn(KEY, stdout.getvalue())
            with patch.object(api, 'request_page', side_effect=AssertionError('network call')), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(api.main(['--output', str(output), '--airport', 'SFO']), 1)
            self.assertEqual(output.read_bytes(), original)

    def test_install_activates_cached_data_without_an_api_call(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            output = base / 'cache.json'
            api.save(api.collect(KEY, QUERY, transport), output)
            with patch.dict(os.environ, {'XDG_DATA_HOME': str(base / 'data')}), \
                 patch.object(api, 'request_page', side_effect=AssertionError('network call')), \
                 patch.object(api, 'load_key', side_effect=AssertionError('key read')), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(api.main(['--output', str(output), '--install']), 0)
            installed = base / 'data/ohmydeparture/aviationstack/nrt.json'
            report = json.loads(installed.read_text())
            self.assertEqual(report['display_time_basis'], 'reported_wall_time')
            self.assertEqual(report['response'], json.loads(output.read_text())['response'])
            self.assertEqual(installed.stat().st_mode & 0o777, 0o600)


if __name__ == '__main__':
    unittest.main()
