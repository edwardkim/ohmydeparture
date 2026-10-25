#!/usr/bin/env python3
"""Export a clean, tagged local release without contacting GitHub."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def main():
    version = (ROOT / 'VERSION').read_text().strip()
    tag = 'v' + version
    if git('status', '--porcelain'):
        raise SystemExit('Commit or remove local source changes before exporting a release.')
    commit = git('rev-parse', 'HEAD')
    if git('rev-parse', tag + '^{commit}') != commit:
        raise SystemExit('The version tag must point at HEAD.')
    dist = ROOT / 'dist'
    dist.mkdir(exist_ok=True)
    source = dist / f'ohmydeparture-{version}-source.tar.gz'
    subprocess.run(['git', 'archive', '--format=tar.gz', f'--prefix=ohmydeparture-{version}/', f'--output={source}', tag], cwd=ROOT, check=True)
    bundle = dist / f'ohmydeparture-{version}.bundle'
    subprocess.run(['git', 'bundle', 'create', str(bundle), '--all'], cwd=ROOT, check=True)
    subprocess.run(['git', 'bundle', 'verify', str(bundle)], cwd=ROOT, check=True)
    binaries = sorted(dist.glob(f'ohmydeparture-omarchy-{version}-*.tar.gz'))
    if not binaries:
        raise SystemExit('Build a native bundle with make package-omarchy first.')
    for binary in binaries:
        info = json.loads((dist / binary.name.removesuffix('.tar.gz') / 'BUILD-INFO.json').read_text())
        if info['sourceCommit'] != commit or info['sourceModified']:
            raise SystemExit(f'Rebuild {binary.name} from the clean tagged source.')
    artifacts = [source, bundle, *binaries]
    checksums = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in artifacts}
    (dist / 'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name, digest in checksums.items()))
    manifest = dict(project='edwardkim/ohmydeparture', version=version, tag=tag,
                    sourceCommit=commit, published=False, artifacts=checksums)
    (dist / 'RELEASE-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Local release prepared: {tag} ({commit[:12]})\nArtifacts: {dist}\nNo remote publication performed.')


if __name__ == '__main__':
    main()
