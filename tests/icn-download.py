#!/usr/bin/env python3
"""Offline pagination and failure checks for the ICN downloader."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('download_icn', ROOT / 'tools/fetch-icn-departures.py')
downloader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(downloader)


def record(identifier, time='202610261000'):
    return {'fid': identifier, 'scheduleDatetime': time, 'codeshare': 'Master', 'passengerOrCargo': 'Passenger'}


def response(items, count):
    return {'ok': True, 'total_count': count, 'items': items}


class DownloadTests(unittest.TestCase):
    def test_collects_every_page_with_scheduled_day_filter(self):
        calls = []
        def probe(*args, **kwargs):
            calls.append(kwargs)
            return response([record(str(kwargs['page']))], 2)
        snapshot = downloader.collect(None, 'test-key', '20261026', rows=1, probe=probe)
        self.assertEqual(snapshot['record_count'], 2)
        self.assertEqual(snapshot['source']['pages'], 2)
        self.assertEqual([item['fid'] for item in snapshot['items']], ['1', '2'])
        self.assertEqual([call['page'] for call in calls], [1, 2])
        self.assertTrue(all(call['time_basis'] == 'S' and call['search_from'] == '0000'
                            and call['search_to'] == '2400' for call in calls))
        self.assertNotIn('test-key', json.dumps(snapshot))

    def test_partial_duplicate_changed_total_and_wrong_day_fail(self):
        cases = [
            [response([record('a')], 2), response([], 2)],
            [response([record('a')], 2), response([record('a')], 2)],
            [response([record('a')], 2), response([record('b')], 3)],
            [response([record('a', '202610271000')], 1)],
            [response([record('a', '202610262561')], 1)],
            [{'ok': False}],
        ]
        for responses in cases:
            with self.subTest(responses=responses), self.assertRaises(ValueError):
                iterator = iter(responses)
                downloader.collect(None, 'test-key', '20261026', rows=1,
                                   probe=lambda *args, **kwargs: next(iterator))

    def test_failed_download_preserves_existing_file(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'snapshot.json'; output.write_text('existing snapshot')
            with patch('sys.argv', ['fetch', '--date', '20261026', '--output', str(output)]), \
                 patch.object(downloader.api, 'load_config', return_value=({'ICN_KEY': 'test-key'}, None)), \
                 patch.object(downloader, 'collect', side_effect=ValueError('failure')), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(downloader.main(), 1)
            self.assertEqual(output.read_text(), 'existing snapshot')
            self.assertEqual(list(Path(folder).glob('.icn-*')), [])


if __name__ == '__main__':
    unittest.main()
