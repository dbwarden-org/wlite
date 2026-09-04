---
title: wlite - SQLite Schema and Migration Toolkit
description: A lightweight, declarative SQLite schema toolkit inspired by dbwarden. CLI and language bindings for C, C++, Rust, Python, C#, Go, and Zig.
---

# wlite

Declarative SQLite schema management, without the overhead.

<a href="https://github.com/dbwarden-org/wlite/blob/main/LICENSE">
  <img src="https://img.shields.io/badge/License-MIT-10AC84?style=for-the-badge" alt="License">
</a>
<a href="https://github.com/dbwarden-org/wlite">
  <img src="https://img.shields.io/badge/GitHub-dbwarden--org%2Fwlite-181717?logo=github&style=for-the-badge" alt="GitHub">
</a>
<a href="https://crates.io/crates/wlite">
  <img src="https://img.shields.io/crates/v/wlite?style=for-the-badge&logo=rust" alt="Rust Crate">
</a>
<a href="https://pypi.org/project/wlite/">
  <img src="https://img.shields.io/pypi/v/wlite?style=for-the-badge&logo=python&logoColor=white" alt="PyPI">
</a>
<a href="https://www.nuget.org/packages/wlite">
  <img src="https://img.shields.io/nuget/v/wlite?style=for-the-badge&logo=nuget" alt="NuGet">
</a>

---

