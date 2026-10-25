#!/usr/bin/env python3
"""Install the bundled Omarchy widget as the desktop user, with rollback."""
import argparse
from copy import deepcopy
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

PLUGIN_ID = "edwardkim.ohmydeparture"
LEGACY_ID = "rainygirl.rdepartures"
MARKER = "X-OhMyDeparture-Omarchy=true"
LEGACY_MARKER = "X-RDepartures-Omarchy=true"


def atomic_write(path, data):
    # Preserve dotfile-manager symlinks and the target file's permissions.
    target = path.resolve() if path.is_symlink() else path
    target.parent.mkdir(parents=True, exist_ok=True)
    mode = target.stat().st_mode & 0o777 if target.exists() else 0o644
    fd, name = tempfile.mkstemp(prefix=".ohmydeparture-", dir=target.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.chmod(name, mode)
        os.replace(name, target)
    finally:
        Path(name).unlink(missing_ok=True)


def read_config(path):
    defaults = Path(os.environ.get("OMARCHY_PATH", "/usr/share/omarchy")) / "config/omarchy/shell.json"
    config = json.loads((path if path.exists() else defaults).read_text())
    if not isinstance(config, dict) or type(config.get("version")) is not int or config["version"] != 1:
        raise ValueError("Unsupported shell.json version; no settings were changed.")
    if not isinstance(config.setdefault("bar", {}), dict):
        raise ValueError("bar must be an object")
    layout = config["bar"].setdefault("layout", {})
    if not isinstance(layout, dict):
        raise ValueError("bar.layout must be an object")
    for section in ("left", "center", "right"):
        if not isinstance(layout.setdefault(section, []), list):
            raise ValueError(f"bar.layout.{section} must be an array")
    return config


def entry_id(entry):
    return entry.get("id") if isinstance(entry, dict) else entry


def update_layout(config, uninstall=False, section=None, migrate=False):
    config = deepcopy(config)
    layout = config["bar"]["layout"]
    names = ("left", "center", "right")
    entries = [(name, index, entry) for name in names for index, entry in enumerate(layout[name])]
    chosen = next((item for item in entries if entry_id(item[2]) == PLUGIN_ID), None)
    if chosen is None and migrate and not uninstall:
        chosen = next((item for item in entries if entry_id(item[2]) == LEGACY_ID), None)
    remove = {PLUGIN_ID, LEGACY_ID} if migrate and not uninstall else {PLUGIN_ID}
    placed = False
    for name in names:
        result = []
        for index, entry in enumerate(layout[name]):
            if entry_id(entry) not in remove:
                result.append(entry)
            elif not uninstall and section is None and chosen and (name, index) == chosen[:2]:
                value = deepcopy(entry) if isinstance(entry, dict) else {"id": PLUGIN_ID}
                value["id"] = PLUGIN_ID
                result.append(value)
                placed = True
        layout[name] = result
    if not uninstall and not placed:
        value = deepcopy(chosen[2]) if chosen and isinstance(chosen[2], dict) else {}
        value["id"] = PLUGIN_ID
        layout[section or "right"].append(value)
    # Some shell versions also keep an explicit enabled-plugin list.
    if isinstance(config.get("plugins"), list):
        result = []
        seen_new = False
        has_new = any(entry_id(e) == PLUGIN_ID for e in config["plugins"])
        for entry in config["plugins"]:
            identity = entry_id(entry)
            if uninstall and identity == PLUGIN_ID:
                continue
            if migrate and not uninstall and identity == LEGACY_ID:
                if has_new:
                    continue
                entry = dict(entry, id=PLUGIN_ID) if isinstance(entry, dict) else PLUGIN_ID
                identity = PLUGIN_ID
            if identity == PLUGIN_ID:
                if seen_new:
                    continue
                seen_new = True
            result.append(entry)
        config["plugins"] = result
    return config


def desktop_quote(path):
    text = str(path).replace("%", "%%")
    for char in ("\\", '"', "`", "$"):
        text = text.replace(char, "\\" + char)
    return '"' + text + '"'


def marked(path, marker):
    return path.is_file() and not path.is_symlink() and marker in path.read_text()


def legacy_owned(plugin, desktop):
    if plugin.is_symlink():
        return False
    if not plugin.exists():
        return marked(desktop, LEGACY_MARKER)
    try:
        manifest = json.loads((plugin / "manifest.json").read_text())
        return manifest.get("id") == LEGACY_ID and (
            manifest.get("author") == "Local Omarchy port (upstream: rainygirl)"
            or marked(desktop, LEGACY_MARKER))
    except (OSError, ValueError):
        return False


def install(args):
    config_home = Path(args.config_home).expanduser().absolute()
    data_home = Path(args.data_home).expanduser().absolute()
    state_home = Path(args.state_home).expanduser().absolute()
    target = config_home / "omarchy/plugins" / PLUGIN_ID
    legacy = target.with_name(LEGACY_ID)
    shell = config_home / "omarchy/shell.json"
    desktop = data_home / "applications" / (PLUGIN_ID + ".desktop")
    old_desktop = desktop.with_name(LEGACY_ID + ".desktop")
    source = Path(__file__).resolve().parent / PLUGIN_ID
    for path in (target, desktop):
        if path.is_symlink():
            raise ValueError(f"Destination is a symlink; refusing to replace: {path}")
    if target.exists() and json.loads((target / "manifest.json").read_text()).get("id") != PLUGIN_ID:
        raise ValueError("The destination belongs to a different plugin.")
    if desktop.exists() and not marked(desktop, MARKER):
        raise ValueError("The desktop launcher is not owned by this installer.")
    if args.uninstall and not target.exists() and not desktop.exists() and not shell.exists():
        print("Oh My Departure is not installed.")
        return
    if shell.is_symlink() and not shell.exists():
        raise ValueError("shell.json is a dangling symlink")
    migrate = not args.uninstall and legacy_owned(legacy, old_desktop)
    config = update_layout(read_config(shell), args.uninstall, args.section, migrate)
    updated = (json.dumps(config, ensure_ascii=False, indent=2) + "\n").encode()
    if not args.uninstall:
        if not shutil.which("omarchy"):
            raise ValueError("Install this plugin from an Omarchy desktop.")
        manifest = json.loads((source / "manifest.json").read_text())
        if manifest.get("id") != PLUGIN_ID:
            raise ValueError("Unexpected bundled plugin ID")
        subprocess.run(["omarchy", "plugin", "validate", str(source)], check=True)
        for name in ("BarWidget.qml", "launch", "bin/ohmydeparture", "libexec/codex-reader.py", "icon.png", "LICENSE"):
            if not (source / name).is_file():
                raise ValueError(f"Incomplete package: {name}")
        subprocess.run([str(source / "bin/ohmydeparture"), "--version"], check=True, capture_output=True)

    backup = state_home / "ohmydeparture-omarchy/backups" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup.mkdir(parents=True)
    paths = [(target, "plugin"), (desktop, "launcher.desktop"), (shell, "shell.json")]
    if migrate and legacy.exists():
        paths.append((legacy, "legacy-plugin"))
    if migrate and marked(old_desktop, LEGACY_MARKER):
        paths.append((old_desktop, "legacy-launcher.desktop"))
    original = {}
    for path, label in paths:
        exists = path.exists()
        original[path] = (label, exists, path.is_dir())
        if exists:
            if path.is_dir():
                shutil.copytree(path, backup / label, symlinks=True)
            else:
                (backup / label).write_bytes(path.read_bytes())
    staged = None
    touched = []
    try:
        if args.uninstall:
            touched.append(target)
            if target.exists():
                shutil.rmtree(target)
        else:
            stage_parent = config_home / "omarchy"
            stage_parent.mkdir(parents=True, exist_ok=True)
            staged = Path(tempfile.mkdtemp(prefix=".ohmydeparture-stage-", dir=stage_parent))
            shutil.copytree(source, staged / "plugin", symlinks=True)
            target.parent.mkdir(parents=True, exist_ok=True)
            touched.append(target)
            if target.exists():
                shutil.rmtree(target)
            os.replace(staged / "plugin", target)
        touched.append(shell)
        if not args.uninstall or shell.exists():
            atomic_write(shell, updated)
        touched.append(desktop)
        if args.uninstall:
            desktop.unlink(missing_ok=True)
        else:
            entry = ("[Desktop Entry]\nType=Application\nName=Oh My Departure\n"
                     "Comment=Read-only Codex sessions and departure boards\n"
                     f"Exec={desktop_quote(target / 'launch')} --windowed --view codex\n"
                     f"Icon={target / 'icon.png'}\nTerminal=false\nCategories=Utility;Amusement;\n"
                     f"StartupWMClass=OhMyDeparture\n{MARKER}\n")
            atomic_write(desktop, entry.encode())
            for path, _ in paths[3:]:
                touched.append(path)
                if path.is_dir():
                    shutil.rmtree(path)
                else:
                    path.unlink(missing_ok=True)
    except Exception:
        for path in reversed(touched):
            label, existed, directory = original[path]
            if directory or path.is_dir():
                if path.exists():
                    shutil.rmtree(path)
                if existed:
                    shutil.copytree(backup / label, path, symlinks=True)
            elif existed:
                atomic_write(path, (backup / label).read_bytes())
            elif path.exists():
                path.unlink()
        raise
    finally:
        if staged is not None:
            shutil.rmtree(staged, ignore_errors=True)
    print(("Removed" if args.uninstall else "Installed and enabled") + f" {PLUGIN_ID}")
    print(f"Backup: {backup}")
    if migrate:
        print("Migrated the managed R Departures plugin; bar position and inline settings were preserved.")
    if not args.uninstall:
        print(f"Direct launch: {target / 'launch'} --view codex --windowed")
        print("Close any older R Departures window once before opening the new application.")
    if not args.no_reload and shutil.which("omarchy-shell"):
        try:
            result = subprocess.run(["omarchy-shell", "shell", "rescanPlugins"], capture_output=True, timeout=4)
            if result.returncode:
                print("Shell IPC is unavailable; file watching will detect the changes.")
        except subprocess.TimeoutExpired:
            print("Shell IPC timed out; file watching will detect the changes.")


def main():
    parser = argparse.ArgumentParser(description="Install/remove Oh My Departure in the Omarchy bar (no sudo).")
    parser.add_argument("--uninstall", action="store_true", help="Remove the plugin and bar entry; keep preferences and backups.")
    parser.add_argument("--section", choices=("left", "center", "right"), help="Bar section; new installs default to right.")
    for option, variable, fallback in (("config", "XDG_CONFIG_HOME", ".config"), ("data", "XDG_DATA_HOME", ".local/share"), ("state", "XDG_STATE_HOME", ".local/state")):
        parser.add_argument(f"--{option}-home", default=os.environ.get(variable) or str(Path.home() / fallback))
    parser.add_argument("--no-reload", action="store_true", help="Skip shell IPC (file watching still works).")
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Run as your regular desktop user, without sudo.")
    try:
        install(args)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Installation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
