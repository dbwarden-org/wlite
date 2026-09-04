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
- Small binaries, fast builds, zero runtime dependencies beyond SQLite3
- MIT licensed, suitable for commercial and open source projects

## Quick start

### Install

**From source (C library + CLI):**

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

**Using CMake:**

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
mkdir build && cd build
cmake ..
cmake --build .
```

**Python (from PyPI):**

```bash
pip install wlite
```

**Rust (from crates.io):**

```bash
cargo add wlite
```

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

The `.wlite` format uses a concise DSL. Each `model` block defines a table. Each `field` block defines a column with its type and constraints. The `model_config` block provides metadata about the schema.

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

### Python

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")
db.migrate(model)

rows = db.query("SELECT * FROM users")
for row in rows:
    print(row["username"])
```

### Rust

```rust
use wlite::{Database, Model};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let model = Model::load("app.wlite")?;
    let mut db = Database::open("app.db")?;
    db.migrate(&model)?;

    let stmt = db.prepare("SELECT * FROM users")?;
    for row in stmt {
        println!("{}", row?.column_text(0));
    }
    Ok(())
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
| `wlite migrate <db> <schema>` | Apply schema migrations to the database. Parses the model, computes the diff, plans the migration, and executes it within a transaction. |
| `wlite diff <db> <schema>` | Compare the live database against the schema model. Prints the SQL needed to bring the database into alignment. Supports `--json` for machine-readable output. |
| `wlite plan <db> <schema>` | Show a human-readable migration plan. Describes what will be created, altered, or dropped without executing any changes. Supports `--json`. |
| `wlite generate <db> <schema>` | Generate migration SQL and write it to a file in `migrations/`. Supports `--name` to set the migration name and `--yes` to skip confirmation. |
| `wlite compile <schema>` | Compile a `.wlite` model to `.wlitem` binary format. Supports `-o` to specify the output file. |
| `wlite query <db> <sql>` | Execute an arbitrary SQL query against the database and print the results. Supports `--json` for JSON output. |
| `wlite check <db> <schema>` | Verify that the database schema matches the model. Exits with code 0 if they match, 1 if they differ. |
| `wlite inspect <db>` | Print the full schema of the database including tables, columns, types, constraints, and indexes. Supports `--json`. |
| `wlite snapshot <db>` | Export the database schema as a JSON snapshot. Supports `--json`. |
| `wlite rollback <db>` | Roll back the last migration. Supports `--steps N` to roll back multiple migrations. |
| `wlite status <db>` | Show the current schema hash of the database. Useful for detecting whether the schema matches the expected state. |
| `wlite hash <db>` | Compute and display a hash of the database schema. Useful for detecting schema changes in CI pipelines. |
| `wlite format <schema>` | Format a `.wlite` model file with consistent indentation and ordering. |
| `wlite version` | Print the version of wlite and the underlying libwlite library. |

All commands that produce output support `--json` for machine-readable JSON output. See [Workflow](workflow.md) for detailed usage.

## How it works

wlite uses a declarative approach. You define your desired schema in a `.wlite` model file. wlite compares the model against the live database, computes the diff, and generates the SQL needed to bring the database into alignment.

### The pipeline

```
  .wlite model file
         |
    [Parse]
         |
    WlSchema (model)
         |
    [Introspect DB]
         |
    WlSchema (database)
         |
    [Diff]
         |
      WlDiff
         |
    [Plan]
         |
      WlPlan
         |
    [Migrate]
         |
   Database updated
```

Each stage is independent. You can run `wlite diff` to see what changes are needed without applying them. You can run `wlite plan` to see the migration steps. You can run `wlite migrate` to apply everything at once.

### Table rebuilds

SQLite supports a limited set of ALTER TABLE operations. When a change requires more than SQLite can handle directly (column type changes, constraint modifications, NOT NULL without defaults), wlite automatically generates a full table rebuild:

1. Create a staging table with the new schema
2. Copy all data from the old table
3. Drop the old table
4. Rename the staging table
5. Recreate all indexes

This is handled entirely by wlite. You do not need to write rebuild SQL manually.

### Type normalization

SQLite stores column types as affinity rules. wlite normalizes equivalent types during comparison so that changing `INT` to `INTEGER` in your model does not trigger a spurious migration. The full normalization table is documented in the [Data Flow](architecture/data-flow.md) page.

## Language bindings

| Language | Package | Status | Notes |
|----------|---------|--------|-------|
| C | [libwlite](https://github.com/dbwarden-org/libwlite) | Complete | Core library. All other bindings go through this ABI. |
| C++ | Header-only wrapper | Complete | Wraps the C API with RAII types. |
| Rust | [`wlite` on crates.io](https://crates.io/crates/wlite) | Complete | Idiomatic Rust API with `Result` types. |
| Python | [`wlite` on PyPI](https://pypi.org/project/wlite/) | Complete | Thin wrapper around libwlite. Python 3.10+. |
| C# | [`wlite` on NuGet](https://www.nuget.org/packages/wlite) | Complete | P/Invoke binding with `IDisposable`. .NET 6+. |
| Go | cgo binding | Complete | Uses cgo to call libwlite directly. |
| Zig | C interop | Structured | Uses Zig's C interop. API design in progress. |

All bindings go through the same C ABI. Behavior is identical across languages. There is no reimplemented logic in any binding.

## Project structure

```
wlite/
├── cli/
│   └── main.c              # CLI entry point
├── include/
│   └── wlite.h             # Public C API header
├── wlite/
│   ├── schema.c            # Schema lifecycle and model API
│   ├── parser.c            # .wlite DSL parser
│   ├── introspect.c        # Database schema reader
│   ├── diff.c              # Schema comparison
│   ├── planner.c           # Migration plan generation
│   ├── migrate.c           # Migration execution
│   ├── query.c             # Prepared statement wrapper
│   ├── record.c            # Row access helpers
│   ├── tx.c                # Transaction management
│   ├── compile.c           # .wlite to .wlitem compiler
│   ├── serialize.c         # JSON and DSL output
│   └── schema_inspect.c    # Live database introspection
├── tests/
│   ├── test_wlite.c        # Core unit tests
│   ├── test_edge_cases.c   # Edge case tests
│   └── conformance.c       # Cross-language conformance tests
├── bindings/
│   ├── python/             # Python binding (ctypes)
│   ├── rust/               # Rust binding (FFI)
│   ├── go/                 # Go binding (cgo)
│   ├── csharp/             # C# binding (P/Invoke)
│   └── zig/                # Zig binding (cImport)
├── docs/                   # Documentation
├── examples/               # Usage examples
├── CMakeLists.txt          # CMake build
└── Makefile                # Simple make build
```

## Next steps

- [Philosophy](philosophy.md): The declarative approach and how it compares to alternatives
- [Relationship to dbwarden](relationship-to-dbwaden.md): How wlite relates to dbwarden
- [Workflow](workflow.md): Complete migration workflow and CLI usage
- [Architecture](architecture/index.md): Internal design and data flow
- [Grammar](grammar.md): The `.wlite` model format specification
- [C API Reference](c-api.md): Complete API reference for libwlite
