---
title: Architecture Overview
description: Comprehensive architectural overview of wlite, its design principles, module structure, data flow, and relationship to dbwarden.
---

# Architecture Overview

wlite is a SQLite migration and query tool built in C. It is split into two layers: the `wlite` CLI (a thin command-line wrapper) and `libwlite` (the C library that does all the real work). Language bindings talk directly to libwlite through its C ABI. The architecture is deliberately minimal: no build system generated code, no runtime dependencies beyond SQLite3, and no hidden behavior.

This document describes the system from top to bottom: principles, layout, modules, data flow, complexity hotspots, performance characteristics, build system, and testing approach.

## Design Principles

wlite follows three design principles inherited from dbwarden. These principles are non-negotiable and every design decision flows from them.

### 1. Single Source of Truth

The `.wlite` model file is the only thing you edit. Everything else (SQL, diffs, snapshots, migration plans) is derived from it. There is no separate SQL file to maintain, no ORM configuration, and no generated migration artifacts that drift out of sync. When you change the model, wlite computes the diff and generates the SQL. When you run the SQL against a live database, wlite can introspect the result and confirm the state matches the model.

This eliminates an entire class of bugs: the ones where your migration file says one thing but your schema says another. There is only one truth, and it lives in the `.wlite` file.

### 2. Plain SQL Output

No runtime, no generated Python, no hidden ORM behavior, no DSL that only works inside a specific framework. The output is SQL you can read, review, and execute anywhere. You can pipe it into `sqlite3`, into a CI pipeline, into a deployment script, or into a manual review process. The SQL is standard SQLite and nothing else.

This means wlite is safe to adopt incrementally. You do not need to rewrite your deployment process. You just add `wlite plan` to the beginning and `wlite migrate` to the end.

### 3. Small and Composable

libwlite is a single C library with no dependencies beyond SQLite3. Bindings are thin wrappers. The CLI is a thin shell. Complexity lives in the algorithm, not the framework. The entire library is roughly 12 source files. Each file handles one stage of the pipeline. You can read the whole thing in an afternoon.

This principle also means wlite composes well with other tools. It does not try to be your ORM, your query builder, your connection pool, or your migration runner. It plans and executes SQL migrations, and it runs queries. That is it.

## System Layout

The system has three layers: the CLI, the C library, and the language bindings. Here is the full picture:

```
                     *.wlite model file
                            |
                            v
                   +-------------------+
                   |    wlite CLI      |
                   |   (thin shell)    |
                   +--------+----------+
                            |
                    calls libwlite C API
                            |
                            v
                   +-------------------+
                   |    libwlite       |
                   |   (C library)     |
                   +--------+----------+
                            |
                    uses SQLite3 C API
                            |
                            v
                   +-------------------+
                   |     SQLite3       |
                   |  (embedded DB)    |
                   +-------------------+
```

The CLI reads `.wlite` files, calls libwlite functions, and prints results. It contains no schema logic, no parsing, and no migration planning. All of that lives in libwlite.

Language bindings bypass the CLI entirely and talk directly to libwlite:

```
+-------------------+     +-------------------+     +-------------------+
|  Python binding   |     |   Rust binding    |     |    Go binding     |
|    (ctypes)       |     |   (FFI / cc)      |     |     (cgo)         |
+--------+----------+     +--------+----------+     +--------+----------+
         |                         |                         |
         +-------------------------+-------------------------+
                                   |
                                   v
                          +-------------------+
                          |    libwlite       |
                          |   (C library)     |
                          +--------+----------+
                                   |
                                   v
                          +-------------------+
                          |     SQLite3       |
                          +-------------------+
```

Every binding wraps the same set of C functions. No binding reimplements wlite semantics. This is critical: the diff algorithm, the planner, the migration executor, and the query engine all live in exactly one place.

## Language Bindings

wlite provides bindings for six languages, all going through the C ABI:

```
libwlite C ABI
      |
      +-- C/C++ (direct / header-only wrapper)
      +-- Rust  (FFI via cc crate)
      +-- Python (ctypes)
      +-- Go (cgo)
      +-- C# (P/Invoke)
      +-- Zig (@cImport)
```

Each binding exposes the same core operations:

| Operation | C Function | Description |
|-----------|------------|-------------|
| Load model | `wl_schema_from_file()` | Parse a `.wlite` file into a `WlSchema` |
| Open database | `wl_db_open()` | Open a SQLite database file |
| Introspect | `wl_schema_from_db()` | Read the live database schema into a `WlSchema` |
| Diff | `wl_diff()` | Compare two `WlSchema` instances |
| Plan | `wl_plan()` | Convert a diff into ordered SQL statements |
| Migrate | `wl_migrate()` | Execute a plan within a transaction |
| Query | `wl_query()` | Run a SELECT with parameter binding |
| Transaction | `wl_tx_begin()` / `wl_tx_commit()` / `wl_tx_rollback()` | Manual transaction control |

