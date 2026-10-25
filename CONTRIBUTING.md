# Contributing

Use English for source comments, default UI strings, issues and pull requests.
Translations are welcome as secondary documentation. Keep user-provided session
text unchanged, including non-Latin text.

## Build and checks

Use C++17, Qt 6 Widgets/Network/Test, make, pkg-config, Python 3 and binutils.
Native GUI checks also require Xvfb, xauth, xdotool, ImageMagick and CJK fonts.
On Omarchy, the installed Quickshell/Omarchy shell enables the optional widget test.

```sh
make -j2 test
make test-omarchy
xvfb-run -a -s '-screen 0 1920x1080x24' env QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME= QT_IM_MODULE= .build/linux/codex-smoke
make test-desktop
xvfb-run -a -s '-screen 0 1920x1080x24' bash tests/omarchy-widget.sh
```

Run Xvfb checks sequentially. Test scripts isolate settings, runtime directories
and Codex history. The installer tests use the native Omarchy validator when
available, or a test-only stub on CI plus the portable bundle validator. They never
install into the user's real desktop profile. The coordinate-driven desktop test
uses a 1920×1080 virtual screen and may need adjustment with different system fonts;
CI runs the Qt UI checks rather than relying on those mouse coordinates.

## Layout

- `src/`: shared split-flap renderer and inherited travel boards.
- `platform/qt/`: Qt compatibility layer, windows, settings and UI socket.
- `platform/codex/reader.py`: standard-library read-only Codex adapter.
- `packaging/omarchy/`: widget, bundled installer and installation guides.
- `tests/`: synthetic fixtures and regression checks.
- `tools/`: packaging and validation; original Haiku tools retain their names.

Keep protocol calls on the reader's allowlist. Do not add automatic approvals,
agent mutations, command execution from history, or writes to Codex data. Changes
to state interpretation need tests that distinguish history, idle and turn outcome.
Use synthetic fixtures for screenshots and bug reports; remove private paths,
prompts and outputs. Include expected behaviour and relevant test results in PRs.

Changes to installer ownership or migration must cover foreign files, symlinks,
backup/rollback and existing inline settings. Keep the legacy ID solely for
migration and preserve original copyright and font licenses.

For releases, follow [docs/PUBLISHING.md](docs/PUBLISHING.md).
