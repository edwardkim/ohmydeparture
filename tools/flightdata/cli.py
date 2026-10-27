"""Collect, inspect and activate flight data through the actual Qt validators."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import uuid
from . import providers
from .common import DataError, RequestBudget, atomic_write, json_text, state_home

ROOT = providers.ROOT
MAX_BYTES = 16 * 1024 * 1024


def read_json(path):
    path = Path(path)
    if path.stat().st_size > MAX_BYTES:
        raise DataError('Snapshot exceeds the 16 MiB limit')
    return json.loads(path.read_text(encoding='utf-8-sig'))


def app_json(app, *arguments):
    app = Path(app).resolve()
    if not app.is_file() or not os.access(app, os.X_OK):
        raise DataError('Build the application with make first, or select a binary with --app')
    environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QPA_PLATFORMTHEME='', QT_IM_MODULE='')
    result = subprocess.run([str(app), *arguments], capture_output=True, text=True, timeout=30, env=environment)
    try:
        value = json.loads(result.stdout)
    except ValueError:
        raise DataError('The binary did not return data inspection JSON; check its version and Qt dependencies') from None
    if result.returncode not in (0, 1):
        raise DataError('The data inspector failed')
    return value


def profile_for(app, airport):
    for profile in app_json(app, '--data-providers'):
        if profile['airport'] == airport.upper():
            return profile
    raise DataError('No data provider registered for this airport')


def validate(app, profile, report, original):
    prepared = providers.prepare(profile, report)
    with tempfile.TemporaryDirectory(prefix='ohmydeparture-data-') as folder:
        candidate = Path(folder) / 'candidate.json'
        atomic_write(candidate, json_text(prepared))
        result = app_json(app, '--data-status', '--airport', profile['airport'], '--data-file', str(candidate))
    result['path'] = str(Path(original).resolve())
    result['normalized_sha256'] = result.pop('sha256', '')
    if not result.get('valid') or result.get('provider') != profile['provider']:
        raise DataError('Snapshot failed provider validation; active data was preserved')
    return prepared, result


def target_for(profile):
    if profile['storage'] not in ('embedded', 'private_cache'):
        raise DataError('Unsupported provider storage policy')
    path = profile['path']
    if profile['storage'] == 'embedded':
        if not path.startswith(':/data/'):
            raise DataError('Unsupported embedded snapshot location')
        target = (ROOT / path[2:]).resolve()
        if not target.is_relative_to(ROOT / 'data'):
            raise DataError('Invalid embedded snapshot location')
        return target
    return Path(path).resolve()


def install(app, profile, report, original, allow_stale=False, dry_run=False):
    prepared, result = validate(app, profile, report, original)
    if result['state'] == 'stale' and not allow_stale:
        raise DataError('Snapshot is stale; refresh it or explicitly use --allow-stale')
    target = target_for(profile)
    text = json_text(prepared)
    changed = not target.exists() or target.read_bytes() != text.encode('utf-8')
    response = dict(action='install', airport=profile['airport'], target=str(target), changed=changed,
                    dry_run=dry_run, api_calls=0, backup=None, validation=result,
                    next_step='rebuild_reinstall_restart' if profile['storage'] == 'embedded' else 'restart_app')
    if dry_run or not changed:
        return response
    if target.exists():
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '-' + uuid.uuid4().hex[:8]
        backup = state_home() / 'ohmydeparture/flight-data/backups' / stamp / (profile['airport'] + '.json')
        atomic_write(backup, target.read_text())
        response['backup'] = str(backup)
    atomic_write(target, text, mode=0o644 if profile['storage'] == 'embedded' else 0o600)
    return response


def parser():
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest='command', required=True)
    commands.add_parser('providers', help='List providers registered in the selected application')
    commands.add_parser('status', help='Inspect active data and detect an outdated embedded ICN build')
    commands.add_parser('validate', help='Validate a file offline without installing it')
    commands.add_parser('fetch', help='Fetch a candidate or reuse its matching cache; never activate it')
    commands.add_parser('install', help='Validate, back up and activate a candidate without API calls')
    for command, child in commands.choices.items():
        child.add_argument('--app', type=Path, default=ROOT / 'ohmydeparture', help='Binary to inspect/validate with')
        if command == 'providers':
            continue
        child.add_argument('--airport', required=True, type=str.upper)
        if command in ('validate', 'install'):
            child.add_argument('--file', type=Path, required=True)
        if command == 'install':
            child.add_argument('--allow-stale', action='store_true')
            child.add_argument('--dry-run', action='store_true')
        if command == 'fetch':
            child.add_argument('--env', type=Path, default=ROOT / '.env')
            child.add_argument('--date')
            child.add_argument('--limit', type=int)
            child.add_argument('--offset', type=int, default=0)
            child.add_argument('--max-requests', type=int, help='Default: 10 for ICN; 1 for Aviationstack')
            child.add_argument('--output', type=Path)
            child.add_argument('--refresh', action='store_true')
            child.add_argument('--dry-run', action='store_true')
    return root


def main(argv=None):
    args = parser().parse_args(argv)
    budget = None
    try:
        if args.command == 'providers':
            result = app_json(args.app, '--data-providers')
        else:
            profile = profile_for(args.app, args.airport)
            if args.command == 'status':
                result = app_json(args.app, '--data-status', '--airport', args.airport)
                result['binary'] = str(args.app.resolve())
                if profile['storage'] == 'embedded':
                    target = target_for(profile)
                    result['source_path'] = str(target)
                    result['needs_rebuild'] = target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() != result.get('sha256')
            elif args.command in ('validate', 'install'):
                report = read_json(args.file)
                if args.command == 'validate':
                    _, result = validate(args.app, profile, report, args.file)
                else:
                    result = install(args.app, profile, report, args.file, args.allow_stale, args.dry_run)
            else:
                adapter = providers.adapter(profile['provider'])
                limit = args.limit if args.limit is not None else adapter.default_limit
                maximum = args.max_requests if args.max_requests is not None else adapter.default_budget
                if not 1 <= limit <= adapter.maximum_limit or args.offset < 0 or (not adapter.supports_offset and args.offset):
                    raise DataError('Invalid page size or offset for this provider')
                budget = RequestBudget(maximum)
                date = providers.request_date(profile, args.date)
                output = args.output or providers.candidate_path(profile, date, args.offset)
                reuse = output.exists() and not args.refresh
                if args.dry_run:
                    result = dict(action='fetch', airport=args.airport, file=str(output), dry_run=True,
                                  cache_reused=reuse, max_requests=maximum, api_calls=0,
                                  planned_calls=0 if reuse or maximum == 0 else None if adapter.supports_date else 1,
                                  blocked_by_budget=not reuse and maximum == 0)
                else:
                    if not reuse and maximum == 0:
                        raise DataError('No matching cache and request budget is zero; no request was sent')
                    report = read_json(output) if reuse else providers.fetch(profile, args.env, date, limit, args.offset, budget)
                    providers.check_query(profile, report, date, limit, args.offset)
                    prepared, metadata = validate(args.app, profile, report, output)
                    if not reuse:
                        atomic_write(output, json_text(prepared))
                    result = dict(action='fetch', airport=args.airport, file=str(output),
                                  cache_reused=reuse, api_calls=budget.used, validation=metadata)
        print(json_text(result), end='')
        return 0 if args.command != 'status' or result.get('valid') else 1
    except DataError as error:
        message = str(error)
    except (OSError, ValueError, KeyError, TypeError, AttributeError, subprocess.SubprocessError):
        message = 'Operation failed; check configuration, response and file access. Credentials are not logged.'
    print(json_text(dict(ok=False, error=message, api_calls=budget.used if budget else 0)), end='')
    return 1
