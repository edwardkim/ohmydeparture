#!/usr/bin/env python3
"""Build the native Linux bundle; Qt and Python remain system dependencies."""
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parent.parent
PLUGIN_ID = 'edwardkim.ohmydeparture'


def main():
    template = ROOT / 'packaging/omarchy'
    manifest = json.loads((template / 'plugin/manifest.json').read_text())
    version = (ROOT / 'VERSION').read_text().strip()
    if manifest['version'] != version:
        raise ValueError('VERSION and plugin manifest must match')
    subprocess.run(['make', '--no-print-directory', 'ohmydeparture'], cwd=ROOT, check=True)
    name = f'ohmydeparture-omarchy-{version}-{platform.machine()}'
    bundle = ROOT / 'dist' / name
    if bundle.exists():
        shutil.rmtree(bundle)
    plugin = bundle / PLUGIN_ID
    shutil.copytree(template / 'plugin', plugin)
    (plugin / 'bin').mkdir()
    shutil.copy2(ROOT / 'ohmydeparture', plugin / 'bin/ohmydeparture')
    (plugin / 'libexec').mkdir()
    shutil.copy2(ROOT / 'platform/codex/reader.py', plugin / 'libexec/codex-reader.py')
    subprocess.run(['strip', '--strip-unneeded', str(plugin / 'bin/ohmydeparture')], check=True)
    shutil.copy2(ROOT / 'docs/icon.png', plugin / 'icon.png')
    for filename in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(ROOT / filename, plugin / filename)
    (plugin / 'licenses').mkdir()
    for filename in ('GUST-FONT-LICENSE.txt', 'D-DIN-OFL-1.1.txt', 'D-DIN-FONTLOG.txt'):
        shutil.copy2(ROOT / 'fonts' / filename, plugin / 'licenses' / filename)
    for filename in ('install.py', 'README.md', 'README.ko.md'):
        shutil.copy2(template / filename, bundle / filename)
    for filename in ('launch', 'bin/ohmydeparture'):
        (plugin / filename).chmod(0o755)
    subprocess.run(['python3', str(ROOT / 'tools/validate-omarchy.py'), str(plugin)], check=True)
    if shutil.which('omarchy'):
        subprocess.run(['omarchy', 'plugin', 'validate', str(plugin)], check=True)
    commit = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=ROOT, capture_output=True, text=True)
    dirty = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True)
    info = dict(name='Oh My Departure', version=version, architecture=platform.machine(),
                system=platform.system(), libc=dict(zip(('name', 'version'), platform.libc_ver())),
                qt=subprocess.check_output(['pkg-config', '--modversion', 'Qt6Widgets'], text=True).strip(),
                sourceCommit=commit.stdout.strip() if commit.returncode == 0 else None,
                sourceModified=bool(dirty.stdout.strip()), dependenciesBundled=False)
    (bundle / 'BUILD-INFO.json').write_text(json.dumps(info, indent=2) + '\n')
    archive = ROOT / 'dist' / (name + '.tar.gz')
    with tarfile.open(archive, 'w:gz') as tar:
        tar.add(bundle, arcname=name)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + '.sha256').write_text(f'{checksum}  {archive.name}\n')
    print(f'Package: {archive}\nInstall: python3 {bundle / "install.py"}')


if __name__ == '__main__':
    main()
