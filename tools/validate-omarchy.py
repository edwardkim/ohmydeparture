#!/usr/bin/env python3
"""Portable bundle checks; supplements the native Omarchy validator."""
import argparse
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(plugin, payload=True):
    plugin = Path(plugin).resolve()
    if any(path.is_symlink() for path in plugin.rglob("*")):
        raise ValueError("Plugin payload must not contain symlinks")
    manifest = json.loads((plugin / 'manifest.json').read_text())
    require(type(manifest.get('schemaVersion')) is int and manifest['schemaVersion'] == 1, 'Unsupported manifest schema')
    require(manifest.get('id') == 'edwardkim.ohmydeparture', 'Unexpected plugin ID')
    require(re.fullmatch(r'\d+\.\d+\.\d+', manifest.get('version', '')), 'Invalid version')
    require(manifest.get('kinds') == ['bar-widget'], 'Expected bar-widget')
    entry = manifest['entryPoints']['barWidget']
    require(not Path(entry).is_absolute() and (plugin / entry).resolve().is_relative_to(plugin), 'Unsafe entry point')
    widget = manifest['barWidget']
    require(widget['defaults']['view'] == 'codex', 'Codex must be the default')
    require(widget['defaultSection'] in ('left', 'center', 'right'), 'Invalid bar section')
    require({i['key'] for i in widget['schema']} == {'view', 'windowed', 'airport'}, 'Unexpected settings')
    files = [entry, 'launch']
    if payload:
        files += ['bin/ohmydeparture', 'libexec/codex-reader.py', 'icon.png', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'licenses/GUST-FONT-LICENSE.txt']
    for name in files:
        path = plugin / name
        require(path.resolve().is_relative_to(plugin) and path.is_file(), f'Missing or unsafe file: {name}')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plugin', type=Path)
    parser.add_argument('--template', action='store_true')
    args = parser.parse_args()
    validate(args.plugin, not args.template)
    print('PASS: portable Omarchy bundle validation')
