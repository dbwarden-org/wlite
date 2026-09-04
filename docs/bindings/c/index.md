---
title: "C Binding for wlite"
description: "Getting started with the wlite C binding. Installation, build integration, types reference, and a complete quick-start example."
---

# C Binding for wlite

The C binding for wlite is the core library that every other language binding wraps. It lives in [libwlite](https://github.com/dbwarden-org/libwlite) and provides a single, stable ABI for schema management, migration, and querying against SQLite databases.

If you are building an embedded application, a CLI tool, a TUI, or a lightweight service that uses SQLite, the C binding is the most direct way to integrate wlite into your project.

## What libwlite provides

libwlite is a static and shared library written in C. It has zero runtime dependencies beyond SQLite3. The library handles:

- Parsing `.wlite` model files into in-memory schema structures
- Introspecting live SQLite databases into the same schema structures
- Computing diffs between current and desired schemas
- Generating migration plans with forward and rollback SQL
- Applying migrations to a database
- Preparing, binding, and stepping through SQL queries
- Transaction and savepoint management
- Schema hashing for integrity checks
- Serialization to JSON and DSL formats

Every language binding (Rust, Python, C#, Go, Zig, C++) calls into this same C library. Behavior is identical regardless of which language you use.

## Requirements

You need the following to build and link against libwlite:

- A C99-compatible compiler (GCC, Clang, MSVC)
- SQLite3 development headers and library (3.35.0 or later recommended)
- GNU Make or CMake 3.14+
- pkg-config (optional, for Makefile-based projects)

## Installation

### From source with Make

```bash
git clone https://github.com/dbwarden-org/libwlite.git
cd libwlite
make
make install
```

By default this installs to `/usr/local`. Override `PREFIX` to change the location:

```bash
make install PREFIX=/opt/wlite
```

This places the header at `$(PREFIX)/include/wlite/wlite.h` and the library at `$(PREFIX)/lib/libwlite.a` (and `.so` on Linux).

### From source with CMake

```bash
git clone https://github.com/dbwarden-org/libwlite.git
cd libwlite
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

CMake exports a `wlite` package target, so downstream projects can use `find_package(wlite)` and link against `wlite::wlite`.

### Verifying the installation

After installing, confirm the header and library are accessible:

```bash
pkg-config --cflags --libs wlite
```

If you installed to a non-standard prefix, set `PKG_CONFIG_PATH`:

```bash
export PKG_CONFIG_PATH=/opt/wlite/lib/pkgconfig:$PKG_CONFIG_PATH
pkg-config --cflags --libs wlite
```

## Quick start

This section walks through a complete program that opens a database, migrates it from a model file, runs a query, and prints the results.

### The model file

Create `app.wlite`:

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

### The C program

Create `main.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_result r;

    /* Load the model from disk */
    r = wlite_model_load_file("app.wlite", &model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Failed to load model: %s\n", wlite_strerror(r));
        return 1;
    }

    /* Validate the model */
    r = wlite_model_validate(model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Invalid model: %s\n", wlite_strerror(r));
        wlite_model_free(model);
        return 1;
    }

    /* Open the database (creates it if it does not exist) */
    r = wlite_open("app.db", &db);
    if (r != WLITE_OK) {
        fprintf(stderr, "Failed to open database: %s\n", wlite_strerror(r));
        wlite_model_free(model);
        return 1;
    }

    /* Apply the migration */
    r = wlite_migrate(db, model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Migration failed: %s\n", wlite_strerror(r));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    /* Insert a row */
    r = wlite_execute(db,
        "INSERT INTO users (username, email) VALUES ('alice', 'alice@example.com')",
        NULL);
    if (r != WLITE_OK) {
        fprintf(stderr, "Insert failed: %s\n", wlite_strerror(r));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    /* Query the data */
    wlite_stmt *stmt = NULL;
    r = wlite_prepare(db, "SELECT id, username, email FROM users", &stmt);
    if (r != WLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", wlite_strerror(r));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    printf("id\tusername\temail\n");
    printf("--\t--------\t-----\n");
    while (wlite_step(stmt) == WLITE_OK) {
        int64_t id = wlite_column_int64(stmt, 0);
        const char *username = wlite_column_text(stmt, 1);
        const char *email = wlite_column_text(stmt, 2);
        printf("%lld\t%s\t%s\n", (long long)id, username, email);
    }

    wlite_stmt_finalize(stmt);
    wlite_close(db);
    wlite_model_free(model);

    printf("Done.\n");
    return 0;
}
```

### Building

#### With pkg-config and GCC

```bash
gcc main.c -o app $(pkg-config --cflags --libs wlite) -lsqlite3
./app
```

#### With a Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags wlite)
LDFLAGS = $(shell pkg-config --libs wlite) -lsqlite3

app: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f app app.db

.PHONY: clean
```

#### With CMake

```cmake
cmake_minimum_required(VERSION 3.14)
project(myapp C)

find_package(wlite REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE wlite::wlite SQLite::SQLite3)
```

### Expected output

```
id	username	email
--	--------	-----
1	alice	alice@example.com
Done.
```

## Types reference

The following table lists all public types defined by `wlite/wlite.h`. Opaque types are created and freed through the API. Concrete types are defined in the header and may be used on the stack or in your own structures.

| Type | Kind | Description |
|------|------|-------------|
| `wlite_result` | enum | Return code from all fallible functions. `WLITE_OK` (0) means success. |
| `wlite_error` | struct | Detailed error information including message, subsystem, object name, and SQLite error code. |
| `wlite_db` | opaque | Database handle. Created by `wlite_open`, freed by `wlite_close`. |
| `wlite_model` | opaque | Loaded model handle. Created by `wlite_model_load_*`, freed by `wlite_model_free`. |
| `wlite_stmt` | opaque | Prepared statement handle. Created by `wlite_prepare`, freed by `wlite_stmt_finalize`. |
| `wlite_record` | opaque | Snapshot of a result row. Created by `wlite_record_from_stmt`, freed by `wlite_record_free`. |
| `wlite_tx` | opaque | Transaction handle. Created by `wlite_begin`, freed by `wlite_tx_free`. |
| `wlite_table` | concrete | Table definition within a schema. Part of `WlSchema`. |
| `wlite_field` | concrete | Column definition within a table. Alias for `WlColumn`. |
| `WlSchema` | concrete | Complete schema structure with tables, indexes, views, and triggers. |
| `WlDiff` | concrete | List of differences between two schemas. |
| `WlDiffEntry` | concrete | A single difference (add, drop, alter, rebuild). |
| `WlDiffOp` | enum | Type of diff operation (add table, drop column, etc.). |
| `WlSafety` | enum | Safety classification of a diff operation (safe, destructive, irreversible, etc.). |
| `WlPlan` | concrete | Migration plan with ordered steps. |
| `WlPlanStep` | concrete | Single step in a migration plan with SQL and rollback SQL. |
| `WlPlanOp` | enum | Type of plan operation (create table, alter column, rebuild table, etc.). |
| `wlite_open_options` | struct | Options for `wlite_open_ex` (readonly, create, foreign_keys, busy_timeout_ms). |
| `wlite_value_type` | enum | SQLite value type: null, integer, real, text, blob. |
| `wlite_col_type` | enum | Column affinity: none, integer, real, text, blob, any. |
| `wlite_fk_action` | enum | Foreign key action: no action, restrict, set null, set default, cascade. |
| `wlite_sqlite_caps` | struct | Runtime detection of SQLite feature support. |
| `wlite_writer` | struct | Writer interface for schema serialization. Has a context pointer and a write callback. |

## Error codes

All fallible functions return `wlite_result`. The full enumeration:

| Code | Value | Meaning |
|------|-------|---------|
| `WLITE_OK` | 0 | Success |
| `WLITE_ERROR` | 1 | Generic error |
| `WLITE_INVALID_ARGUMENT` | 2 | Null pointer or out-of-range argument |
| `WLITE_OUT_OF_MEMORY` | 3 | Memory allocation failed |
| `WLITE_IO_ERROR` | 4 | File system read/write failure |
| `WLITE_PARSE_ERROR` | 5 | Model file syntax error |
| `WLITE_MODEL_ERROR` | 6 | Semantic error in the model |
| `WLITE_SQLITE_ERROR` | 7 | Underlying SQLite error |
| `WLITE_CONSTRAINT_ERROR` | 8 | UNIQUE, CHECK, or FOREIGN KEY violation |
| `WLITE_NOT_FOUND` | 9 | File, table, or column not found |
| `WLITE_BUSY` | 10 | Database is locked by another connection |
| `WLITE_TRANSACTION_ERROR` | 11 | Transaction state error |

Legacy aliases: `WLITE_ERR_SYNTAX` = `WLITE_PARSE_ERROR`, `WLITE_ERR_NULL_PTR` = `WLITE_INVALID_ARGUMENT`, `WLITE_ERR_IO` = `WLITE_IO_ERROR`.

Use `wlite_strerror(code)` to get a human-readable string for any code.

## Build integration

### pkg-config

After running `make install`, libwlite installs a `.pc` file. Use it in Makefiles:

```makefile
CFLAGS += $(shell pkg-config --cflags wlite)
LDLIBS += $(shell pkg-config --libs wlite) -lsqlite3
```

Or in GCC invocations directly:

```bash
gcc main.c -o app $(pkg-config --cflags --libs wlite) -lsqlite3
```

### CMake

libwlite exports a CMake package. In your `CMakeLists.txt`:

```cmake
find_package(wlite REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE wlite::wlite SQLite::SQLite3)
```

If libwlite is not installed system-wide, point CMake to it:

```bash
cmake -B build -Dwlite_DIR=/path/to/libwlite/build
```

### Static linking

If you prefer static linking (common for embedded targets), link against the archive:

```bash
gcc main.c -o app /usr/local/lib/libwlite.a -lsqlite3 -lm
```

Or with CMake, set `wlite_SHARED` to `OFF` when building libwlite:

```bash
cmake -B build -Dwlite_SHARED=OFF
```

### Embedding libwlite

For projects that want to vendor libwlite directly, copy the `include/wlite/` directory and the source files from the libwlite repository into your own tree. Then compile them alongside your application sources. This avoids any external dependency on a system-installed library.

## Header structure

The single public header `wlite/wlite.h` is organized into these sections:

1. ABI and version macros
2. Error codes and error struct
3. Opaque type forward declarations
4. Schema parsing and introspection
5. Diff and migration plan types
6. Database API
7. Model API (load, validate, introspect)
8. Query API (prepare, bind, step, column access)
9. Record API
10. Transaction and savepoint API
11. Schema hashing and serialization
12. SQLite capability detection
13. Memory utilities

Include it once with `#include <wlite/wlite.h>`. The header is C99-compatible and uses `extern "C"` guards for C++ projects.

## Version checking

You can check the library and ABI version at runtime:

```c
#include <stdio.h>
#include <wlite/wlite.h>

int main(void) {
    printf("wlite %s (ABI v%d)\n", wlite_version(), wlite_abi_version());
    return 0;
}
```

Compile-time version macros are also available:

```c
#if WLITE_VERSION_MAJOR == 0 && WLITE_VERSION_MINOR >= 2
    /* Use features added in 0.2.0 */
#endif
```

## Thread safety

libwlite handles are not thread-safe by default. Each thread should use its own `wlite_db` handle. If multiple threads need to access the same database, open the database once and use `wlite_open_ex` with a busy timeout, or serialize access through a mutex.

SQLite itself supports multiple readers with a single writer when WAL mode is enabled. libwlite passes through SQLite threading modes, so you can configure this at the SQLite level if needed.

## Next steps

- [Migration](migration.md): Load models, diff schemas, run migrations, and work with compiled models
- [Queries](queries.md): Prepare statements, bind parameters, step through results, manage transactions, and handle blobs
- [Errors](errors.md): Error codes, structured errors, cleanup patterns, and thread safety
- [C API Reference](../../c-api.md): Complete function-by-function reference
