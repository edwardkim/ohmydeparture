#!/usr/bin/env python3
"""Exercise data operations offline using the same validators as the board."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from flightdata import cli, providers
from flightdata.common import DataError, atomic_write, json_text

KEY = 'test-private-credential-abcdef'


def report(identifier='NH1'):
    return dict(provider='aviationstack', endpoint='https://api.aviationstack.com/v1/flights',
                query=dict(dep_iata='NRT', limit=100, offset=0), http_status=200,
                fetched_at='2000-01-01T00:00:00Z', response=dict(
                    pagination=dict(limit=100, offset=0, count=1, total=1418), data=[dict(
                        flight_date='2000-01-01', flight_status='scheduled',
                        departure=dict(iata='NRT', timezone='Asia/Tokyo', scheduled='2000-01-01T12:00:00+00:00', gate=None),
                        arrival=dict(iata='ICN', airport='Incheon'), airline=dict(iata='NH', name='Test airline'),
                        flight=dict(iata=identifier, codeshared=None))]))


class DataTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(); self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.environment = patch.dict(os.environ, dict(XDG_DATA_HOME=str(self.base / 'data'),
            XDG_STATE_HOME=str(self.base / 'state'), XDG_CONFIG_HOME=str(self.base / 'config'), AVIATIONSTACK_KEY=KEY))
        self.environment.start(); self.addCleanup(self.environment.stop)
        self.profile = cli.profile_for(ROOT / 'ohmydeparture', 'NRT')
        self.target = Path(self.profile['path'])

    def run_cli(self, *arguments):
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            code = cli.main(list(arguments))
        self.assertNotIn(KEY, stdout.getvalue())
        self.assertFalse(list((self.base / 'config').rglob('*.conf')))
        return code, json.loads(stdout.getvalue())

    def test_provider_discovery_and_missing_status(self):
        code, value = self.run_cli('providers')
        self.assertEqual(code, 0)
        self.assertEqual({p['provider'] for p in value}, {'icn', 'aviationstack'})
        code, value = self.run_cli('status', '--airport', 'NRT')
        self.assertEqual((code, value['state']), (1, 'missing'))

    def test_inspection_is_headless_even_when_the_binary_does_not_set_its_backend(self):
        app = self.base / 'inspector'
        app.write_text("""#!/bin/sh
