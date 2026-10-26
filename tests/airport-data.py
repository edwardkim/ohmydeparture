#!/usr/bin/env python3
"""Offline airport CSV conversion and snapshot checks."""
import csv
import importlib.util
import io
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('airport_csv', ROOT / 'tools/import-airport-csv.py')
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)


def fixture(rows):
    stream = io.StringIO(newline='')
    writer = csv.writer(stream)
    writer.writerow(importer.FIELDS)
    writer.writerows(rows)
    return stream.getvalue().encode('cp949')


class AirportDataTests(unittest.TestCase):
    def test_snapshot_is_reproducible_from_bundled_source(self):
        snapshot = json.loads((ROOT / 'data/airport-details.json').read_text(encoding='utf-8'))
        source = ROOT / 'data/sources' / snapshot['source']['filename']
        actual = importer.convert(source.read_bytes(), source.name, snapshot['source']['encoding'],
                                  provider=snapshot['source']['provider'],
                                  source_url=snapshot['source']['catalog_url'])
        self.assertEqual(snapshot, actual)
        self.assertEqual(snapshot['record_count'], len(snapshot['items']))
        self.assertEqual([item['sourceRecord'] for item in snapshot['items']],
                         list(range(1, snapshot['record_count'] + 1)))

    def test_duplicate_codes_keep_all_airports(self):
        data = importer.convert(fixture([
            ['First Airport', '첫 공항', 'DUP', 'AAAA', '기존공항'],
            ['Second Airport', '둘째 공항', 'DUP', 'BBBB', '기존공항'],
        ]), 'IATA_20251231.csv', 'cp949')
        self.assertEqual(data['record_count'], 2)
        self.assertEqual([item['icaoCode'] for item in data['items']], ['AAAA', 'BBBB'])
        self.assertEqual(data['quality']['duplicate_iata_codes'], {'DUP': [1, 2]})
        self.assertEqual(data['quality']['unique_iata_codes'], 1)

    def test_missing_and_invalid_codes_are_not_lookup_codes(self):
        data = importer.convert(fixture([
            ['Missing Codes', '코드 없음', 'n/a', 'n/a', '기존공항'],
            ['Invalid Codes', '잘못된 코드', 'SKMP', 'MMP', '기존공항'],
        ]), 'IATA_20251231.csv', 'cp949')
        for item in data['items']:
            self.assertIsNone(item['iataCode'])
            self.assertIsNone(item['icaoCode'])
        self.assertEqual(data['quality']['missing_iata_records'], 1)
        self.assertEqual(data['quality']['missing_icao_records'], 1)
        self.assertEqual(data['quality']['invalid_codes'], [
            {'sourceRecord': 2, 'field': 'iataCode', 'sourceValue': 'SKMP'},
            {'sourceRecord': 2, 'field': 'icaoCode', 'sourceValue': 'MMP'},
        ])

    def test_names_status_and_filename_date_are_preserved(self):
        data = importer.convert(fixture([
            ['A, B Airport', '쉼표, 공항', 'ABC', 'ABCD', '기존공항'],
        ]), 'IATA_20251231.csv', 'cp949')
        self.assertEqual(data['items'][0]['nameEnglish'], 'A, B Airport')
        self.assertEqual(data['items'][0]['nameKorean'], '쉼표, 공항')
        self.assertEqual(data['items'][0]['operatingStatus'], '기존공항')
        self.assertEqual(data['source']['date_from_filename'], '2025-12-31')
        self.assertIsNone(data['source']['provider'])
        self.assertIsNone(data['source']['usage_terms'])

    def test_incomplete_inputs_fail(self):
        for raw in (b'wrong,columns\n', fixture([]),
                    fixture([['Only one field']]),
                    fixture([['Airport', '공항', 'ABC', 'ABCD', '기존공항', 'extra']])):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                importer.convert(raw, 'IATA_20251231.csv', 'cp949')


if __name__ == '__main__':
    unittest.main()
