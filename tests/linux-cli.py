#!/usr/bin/env python3
"""Exercise every board and language phase using the installed renderer."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
APP = ROOT / "ohmydeparture"
OUT = ROOT / "test-output"
OUT.mkdir(exist_ok=True)


def render(job):
    code, phase = job
    path = OUT / f"{code.lower()}-{phase}.png"
    result = subprocess.run(
        [str(APP), "--snapshot", str(path), "--airport", code,
         "--phase", str(phase), "--size", "1600x900",
         "--wait", "4" if code in AIRPORTS else "0"],
        capture_output=True, text=True, timeout=20, env=ENV,
    )
    assert result.returncode == 0, (job, result.stdout, result.stderr)
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) > 10000, job
    assert struct.unpack(">II", data[16:24]) == (1600, 900), job
    return job


AIRPORTS = ("ICN", "NRT", "HKG", "LHR", "CDG", "MXP", "JFK", "SFO")
JOBS = [(code, phase) for code in AIRPORTS for phase in range(3)]
JOBS += [("TOKYO", phase) for phase in range(8)]
JOBS += [("SEOUL", phase) for phase in range(4)] + [("VICTORIA", 0), ("VICTORIA", 1), ("SYDNEY", 0)]

with tempfile.TemporaryDirectory(prefix="rdepartures-cli-") as config:
    ENV = dict(os.environ, XDG_CONFIG_HOME=config)
    with ThreadPoolExecutor(max_workers=4) as workers:
        results = list(workers.map(render, JOBS))
    for args in (
        ["--airport", "INVALID"], ["--size", "1x1"], ["--size", "1600x900x2"],
        ["--size", "8192x8192"], ["--phase", "-1"], ["--wait", "nan"],
        ["--wait", "-1"], ["--offset", "not-a-number"],
    ):
        result = subprocess.run([str(APP), "--view", "departures", "--snapshot", str(OUT / "invalid.png"), *args],
                                env=ENV, capture_output=True, timeout=10)
        assert result.returncode == 2, (args, result.returncode, result.stderr)
    assert not (OUT / "invalid.png").exists()
    for option in ("--help", "--version"):
        result = subprocess.run([str(APP), option], env=ENV, capture_output=True, timeout=10)
        assert result.returncode == 0 and result.stdout, option
    assert not list(Path(config).rglob("*.conf")), "snapshots must not change user settings"
    print(f"PASS: {len(results)} rendered boards/phases, 8 invalid inputs, headless help/version, no settings changes")
