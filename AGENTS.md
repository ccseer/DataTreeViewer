# AGENTS.md — DataTreeViewer

Seer plugin for read-only tree-structured data preview (JSONC, YAML, INI, TOML).
C++17 · Qt 6.8 · MSVC · CMake 3.16+

**Reference repo:** `C:\Users\corey\OneDrive\TODO\github\JsonTreeViewer`
(threading model, BreadcrumbBar, SearchBar ported from here)

---

## Build & Test

```powershell
cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
cmake --build build --config Release
cmake --build build --config Debug --target datatreeviewer_demo
ctest --test-dir build --build-config Debug
```

Enable/disable tests: `-DBUILD_TESTS=OFF`

---

## Architecture

Strictly layered with enforced dependency rules:

```
Seer Plugin Host (createWidget → QWidget*)
└── DataTreeViewer          ← top-level widget + thread management
    ├── ParseWorker          ← background QThread, I/O + parsing
    │   └── IFormatParser    ← parser interface (core/)
    ├── TreeRenderer         ← QTreeWidget, knows ConfigNode only
    ├── BreadcrumbBar        ← ported from JsonTreeViewer
    └── SearchBar            ← ported from JsonTreeViewer
```

**Dependency rules (enforced by include discipline):**
- `core/` and `parsers/` — zero Qt headers, zero third-party UI headers
- `ParseWorker` — knows `IFormatParser`, `ConfigNode`; no UI or parser headers
- `TreeRenderer` — knows `ConfigNode` only; no parser headers
- `DataTreeViewer` — the only file that includes both `core/` and `ui/` headers

### v1 Non-Goals
- No editing/serialization back to disk
- No large-file pagination (no `FetchWorker`)
- No memory-mapped I/O (`QFile::readAll` is fine)
- No ANSI/CP1252 transcoding (UTF-8 only)

---

## Directory Structure (planned)

```
DataTreeViewer/
├── CMakeLists.txt
├── plugin_main.cpp              ← DLL entry point (4 export C functions)
├── AGENTS.md
└── src/
    ├── core/                    ← zero Qt, zero UI deps
    │   ├── config_node.h        ← IR struct (Type enum, children vector)
    │   ├── iformat_parser.h     ← IFormatParser pure virtual + ParseResult
    │   └── parser_registry.h/.cpp ← singleton, macro-based registration
    ├── parsers/                 ← zero Qt
    │   ├── jsonc_parser.h/.cpp  ← nlohmann/json
    │   ├── yaml_parser.h/.cpp   ← rapidyaml
    │   ├── ryml_impl.cpp        ← single-header define-once TU
    │   ├── ini_parser.h/.cpp    ← SimpleIni
    │   └── toml_parser.h/.cpp   ← toml++
    ├── workers/
    │   └── parse_worker.h/.cpp  ← background QThread worker
    ├── ui/
    │   ├── tree_renderer.h/.cpp ← QTreeWidget, knows ConfigNode only
    │   ├── breadcrumb_bar.h/.cpp
    │   ├── search_bar.h/.cpp
    │   └── data_tree_viewer.h/.cpp ← top-level widget
    ├── third_party/
    │   ├── nlohmann/json.hpp
    │   ├── tomlplusplus/toml.hpp
    │   ├── SimpleIni/SimpleIni.h
    │   └── ryml/ryml_all.hpp
    └── demo_host.cpp
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
| Directories | `snake_case` | `src/core/`, `src/third_party/` |
| Macros | `ALL_CAPS` | `REGISTER_PARSER` |

### Formatting
- 4-space indentation
- Opening brace on same line for functions and control structures
- `public:` / `private:` indented one level inside class body
- `//` comments for inline docs

### Memory & Ownership
- `std::shared_ptr<const ParseResult>` through queued connections (avoids deep copy)
- `ParseWorker` uses `deleteLater()` + `QScopeGuard` — never `delete` manually
- Background `QThread` has no QObject parent; self-destructs via `QThread::finished → deleteLater`
- Must call `qRegisterMetaType<std::shared_ptr<const ParseResult>>()` in plugin init

---

## Key Patterns

### Adding a New Format
1. Add third-party header to `src/third_party/`
2. Create `src/parsers/xxx_parser.h/.cpp`, implement `IFormatParser`
3. Add `REGISTER_PARSER("ext", XxxParser)` at bottom of `.cpp`
4. Add files to CMakeLists sources
5. Done — zero changes to core, registry, worker, renderer, or widget

### Cancellation (non-blocking)
`cancelPending()` bumps a `m_generation` counter — never calls `QThread::wait()`. Old thread finishes naturally, emits `parseCompleted`, then a generation-check lambda discards stale results. Old thread self-destructs via `quit()` + `deleteLater()`.

```
cancelPending() → m_generation++
loadFile() → connect worker::parseCompleted → lambda(generation check)
  if (gen != m_generation) return;  // discard stale
```

### Parser Lifetime Contract
Parsers MUST copy all strings into `std::string` before `parse()` returns. The input buffer may be freed immediately after — no references, no string_views, no in-place parsing across the return boundary.

### Plugin Entry Point (`plugin_main.cpp`)
Four exported C functions via `extern "C" __declspec(dllexport)`:
- `createWidget(filePath, parent) → QWidget*`
- `destroyWidget(widget)`
- `supportedExtensions() → const char*`
- `pluginVersion() → int`

---

## Implementation Phases (from spec §16)

1. Core types + registry (no Qt)
2. JsoncParser + unit tests
3. ParseWorker + DataTreeViewer shell with cancellation (JSONC only)
4. TreeRenderer with style, expand, array keys, soft limit
5. TomlParser
6. IniParser
7. YamlParser + ryml_impl.cpp
8. BreadcrumbBar + SearchBar (ported from JsonTreeViewer)
9. Status bar polish
10. Encoding (UTF-8 BOM strip)
11. File size guard + metatype registration + plugin exports

---

## Third-Party Libraries (header-only in `src/third_party/`)

| Library | Min Version | Notes |
|---------|------------|-------|
| nlohmann/json | 3.11 | strips comments (v1 limitation) |
| toml++ | 3.4 | ISO 8601 datetimes → String |
| SimpleIni | 4.22 | case-sensitive keys, duplicates not merged |
| rapidyaml | 0.7 | requires `ryml_impl.cpp` TU with `RYML_SINGLE_HDR_DEFINE_NOW` |
