# AGENTS.md — DataTreeViewer

Seer plugin for read-only tree-structured data preview (JSONC, YAML, INI, TOML).
C++17 · Qt 6.8 · MSVC · CMake 3.16+

**Reference repo:** `../JsonTreeViewer`
(threading model, BreadcrumbBar, SearchBar ported from here)

---

## Build & Test

```powershell
cmake -B build -DCMAKE_PREFIX_PATH=C:/Users/corey/Dev/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
cmake --build build --config Debug --target datatreeviewer_test
ctest --test-dir build --build-config Debug
```

Enable/disable tests: `-DBUILD_TESTS=OFF`

---

## Architecture

Strictly layered with enforced dependency rules:

```
Seer Plugin Host (QPluginLoader → ViewerPluginInterface)
└── DTPlugin                ← plugin entry point, creates DataTreeViewer
    └── DataTreeViewer      ← ViewerBase subclass, thread management
        ├── ParseWorker     ← background QThread, I/O + parsing
        │   └── IFormatParser ← parser interface (core/)
        ├── TreeRenderer    ← QTreeWidget, knows ConfigNode only
        ├── BreadcrumbBar   ← ported from JsonTreeViewer
        ├── SearchBar       ← ported from JsonTreeViewer
        └── StatusBar       ← format stats, error, filter hits
```

**Dependency rules (enforced by include discipline):**
- `core/` and `parsers/` — zero Qt headers, zero third-party UI headers
- `ParseWorker` — knows `IFormatParser`, `ConfigNode`; no UI or parser headers
- `TreeRenderer` — knows `ConfigNode` only; no parser headers
- `DataTreeViewer` — the only file that includes both `core/` and `ui/` headers; also includes `seer/viewerbase.h`

### v1 Non-Goals
- No editing/serialization back to disk
- No large-file pagination (no `FetchWorker`)
- No memory-mapped I/O (`QFile::readAll` is fine)
- No ANSI/CP1252 transcoding (UTF-8 only)

---

## Directory Structure

```
DataTreeViewer/
├── CMakeLists.txt
├── AGENTS.md
├── bin/
│   └── plugin.json            ← Seer plugin metadata (copied to build dir)
└── src/
    ├── core/                  ← zero Qt, zero UI deps
    │   ├── config_node.h      ← IR struct (Type enum, children vector)
    │   ├── iformat_parser.h   ← IFormatParser pure virtual + ParseResult
    │   └── parser_registry.h/.cpp ← singleton, macro-based registration
    ├── parsers/               ← zero Qt
    │   ├── jsonc_parser.h/.cpp ← nlohmann/json
    │   ├── yaml_parser.h/.cpp  ← rapidyaml
    │   ├── ini_parser.h/.cpp   ← SimpleIni
    │   └── toml_parser.h/.cpp  ← toml++
    ├── workers/
    │   ├── background_thread.h ← QThread with quit+wait in destructor
    │   └── parse_worker.h/.cpp ← QObject, moved to BackgroundThread
    ├── ui/
    │   ├── data_tree_viewer.h/.cpp ← ViewerBase subclass + DTPlugin
    │   ├── tree_renderer.h/.cpp    ← QTreeWidget, knows ConfigNode only
    │   ├── breadcrumb_bar.h/.cpp
    │   ├── search_bar.h/.cpp
    │   └── status_bar.h/.cpp
    └── test.cpp               ← Seer integration test executable
tests/
    ├── CMakeLists.txt
    ├── test_jsonc_parser.cpp
    ├── test_yaml_parser.cpp
    ├── test_ini_parser.cpp
    ├── test_toml_parser.cpp
    └── fixtures/
```

---

## Coding Conventions

### Naming
| Category | Convention | Example |
|----------|-----------|---------|
| Classes / Structs | `PascalCase` | `ConfigNode`, `TreeRenderer` |
| Member variables | `m_` prefix + `camelCase` | `m_thread`, `m_generation` |
| Constants | `k` prefix + `PascalCase` | `kMaxFileBytes` |
| Enums | `enum class` with `PascalCase` | `Type::Object`, `Type::Array` |
| Files | `snake_case` | `config_node.h`, `parse_worker.cpp` |
| Directories | `snake_case` | `src/core/` |
| Macros | `ALL_CAPS` | `REGISTER_PARSER` |

### Formatting
- 4-space indentation
- Opening brace on same line for functions and control structures
- `public:` / `private:` indented one level inside class body
- `//` comments for inline docs

### Debug Logging
Each translation unit defines a `qprintt` macro at the top:
```cpp
#define qprintt qDebug() << "[ClassName]"
```
Used for trace-level lifecycle logging (ctor, dtor, load, errors). No `qprintt` in `core/` or `parsers/`.

