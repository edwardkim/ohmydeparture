#!/usr/bin/env python3
"""Check the actual bundle's lifecycle and launcher without touching the desktop."""
import importlib.util
from types import SimpleNamespace
from unittest.mock import patch
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
PLUGIN_ID = "edwardkim.ohmydeparture"
VERSION = json.loads((ROOT / "packaging/omarchy/plugin/manifest.json").read_text())["version"]
BUNDLE = ROOT / "dist" / f"ohmydeparture-omarchy-{VERSION}-{platform.machine()}"


def run(args, *, env, expected=0):
    result = subprocess.run(args, capture_output=True, text=True, env=env, timeout=15)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix="rdepartures-package-") as temporary:
    base = Path(temporary)
    config = base / "config with space's"
    data, state = base / "data", base / "state"
    env = dict(os.environ, XDG_CONFIG_HOME=str(config), XDG_DATA_HOME=str(data), XDG_STATE_HOME=str(state))
    # CI lacks Omarchy: substitute ONLY the validator process in this test sandbox.
    # Portable manifest checks still run in the package builder.
    if not shutil.which("omarchy"):
        stub = base / "test-bin"; stub.mkdir()
        executable = stub / "omarchy"
        executable.write_text("#!/bin/sh\n[ \"$1\" = plugin ] && [ \"$2\" = validate ]\n")
        executable.chmod(0o755)
        env["PATH"] = str(stub) + os.pathsep + env["PATH"]
    env.pop("DBUS_SESSION_BUS_ADDRESS", None)
    env.pop("XDG_RUNTIME_DIR", None)
    env.pop("HYPRLAND_INSTANCE_SIGNATURE", None)
    shell = config / "omarchy/shell.json"
    shell.parent.mkdir(parents=True)
    fixture = {"version": 1, "idle": {"lock": 1234}, "bar": {"position": "top", "layout": {
        "left": [{"id": "existing.clock", "format": "HH:mm"}], "center": [],
        "right": [{"id": "existing.audio"}]}}, "plugins": [{"id": "existing.service"}], "other": "preserve"}
    shell.write_text(json.dumps(fixture))
    command = [sys.executable, str(BUNDLE / "install.py"), "--no-reload"]
    run(command, env=env)
    plugin = config / "omarchy/plugins" / PLUGIN_ID
    desktop = data / "applications" / (PLUGIN_ID + ".desktop")
    actual = json.loads(shell.read_text())
    assert actual["idle"] == fixture["idle"] and actual["plugins"] == fixture["plugins"]
    assert actual["other"] == "preserve" and actual["bar"]["layout"]["left"] == fixture["bar"]["layout"]["left"]
    assert actual["bar"]["layout"]["right"][-1] == {"id": PLUGIN_ID}
    assert os.access(plugin / "launch", os.X_OK) and os.access(plugin / "bin/ohmydeparture", os.X_OK)
    assert (plugin / "libexec/codex-reader.py").is_file() and "--view codex" in desktop.read_text()
    run(["omarchy", "plugin", "validate", str(plugin)], env=env)
    run(["desktop-file-validate", str(desktop)], env=env)
    run([str(plugin / "launch"), "--version"], env=env)
    image = base / "seoul.png"
    run([str(plugin / "launch"), "--snapshot", str(image), "--airport", "SEOUL", "--wait", "0"], env=env)
    assert image.read_bytes().startswith(b"\x89PNG")
    assert list((state / "ohmydeparture-omarchy/backups").glob("*/shell.json"))

    # Updating keeps existing placement and inline user settings.
    layout = actual["bar"]["layout"]
    layout["right"] = fixture["bar"]["layout"]["right"]
    settings = {"id": PLUGIN_ID, "view": "departures", "airport": "SEOUL", "windowed": False}
    layout["left"].append(settings)
    shell.write_text(json.dumps(actual))
    run(command, env=env)
    actual = json.loads(shell.read_text())
    assert actual["bar"]["layout"]["left"][-1] == settings
    run(command + ["--section", "center"], env=env)
    actual = json.loads(shell.read_text())
    assert actual["bar"]["layout"]["center"] == [settings]
    assert all(entry.get("id") != PLUGIN_ID for entry in actual["bar"]["layout"]["left"])

    # Dotfile-manager symlinks survive config writes.
    real_config = base / "managed-shell.json"
    shell.rename(real_config)
    shell.symlink_to(real_config)
    run(command, env=env)
    assert shell.is_symlink() and json.loads(real_config.read_text()) == actual

    shell.write_text("{invalid")
    before_plugin = (plugin / "manifest.json").read_bytes()
    run(command, env=env, expected=1)
    assert shell.read_text() == "{invalid" and (plugin / "manifest.json").read_bytes() == before_plugin
    shell.write_text(json.dumps(actual))

    preferences = config / "edwardkim/OhMyDeparture.conf"
    preferences.parent.mkdir(parents=True)
    preferences.write_text("[General]\nairport=SEOUL\n")
    run(command + ["--uninstall"], env=env)
    assert not plugin.exists() and not desktop.exists()
    assert json.loads(shell.read_text()) == fixture
    assert preferences.read_text().endswith("airport=SEOUL\n")
    run(command + ["--uninstall"], env=env)

    # A cached 1.0 widget with no --view must enter Codex. Explicit airport
    # requests retain departure mode; explicit view values go through unchanged.
    mock = base / "mock plugin's directory"
    shutil.copytree(BUNDLE / PLUGIN_ID, mock)
    renderer = mock / "bin/ohmydeparture"
    renderer.write_text(f"#!{sys.executable}\nimport json,os,sys\n"
                        "with open(os.environ['RDEPARTURES_TEST_LOG'],'a') as f: f.write(json.dumps(sys.argv[1:])+'\\n')\n")
    renderer.chmod(0o755)
    log = base / "renderer.jsonl"
    env["RDEPARTURES_TEST_LOG"] = str(log)
    for args in [[], ["--airport","SEOUL"], ["--view","departures","--windowed"], ["--version"]]:
        run([str(mock/"launch"),*args],env=env)
    calls=[json.loads(s) for s in log.read_text().splitlines()]
    assert calls==[["--view","codex"],["--view","departures","--airport","SEOUL"],
                   ["--view","departures","--windowed"],["--version"]],calls

    # Migrate only the locally managed legacy plugin, with position and settings.
    old_id = "rainygirl.rdepartures"
    old_plugin = plugin.with_name(old_id)
    old_desktop = desktop.with_name(old_id + ".desktop")
    old_plugin.mkdir(parents=True)
    (old_plugin / "manifest.json").write_text(json.dumps(dict(id=old_id, author="Local Omarchy port (upstream: rainygirl)")))
    (old_plugin / "preserved-link").symlink_to("manifest.json")
    old_desktop.write_text("[Desktop Entry]\nX-RDepartures-Omarchy=true\n")
    migration = json.loads(json.dumps(fixture))
    old_settings = dict(id=old_id, view="departures", airport="NRT", windowed=False)
    migration["bar"]["layout"]["left"].insert(0, old_settings)
    migration["plugins"].append(dict(id=old_id, custom=True))
    shell.write_text(json.dumps(migration))
    run(command, env=env)
    moved = json.loads(shell.read_text())
    expected_settings = dict(old_settings, id=PLUGIN_ID)
    assert moved["bar"]["layout"]["left"][0] == expected_settings
    assert moved["plugins"][-1] == dict(id=PLUGIN_ID, custom=True)
    assert not old_plugin.exists() and not old_desktop.exists()
    backups = state / "ohmydeparture-omarchy/backups"
    assert any(p.is_symlink() for p in backups.glob("*/legacy-plugin/preserved-link"))

    # A foreign legacy installation stays intact, including its bar entry.
    old_plugin.mkdir(); (old_plugin / "manifest.json").write_text(json.dumps(dict(id=old_id, author="Someone else")))
    old_desktop.write_text("[Desktop Entry]\nName=Foreign\n")
    moved["bar"]["layout"]["center"].append(old_settings)
    shell.write_text(json.dumps(moved))
    run(command, env=env)
    assert old_plugin.exists() and old_desktop.exists()
    assert json.loads(shell.read_text())["bar"]["layout"]["center"] == [old_settings]

    # Refuse foreign launchers and top-level plugin symlinks before mutation.
    original_desktop = desktop.read_bytes()
    desktop.write_text("[Desktop Entry]\nName=Unmanaged\n")
    unchanged = shell.read_bytes()
    run(command, env=env, expected=1)
    assert shell.read_bytes() == unchanged
    desktop.write_bytes(original_desktop)
    real_plugin = base / "linked-plugin"; plugin.rename(real_plugin); plugin.symlink_to(real_plugin)
    run(command, env=env, expected=1)
    assert plugin.is_symlink() and shell.read_bytes() == unchanged
    plugin.unlink(); real_plugin.rename(plugin)

    # Inject a write failure after replacing the plugin and writing shell.json.
    spec = importlib.util.spec_from_file_location("bundle_installer", BUNDLE / "install.py")
    installer = importlib.util.module_from_spec(spec); spec.loader.exec_module(installer)
    old_plugin_manifest = (plugin / "manifest.json").read_bytes()
    sentinel = plugin / "previous-install.txt"; sentinel.write_text("restore this")
    native_write = installer.atomic_write
    failed = False
    def fail_once(path, data):
        global failed
        if path == desktop and not failed:
            failed = True
            raise OSError("injected desktop write failure")
        native_write(path, data)
    args = SimpleNamespace(config_home=str(config), data_home=str(data), state_home=str(state), uninstall=False, section="right", no_reload=True)
    with patch.dict(os.environ, env), patch.object(installer, "atomic_write", side_effect=fail_once):
        try:
            installer.install(args)
            raise AssertionError("failure injection did not fire")
        except OSError as error:
            assert "injected" in str(error)
    assert shell.is_symlink() and shell.read_bytes() == unchanged
    assert sentinel.read_text() == "restore this" and (plugin / "manifest.json").read_bytes() == old_plugin_manifest
    assert desktop.read_bytes() == original_desktop

    # Duplicate new/owned-legacy entries prefer new settings, then remove duplicates.
    duplicate = installer.update_layout({"bar":{"layout":{"left":[old_settings],"center":[expected_settings],"right":[dict(id=PLUGIN_ID)]}}}, migrate=True)
    assert duplicate["bar"]["layout"] == {"left":[],"center":[expected_settings],"right":[]}

print("PASS: migration, foreign ownership, duplicate entries, injected rollback, plugin install/update/move/remove, backups, settings and symlinks, no session bus, packaged rendering, view-aware launcher")
