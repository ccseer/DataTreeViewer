# DataTreeViewer

**A multi-format tree-structured data viewer for Windows** — built as a native plugin for [Seer](https://1218.io), the quick-look file preview tool.

DataTreeViewer lets you preview and navigate JSONC, TOML, INI, and YAML files in a beautiful tree view — press Space on any supported file to see its structure instantly.

## Features

- **Multi-format support**: JSONC, TOML, INI, YAML — auto-detected by file extension
- **Async parsing**: Background-thread parsing keeps the UI responsive
- **Interactive breadcrumb navigation**: IDE-style breadcrumb bar for quick navigation through deeply nested structures
- **Real-time search**: Search bar with live filtering and result navigation
- **Status bar metrics**: Detailed file statistics at a glance

## Building

Requirements: Qt 6.8, CMake 3.16+, MSVC (Visual Studio 2022)

Open the project folder in your IDE (Visual Studio or VS Code with CMake Tools), select `datatreeviewer_test` as the startup item, then build and run.

This produces:
- `datatreeviewer.dll` — the Seer plugin

To run parser tests:

## Use with Seer

**[Seer](https://1218.io)** is the ultimate quick-look file preview tool for Windows — press Space on any file to instantly preview it without opening a full application.

1. Download **[Seer](https://1218.io)**
2. In Seer, go to **Settings → Plugins** and install the _DataTreeViewer_ plugin
3. Replace the installed plugin DLL with your freshly built `datatreeviewer.dll`
4. Press Space on any `.jsonc`, `.toml`, `.ini`, or `.yaml` file

## License

MIT © 2026 ccseer