wlite is a lightweight SQLite schema and migration toolkit. It provides a CLI and language bindings (C, C++, Rust, Python, C#, Go, Zig) for [libwlite](https://github.com/dbwarden-org/libwlite). Define your database in `.wlite`, then use it from any supported language.

The core engine is written in C as a standalone library with zero runtime dependencies beyond SQLite3 itself. Every language binding goes through the same C ABI, which means behavior is identical whether you call it from Rust, Python, C++, or C#.

## Why wlite exists

[dbwarden](https://github.com/dbwarden-org/dbwarden) is a full-featured declarative schema compiler for SQLAlchemy. It is powerful, but it carries the weight of the Python ecosystem: SQLAlchemy, async drivers, plugin systems, multi-database orchestration.

Not every project needs that. Many projects live in a different world entirely:

**Embedded applications.** Firmware, IoT devices, and embedded Linux systems often use SQLite as their primary data store. These environments have limited memory, no package manager, and no Python runtime. They need a schema toolkit that compiles to C and links directly into the application binary.

**CLI tools.** Command-line utilities that ship with a database benefit from schema management that runs as a single binary. There is no interpreter to locate, no virtual environment to activate, and no dependency to install on the user's machine.

**TUI interfaces.** Terminal user interfaces built with libraries like Bubble Tea, Ratatui, or ncurses often need a local database for state persistence. These applications are distributed as static binaries. Linking against libwlite means the database schema is managed in the same language as the rest of the application.

**Small services.** Not every backend is a Django monolith. Lightweight HTTP servers, worker processes, and background daemons that use SQLite can benefit from schema management without pulling in SQLAlchemy and its transitive dependencies.

**wlite is dbwarden for projects that do not need the dbwarden machinery.**

See [Philosophy](philosophy.md) for the full comparison with other approaches.

## At a glance

- Declarative `.wlite` model format for schema definition
- CLI for init, diff, migrate, query, plan, generate, and more
- Language bindings for C, C++, Rust, Python, C#, Go, and Zig
- All bindings go through the libwlite C ABI for consistent behavior
- SQLite-native: table rebuilds, type normalization, constraint diffing
- Mirrors dbwarden's SQLite3 backend exactly
- Small binaries, fast builds, zero runtime dependencies beyond SQLite3
- MIT licensed, suitable for commercial and open source projects

## Quick start

### Model file

Create `schema.wlite`:

```
model_config {
    name "my_application"
    version 1
}

model User {
    table "users"

    field id integer {
        primary_key
        autoincrement
    }

    field username text {
        not_null
        unique
    }

    field email text {
        not_null
    }

    field created_at datetime {
        not_null
        default CURRENT_TIMESTAMP
    }
}
```

### CLI workflow

```bash
# Build (requires libwlite installed)
cd libwlite && make install
cd ../wlite && make

# Initialize the project
wlite init

# See what migration is needed
wlite diff app.db app.wlite

# Apply the migration
wlite migrate app.db app.wlite

# Verify schema matches
wlite check app.db app.wlite
```

### C

```c
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;

    wlite_model_load_file("app.wlite", &model);
    wlite_open("app.db", &db);
    wlite_migrate(db, model);

    wlite_stmt *stmt;
    wlite_prepare(db, "SELECT * FROM users", &stmt);
    while (wlite_step(stmt) == WLITE_OK)
        printf("%s\n", wlite_column_text(stmt, 0));
    wlite_stmt_finalize(stmt);

    wlite_close(db);
    wlite_model_free(model);
    return 0;
}
```

For other languages, see the dedicated binding pages:

- [C](bindings/c/index.md)
- [Rust](bindings/rust/index.md)
- [Python](bindings/python/index.md)
- [C++](bindings/cpp/index.md)
- [Go](bindings/go/index.md)
- [C#](bindings/csharp/index.md)
- [Zig](bindings/zig/index.md)

## CLI commands

| Command | Description |
|---------|-------------|
| `wlite init` | Create `schema.wlite` and `migrations/` directory in the current project. |
| `wlite diff <db> <schema>` | Compare the live database against the schema model. Prints the SQL needed to bring the database into alignment. |
| `wlite plan <db> <schema>` | Show a human-readable migration plan. Describes what will be created, altered, or dropped without executing any changes. |
| `wlite generate <db> <schema>` | Generate migration SQL and write it to a file in `migrations/`. |
| `wlite compile <schema>` | Compile a `.wlite` model to JSON. Useful for debugging the parsed representation. |
| `wlite query <db> <sql>` | Execute an arbitrary SQL query against the database and print the results. |
| `wlite check <db> <schema>` | Verify that the database schema matches the model. Exits with code 0 if they match, 1 if they differ. |
| `wlite inspect <db>` | Print the full schema of the database including tables, columns, types, constraints, and indexes. |
| `wlite snapshot <db>` | Export the database schema as a JSON snapshot. |
| `wlite hash <db>` | Compute and display a hash of the database schema. Useful for detecting schema changes in CI pipelines. |
| `wlite format <schema>` | Format a `.wlite` model file with consistent indentation and ordering. |
| `wlite version` | Print the version of wlite and the underlying libwlite library. |

All commands that produce output support `--json` for machine-readable JSON output. See [Workflow](workflow.md) for detailed usage.

## Language bindings

| Language | Package | Status | Notes |
|----------|---------|--------|-------|
| C | [libwlite](https://github.com/dbwarden-org/libwlite) | Complete | Core library. All other bindings go through this ABI. |
| C++ | Header-only wrapper | Complete | Wraps the C API with RAII types. |
| Rust | [`wlite` on crates.io](https://crates.io/crates/wlite) | Complete | Idiomatic Rust API with `Result` types. |
| Python | [`wlite` on PyPI](https://pypi.org/project/wlite/) | Complete | Thin wrapper around libwlite. Python 3.8+. |
| C# | [`wlite` on NuGet](https://www.nuget.org/packages/wlite) | Complete | P/Invoke binding with `IDisposable`. .NET 6+. |
| Go | cgo binding | Complete | Uses cgo to call libwlite directly. |
| Zig | C interop | Structured | Uses Zig's C interop. API design in progress. |

## Next steps

- [Philosophy](philosophy.md): The declarative approach and how it compares to alternatives
- [Relationship to dbwarden](relationship-to-dbwaden.md): How wlite relates to dbwarden
- [Workflow](workflow.md): Complete migration workflow and CLI usage
- [Architecture](architecture/index.md): Internal design and data flow
- [Grammar](grammar.md): The `.wlite` model format specification
- [C API Reference](c-api.md): Complete API reference for libwlite