test "$QT_QPA_PLATFORM" = offscreen || exit 3
test -z "$QT_QPA_PLATFORMTHEME" || exit 3
test -z "$QT_IM_MODULE" || exit 3
printf '[]'
""")
        app.chmod(0o700)
        with patch.dict(os.environ, dict(QT_QPA_PLATFORM='wayland', QT_QPA_PLATFORMTHEME='gtk3', QT_IM_MODULE='ibus')):
            self.assertEqual(cli.app_json(app, '--data-providers'), [])

    def test_validation_uses_board_rules_and_reports_stale_partial_data(self):
        candidate = self.base / 'report.json'; candidate.write_text(json_text(report()))
        code, value = self.run_cli('validate', '--airport', 'NRT', '--file', str(candidate))
        self.assertEqual(code, 0)
        self.assertTrue(value['valid'] and value['partial'])
        self.assertEqual((value['state'], value['display_count']), ('stale', 1))
        value = report(); value['response']['data'][0]['departure']['timezone'] = 'UTC'
        candidate.write_text(json_text(value))
        self.assertEqual(self.run_cli('validate', '--airport', 'NRT', '--file', str(candidate))[0], 1)
        self.assertFalse(self.target.exists())

    def test_install_has_dry_run_backups_and_preserves_last_good_data(self):
        result = cli.install(ROOT / 'ohmydeparture', self.profile, report(), 'candidate', True, True)
        self.assertTrue(result['changed']); self.assertFalse(self.target.exists())
        first = cli.install(ROOT / 'ohmydeparture', self.profile, report(), 'candidate', True)
        self.assertIsNone(first['backup']); original = self.target.read_bytes()
        second = cli.install(ROOT / 'ohmydeparture', self.profile, report('NH2'), 'candidate', True)
        backup = Path(second['backup'])
        self.assertEqual(backup.read_bytes(), original)
        self.assertEqual(backup.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.target.stat().st_mode & 0o777, 0o600)
        stable = self.target.read_bytes(); invalid = report(); invalid['response']['pagination']['count'] = 2
        with self.assertRaises(DataError):
            cli.install(ROOT / 'ohmydeparture', self.profile, invalid, 'candidate', True)
        self.assertEqual(self.target.read_bytes(), stable)

    def test_stale_install_requires_explicit_opt_in(self):
        with self.assertRaisesRegex(DataError, 'stale'):
            cli.install(ROOT / 'ohmydeparture', self.profile, report(), 'candidate')
        self.assertFalse(self.target.exists())

    def test_cache_reuse_dry_run_and_zero_budget_make_no_requests(self):
        candidate = self.base / 'report.json'; candidate.write_text(json_text(report()))
        with patch.object(providers, 'fetch', side_effect=AssertionError('network')):
            code, value = self.run_cli('fetch', '--airport', 'NRT', '--output', str(candidate), '--max-requests', '0')
            self.assertEqual((code, value['api_calls'], value['cache_reused']), (0, 0, True))
            candidate.unlink()
            code, value = self.run_cli('fetch', '--airport', 'NRT', '--output', str(candidate), '--dry-run')
            self.assertEqual((code, value['api_calls'], value['planned_calls']), (0, 0, 1))
            code, value = self.run_cli('fetch', '--airport', 'NRT', '--output', str(candidate), '--dry-run', '--max-requests', '0')
            self.assertEqual((code, value['planned_calls'], value['blocked_by_budget']), (0, 0, True))
            code, value = self.run_cli('fetch', '--airport', 'NRT', '--output', str(candidate), '--max-requests', '0')
            self.assertEqual((code, value['api_calls']), (1, 0))

    def test_fetch_redacts_and_does_not_activate(self):
        candidate = self.base / 'report.json'; value = report()
        value['response']['data'][0]['airline']['name'] = KEY
        module = providers.legacy('aviationstack')
        with patch.object(module, 'request_page', return_value=(200, json.dumps(value['response']).encode())) as request:
            code, result = self.run_cli('fetch', '--airport', 'NRT', '--output', str(candidate))
        self.assertEqual((code, result['api_calls'], request.call_count), (0, 1, 1))
        self.assertNotIn(KEY, candidate.read_text()); self.assertFalse(self.target.exists())

    def test_icn_request_budget_stops_before_next_page_and_preserves_candidate(self):
        candidate = self.base / 'icn.json'; candidate.write_text('previous candidate')
        module = providers.legacy('icn')
        page = dict(ok=True, total_count=2, items=[dict(fid='a', scheduleDatetime='202610271000')])
        with patch.object(module.api, 'load_config', return_value=({'ICN_KEY': KEY}, None)), \
             patch.object(module.api, 'probe', return_value=page) as request:
            code, value = self.run_cli('fetch', '--airport', 'ICN', '--date', '20261027', '--limit', '1',
                                      '--max-requests', '1', '--output', str(candidate), '--refresh')
        self.assertEqual((code, value['api_calls'], request.call_count), (1, 1, 1))
        self.assertIn('budget', value['error']); self.assertEqual(candidate.read_text(), 'previous candidate')

    def test_credentials_are_rejected_before_install(self):
        value = report(); value['source'] = dict(serviceKey=KEY)
        with self.assertRaises(DataError):
            cli.install(ROOT / 'ohmydeparture', self.profile, value, 'candidate', True)
        self.assertFalse(self.target.exists())

    def test_status_detects_source_changes_not_embedded_in_binary(self):
        source = self.base / 'changed-source.json'; source.write_text('{}')
        with patch.object(cli, 'target_for', return_value=source):
            code, value = self.run_cli('status', '--airport', 'ICN')
        self.assertEqual(code, 0); self.assertTrue(value['needs_rebuild'])
        self.assertEqual(len(value['sha256']), 64)

    def test_failed_atomic_publish_preserves_previous_file(self):
        target = self.base / 'data.json'; target.write_text('previous')
        with patch('flightdata.common.os.replace', side_effect=OSError('failure')), self.assertRaises(OSError):
            atomic_write(target, 'replacement')
        self.assertEqual(target.read_text(), 'previous')
        self.assertFalse(list(self.base.glob('.flight-data-*')))


if __name__ == '__main__':
    unittest.main()