The C API is documented in the header file `wlite.h`. The binding wrappers are intentionally thin: they translate between the host language's types and the C types, handle memory ownership, and nothing else.

## Module Map

libwlite is organized into focused modules. Each module handles one stage of the pipeline. The table below lists every module, the stage it belongs to, and its purpose.

| Module | Stage | Purpose |
|--------|-------|---------|
| `parser.c` | Input | Parse `.wlite` DSL into a `WlSchema` tree |
| `schema.c` | Core | Schema lifecycle, memory management, model API |
| `introspect.c` | Input | Read a live SQLite database into a `WlSchema` |
| `diff.c` | Compare | Compare two `WlSchema` instances, produce a `WlDiff` |
| `planner.c` | Plan | Convert a `WlDiff` into an ordered `WlPlan` of SQL statements |
| `migrate.c` | Execute | Run the plan within a transaction, record checksums, verify |
| `query.c` | Query | Prepared statements, parameter binding, result iteration |
| `record.c` | Query | Generic row access, column by name or index |
| `tx.c` | Query | Transactions and savepoints |
| `compile.c` | Format | Compile `.wlite` to `.wlitem` binary model format |
| `serialize.c` | Format | JSON and DSL serialization of schemas and diffs |
| `schema_inspect.c` | Bridge | Live database to `WlSchema` conversion (shared with introspect) |

The modules have a strict dependency order. `parser.c` depends on `schema.c`. `diff.c` depends on `schema.c`. `planner.c` depends on `diff.c`. `migrate.c` depends on `planner.c`. `query.c` depends on `schema.c`. No module depends on a module that comes after it in the pipeline. This makes the dependency graph a clean DAG with no cycles.

## Data Flow Summary

The complete pipeline from model to SQL follows five stages. Each stage is documented in detail in the linked subpages.

### Stage 1: Parse

`parser.c` reads a `.wlite` file and produces a `WlSchema`. The parser is a hand-written recursive descent parser. It does not use lex or yacc. The grammar is small enough that a hand-written parser is clearer and more maintainable.

The output is a `WlSchema` struct containing an array of `WlTable` structs, each containing an array of `WlColumn` and `WlIndex` structs. All strings are interned in a single arena allocator, so freeing a schema is a single operation.

### Stage 2: Introspect

`introspect.c` reads the live SQLite database using `PRAGMA table_info()` and `PRAGMA index_list()`. It produces a second `WlSchema` that represents the current state of the database. This schema is compared against the parsed schema in the next stage.

Introspection is read only. It never modifies the database. It handles SQLite's various quirks around type affinity, default values, and constraint representation.

### Stage 3: Diff

`diff.c` compares the two `WlSchema` instances and produces a `WlDiff`. The diff contains lists of tables to add, tables to remove, tables to modify, and tables to rebuild. Each modified table has a list of column changes and index changes.

The diff algorithm walks both schemas in parallel, matching tables and columns by name. It does not try to detect renames or moves. Renames are ambiguous and unreliable to detect, so wlite treats them as drop plus add.

### Stage 4: Plan

`planner.c` converts the diff into an ordered `WlPlan` of SQL statements. The planner handles the ordering constraints: tables must be created before they can be referenced, columns must be added before they can have defaults, indexes must be created after their tables exist.

The planner also handles the cases where SQLite's ALTER TABLE is insufficient. When a column type change, a NOT NULL constraint addition without a default, or a constraint modification is detected, the planner generates a full table rebuild instead of a simple ALTER TABLE. These rebuilds are described in detail in the [SQLite Patterns](sqlite-patterns.md) page.

### Stage 5: Migrate

`migrate.c` executes the plan within a transaction. If any statement fails, the entire migration is rolled back. On success, the plan is recorded in the `wlite_meta` table with a checksum, so subsequent runs can detect whether the migration has already been applied.

The migration executor is intentionally simple. It runs SQL statements in order. It does not retry, does not back off, and does not do anything clever. The cleverness is in the planner.

## Where Complexity Lives

Most SQLite schema tools are simple: they diff two schemas and produce ALTER TABLE statements. The complexity in wlite (and its predecessor dbwarden) comes from the cases where SQLite's ALTER TABLE is not enough.

### Table Rebuilds

SQLite supports a limited set of ALTER TABLE operations:

- `ALTER TABLE ... ADD COLUMN`
- `ALTER TABLE ... RENAME COLUMN`
- `ALTER TABLE ... RENAME TABLE`
- `ALTER TABLE ... DROP COLUMN` (SQLite 3.35.0+)

Everything else requires rebuilding the table. This includes:

