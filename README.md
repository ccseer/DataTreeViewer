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

```powershell
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
cmake --build build --config Release
```

This produces:
- `datatreeviewer.dll` — the Seer plugin
- `datatreeviewer_demo.exe` — standalone demo viewer

To run tests:

```powershell
cmake --build build --config Debug --target datatreeviewer_demo
ctest --test-dir build --build-config Debug
```

## Use with Seer

**[Seer](https://1218.io)** is the ultimate quick-look file preview tool for Windows — press Space on any file to instantly preview it without opening a full application.

1. Download **[Seer](https://1218.io)**
2. Copy `datatreeviewer.dll` to your Seer plugins directory
3. Press Space on any `.jsonc`, `.toml`, `.ini`, or `.yaml` file

## License

MIT © 2026 ccseer