### Memory & Ownership
- `std::shared_ptr<const ParseResult>` through queued connections (avoids deep copy)
- `ParseWorker` uses `QScopeGuard` + `deleteLater()` — never `delete` manually
- `BackgroundThread` has no QObject parent; self-destructs via chain:
  1. `QScopeGuard` → `worker->deleteLater()`
  2. `QObject::destroyed` (worker) → `thread->deleteLater()`
  3. `~BackgroundThread { quit(); wait(); }`
- `qRegisterMetaType<std::shared_ptr<const ParseResult>>()` called once in `init()`

---

## Key Patterns

### Adding a New Format
1. Create `src/parsers/xxx_parser.h/.cpp`, implement `IFormatParser`
2. Add `REGISTER_PARSER("ext", XxxParser)` at bottom of `.cpp`
3. Add files to `PARSER_SOURCES` in CMakeLists
4. Done — zero changes to core, registry, worker, renderer, or widget

### Seer Plugin Entry Point (`data_tree_viewer.h`)
`DTPlugin` class implements `ViewerPluginInterface` (from SeerSdk). Qt discovers it via `Q_PLUGIN_METADATA` — no `extern "C"` exports needed:

```cpp
class DTPlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "../../bin/plugin.json")
    Q_INTERFACES(ViewerPluginInterface)
public:
    ViewerBase* createViewer(QWidget* parent = nullptr) override;
};
```

`plugin.json` is copied to the DLL output directory via `POST_BUILD` custom command.

### ViewerBase Lifecycle
`DataTreeViewer` inherits `ViewerBase` (SeerSdk). The host calls `load(ctrl_bar, options)`:
1. `ViewerBase::load()` stores the `ViewOptions*`, calls `loadImpl(layout, ctrl_bar)`
2. `loadImpl()` calls `init()` once (lazy), adds widgets to layout, reads `options()->path()` and `options()->theme()` / `options()->dpr()`, then calls `doLoadFile()`
3. `options()` pointer is only accessed during the synchronous `loadImpl()` call — never stored

Must override:
- `QString name() const` — `"DataTreeViewer"`
- `QSize getContentSize() const` — `{960, 700}`
- `void loadImpl(QBoxLayout*, QHBoxLayout*)` — widget setup + load trigger

Optional overrides (theme/DPI):
- `void updateDPR(qreal)` — rescale fonts, reapply styles
- `void updateTheme(int)` — switch dark/light palette

### State Signaling
Use `sigCommand(VCT_StateChange, ...)` to notify the Seer host about lifecycle:
- `VCV_Loading` — set by `ViewerBase::load()` automatically
- `VCV_Loaded` — emit after successful parse in `onParseCompleted()`
- `VCV_Error` — emit on unsupported format or parse failure

### Cancellation (non-blocking)
`cancelPending()` bumps a `m_generation` counter — never calls `QThread::wait()`. Old thread finishes naturally, emits `parseCompleted`, then a generation-check lambda discards stale results. Old thread self-destructs via `BackgroundThread` destructor chain.

```
cancelPending() → emit cancelRequested() + m_generation++
doLoadFile() → connect worker::parseCompleted → lambda(generation check)
  if (gen != m_generation) return;  // discard stale
```

### Parser Lifetime Contract
Parsers MUST copy all strings into `std::string` before `parse()` returns. The input buffer may be freed immediately after — no references, no string_views, no in-place parsing across the return boundary.

### ParseWorker Flow
1. Open file + size check (max 64 MB)
2. Strip UTF-8 BOM if present
3. Capture parser metadata (`format_name`, `library_credit`)
4. Parse → `ParseResult` (owns `ConfigNode` tree + error info)
5. Count total nodes (non-recursive DFS)
6. Emit `parseCompleted` with elapsed time
At each step, check `QThread::isInterruptionRequested()` for cooperative cancellation.

---

## Implementation Phases (from spec §16)

1. Core types + registry (no Qt)
2. JsoncParser + unit tests
3. ParseWorker + DataTreeViewer shell with cancellation (JSONC only)
4. TreeRenderer with style, expand, array keys, soft limit
5. TomlParser
6. IniParser
7. YamlParser (FetchContent rapidyaml)
8. BreadcrumbBar + SearchBar (ported from JsonTreeViewer)
9. Status bar polish
10. Encoding (UTF-8 BOM strip)
11. File size guard + metatype registration
12. SeerSdk integration — ViewerBase, DTPlugin, plugin.json, sigCommand, theme/DPI

---

## Third-Party Libraries (all via FetchContent)

| Library | Version | Notes |
|---------|---------|-------|
| **SeerSdk** | main | ViewerBase, ViewerPluginInterface, ViewOptions |
| nlohmann/json | 3.11.3 | strips comments (v1 limitation) |
| toml++ | 3.4.0 | ISO 8601 datetimes → String; `TOML_EXCEPTIONS=0` |
| SimpleIni | 4.22 | case-sensitive keys, duplicates not merged |
| rapidyaml | 0.7.2 | FetchContent with c4core submodule, exceptions required |