- Changing a column type
- Adding a NOT NULL column without a DEFAULT value
- Changing a constraint (CHECK, UNIQUE, PRIMARY KEY)
- Reordering columns
- Dropping a column in older SQLite versions

A table rebuild in SQLite works like this:

1. Create a new temporary table with the desired schema
2. Copy all data from the old table into the new table
3. Drop the old table
4. Rename the temporary table to the original name
5. Recreate all indexes

This is expensive for large tables, but it is the only way to handle these cases in SQLite. wlite automates this entirely.

### Collapse Logic

When a table needs multiple changes (for example, both a column type change and a new index), wlite collapses them into a single rebuild. It would be incorrect and inefficient to rebuild the table once for the column change and then again for the constraint change. The collapse logic in `planner.c` detects when a table is scheduled for multiple rebuilds and merges them into one.

The collapse logic also handles the interaction between column changes and index changes. If a column is modified and an index on that column is also modified, both changes are applied in the same rebuild. The planner tracks which tables have been scheduled for rebuild and skips redundant operations.

### Default Value Handling

SQLite represents DEFAULT values in `PRAGMA table_info()` using SQLite's own formatting. This means `DEFAULT 0` might come back as `DEFAULT '0'`, or `DEFAULT CURRENT_TIMESTAMP` might come back as a string. wlite normalizes these representations during introspection so they can be compared against the model's defaults. Without this normalization, every migration would detect a spurious diff on every column with a default value.

## Size and Performance Characteristics

libwlite is designed to be small and fast.

### Code Size

- Single C library, approximately 12 source files
- No dynamic memory allocation for schema operations (uses caller-provided buffers)
- Compiles in under a second on modern hardware
- The CLI binary is under 200KB when statically linked
- Total line count (including comments and whitespace) is roughly 8,000 to 10,000 lines

### Memory Model

Schema operations use arena allocation. When you load a schema, all strings and structs are allocated in a single contiguous block. Freeing the schema frees the entire arena in one operation. There are no individual `free()` calls for schema components. This makes memory management predictable and leak free.

Query operations use SQLite's own memory management. Prepared statements are created and destroyed through the standard SQLite API. wlite adds no additional allocation layers.

### Performance Profile

Migration speed is limited by SQLite I/O, not by wlite's diffing or planning. For typical schemas (under 100 tables), the planning phase takes microseconds. The actual migration time depends on the size of the tables being rebuilt.

Query performance is identical to raw SQLite. wlite adds no overhead to query execution beyond the cost of preparing statements and binding parameters.

The diff phase is O(n) where n is the total number of tables and columns in both schemas. For any realistic schema, this is effectively instantaneous.

## How wlite Compares to dbwarden Architecturally

wlite is the C rewrite of dbwarden. The architecture is the same: single source of truth, plain SQL output, small and composable. The difference is the implementation language and the scope.

### What Stayed the Same

- The `.wlite` model file format is a superset of the `.dbwarden` format
- The diff algorithm is identical
- The planner logic is identical, including table rebuilds and collapse behavior
- The migration execution model is identical (transactional, checksum recorded)
- The design principles are identical

### What Changed

| Aspect | dbwarden | wlite |
|--------|----------|-------|
| Language | Python | C |
| Runtime dependency | Python 3, sqlite3 module | None (statically linked SQLite3) |
| Binary size | N/A (interpreted) | Under 200KB |
| Startup time | Python interpreter startup | Near instant |
| Bindings | Python only | C, C++, Rust, Python, Go, C#, Zig |
| Embeddability | Requires Python runtime | Can be linked into any C program |
| Memory model | Garbage collected | Arena allocation, manual but predictable |

### Why the Rewrite

dbwarden is a good tool, but Python has limitations for this use case:

- Python startup time makes it unsuitable for being called many times in a build pipeline
- The Python runtime is a large dependency for a tool that should be a single binary
- Embedding Python in other programs is possible but awkward
- ctypes bindings from other languages to Python are fragile and slow

wlite solves all of these problems while keeping the same architecture and the same behavior.

## Build System Overview

wlite uses two build systems: Make for development and CMake for packaging and distribution.

### Makefile

The Makefile builds the wlite CLI. It requires `libwlite` to be installed or built nearby:

```
make            # build the CLI
make test       # build the CLI (prints a reminder to run it)
make clean      # remove build artifacts
make install    # install CLI to /usr/local/bin
```

The Makefile compiles `cli/main.c` and links it against libwlite and SQLite3. It uses `pkg-config` to find libwlite if installed system-wide, otherwise falls back to a local build directory.

Key Makefile targets:

| Target | Description |
|--------|-------------|
| `all` | Build the `wlite` CLI executable |
| `test` | Build the CLI and print a usage reminder |
| `clean` | Remove the built `wlite` binary |
| `install` | Install the CLI to `$(DESTDIR)/usr/local/bin` |

Key Makefile variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `LIBWLITE_DIR` | `../libwlite` | Path to the local libwlite build directory |
| `LIBWLITE_CFLAGS` | `-I$(LIBWLITE_DIR)/include` | Compiler flags for libwlite headers |
| `LIBWLITE_LDFLAGS` | `-L$(LIBWLITE_DIR) -lwlite` | Linker flags for libwlite |
| `CC` | `gcc` | C compiler |
| `CFLAGS` | `-Wall -Wextra -std=c11 -pedantic -O2` | Compiler flags |
| `LDFLAGS` | (empty) | Additional linker flags |
| `LIBS` | `-lsqlite3` | Libraries to link |

### CMake

CMake is used for packaging and for integration with projects that already use CMake. The CMakeLists.txt file defines:

- The `wlite` library target (static and shared)
- The `wlite-cli` executable target
- Header installation
- pkg-config file generation
- Integration with FetchContent for downstream projects

Downstream projects can use wlite via CMake's FetchContent:

```cmake
FetchContent_Declare(
  wlite
  GIT_REPOSITORY https://github.com/dbwarden-org/wlite.git
  GIT_TAG main
)
FetchContent_MakeAvailable(wlite)
target_link_libraries(myapp PRIVATE wlite)
```

### CMake Options

The CMake build supports the following options:

| Option | Default | Description |
|--------|---------|-------------|
| `WLITE_BUILD_SHARED` | ON | Build the shared library |
| `WLITE_BUILD_STATIC` | ON | Build the static library |
| `WLITE_BUILD_CLI` | ON | Build the CLI executable |
| `WLITE_BUILD_TESTS` | ON | Build the test executables |

### Compiler Requirements

wlite requires a C11 compiler. It uses no compiler extensions. It has been tested with:

- GCC 9 and later
- Clang 10 and later
- Apple Clang 12 and later
- MSVC 2019 and later (with CMake)

The only external dependency is SQLite3, which is found via `pkg-config` or CMake's `find_package`.

## Testing Approach

wlite has three categories of tests: unit tests, integration tests, and binding tests.

### Unit Tests

Unit tests exercise individual functions in isolation. Each module has a corresponding test file in `tests/`:

| Test File | Module Under Test |
|-----------|-------------------|
| `test_parser.c` | `parser.c` |
| `test_diff.c` | `diff.c` |
| `test_planner.c` | `planner.c` |
| `test_migrate.c` | `migrate.c` |
| `test_query.c` | `query.c` |
| `test_introspect.c` | `introspect.c` |

Unit tests use a minimal test framework that is part of wlite itself (no external test dependencies). The framework provides `ASSERT`, `ASSERT_EQUAL`, and `ASSERT_STRING` macros. Tests return 0 on success and 1 on failure.

### Integration Tests

Integration tests exercise the full pipeline from `.wlite` file to SQL output. They are written as shell scripts in `tests/integration/`. Each test:

1. Creates a temporary `.wlite` file
2. Runs `wlite plan` and captures the output
3. Runs `wlite migrate` against a temporary database
4. Runs `wlite query` to verify the result
5. Compares output against expected results

Integration tests cover the common migration scenarios:

- Creating a new table
- Adding a column
- Changing a column type (table rebuild)
- Adding an index
- Removing a table
- Complex multi-table migrations
- Idempotent migrations (running the same plan twice)

### Binding Tests

Each language binding has its own test suite that verifies the binding correctly wraps the C API. These tests are in their respective directories:

```
bindings/python/tests/
bindings/rust/tests/
bindings/go/tests/
bindings/csharp/tests/
bindings/zig/tests/
```

Binding tests focus on:

- Correct memory management (no leaks)
- Correct error propagation
- Correct type marshaling between the host language and C

### Test Runner

CMake builds run tests via `ctest` after building with `-DWLITE_BUILD_TESTS=ON`. The CMake test suite includes:

- `test_wlite` — core library unit tests
- `test_edge` — edge case tests
- `test_conformance` — conformance tests

The Makefile's `test` target builds the CLI but does not run any tests — it prints a reminder to run `./wlite version`.

CI runs the full test suite on every push, across Linux, macOS, and Windows (via MSVC and MinGW).

## Summary

wlite is a small, focused tool for SQLite schema management. Its architecture is a clean pipeline: parse, introspect, diff, plan, execute. The design principles (single source of truth, plain SQL output, small and composable) constrain every decision. The complexity lives in the planner, specifically in table rebuilds and collapse logic. The rest of the system is deliberately simple.

The C library is embeddable, the bindings are thin wrappers, and the build system is minimal. The testing approach covers unit, integration, and binding levels. The result is a tool that is fast, predictable, and safe to use in production.
