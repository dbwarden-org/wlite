---
title: Python Binding Overview
description: Getting started with the wlite Python binding for libwlite.
---

# Python Binding Overview

The `wlite` Python package provides a ctypes-based binding to the libwlite C library. It lets you define your schema in `.wlite` files and use the compiled model from Python to manage databases, run queries, and work with transactions.

This document covers installation, prerequisites, a quick start guide, and the core types exposed by the binding.

## Prerequisites

Before installing the Python binding you need libwlite available on your system. The binding communicates with the shared library at runtime through ctypes, so it must be discoverable by the dynamic linker.

### Build and install libwlite

Clone the repository and build from source:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

This installs the shared library and headers to the default system paths. On most Linux distributions this places `libwlite.so` under `/usr/local/lib` and headers under `/usr/local/include`.

If the library is installed to a non-standard location you can point the linker to it with an environment variable:

```bash
export LD_LIBRARY_PATH=/opt/wlite/lib:$LD_LIBRARY_PATH
```

### Python version

The binding requires Python 3.8 or later. It uses only the standard library (ctypes, os, pathlib) so no additional Python packages are needed at runtime.

You can check your Python version with:

```bash
python3 --version
```

## Installation

### Install from PyPI

The recommended way to install the binding is through pip:

```bash
pip install wlite
```

This pulls the published package from PyPI and installs it into your current environment. The package includes the Python source and a thin loader that finds libwlite at import time.

### Development installation

If you are contributing to the binding or want to work against the latest source, clone the repository and install in editable mode:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite/bindings/python
pip install -e .
```

The editable install creates a link to the source tree so changes you make to the Python files take effect immediately without reinstalling.

### System package

Some distributions may package libwlite separately. Check your package manager:

```bash
# Debian / Ubuntu
apt list --installed 2>/dev/null | grep wlite

# Fedora / RHEL
rpm -qa | grep wlite
```

If the library is available through your distribution you can skip building from source and go straight to `pip install wlite`.

### Verifying the installation

After installing, confirm the package loads and can find libwlite:

```python
import wlite
print(wlite.__version__)
```

If the import fails with an `ImportError` the most common cause is that libwlite is not installed or not on the library search path. Revisit the prerequisites section.

## Quick start

The following example creates a database, defines a schema, inserts some data, and queries it back. It demonstrates the core workflow that most applications will follow.

```python
import wlite

# Load the schema model from a .wlite file
model = wlite.Model.load("app.wlite")

# Open (or create) the database file
db = wlite.Database.open("app.db")

# Apply the schema to the database
db.migrate(model)

# Insert rows with a prepared statement
stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
try:
    stmt.bind(1, "Alice")
    stmt.bind(2, "alice@example.com")
    stmt.step()
    stmt.reset()

    stmt.bind(1, "Bob")
    stmt.bind(2, "bob@example.com")
    stmt.step()
finally:
    stmt.finalize()

# Query the data back
rows = db.query("SELECT id, name, email FROM users ORDER BY name")
for row in rows:
    print(f"{row['id']}: {row['name']} <{row['email']}>")

# Close the database
db.close()
```

Running this program prints:

```
1: Alice <alice@example.com>
2: Bob <bob@example.com>
```

### Using the context manager

The `Database` type supports the context manager protocol so you do not have to call `close` manually:

```python
import wlite

with wlite.Database.open("app.db") as db:
    model = wlite.Model.load("app.wlite")
    db.migrate(model)

    rows = db.query("SELECT * FROM users")
    for row in rows:
        print(row["name"])
```

When the `with` block exits the database is closed automatically, even if an exception is raised inside the block.

## Core types

The binding exposes five primary types. Each corresponds to a concept in the underlying C library.

| Python Type | C Equivalent | Description |
|---|---|---|
| `wlite.Database` | `wlite_db` | An open connection to a database file. Provides methods for executing SQL, preparing statements, running queries, and managing transactions. |
| `wlite.Model` | `wlite_model` | A parsed `.wlite` schema. Loaded once and used to migrate one or more databases. Immutable after loading. |
| `wlite.Statement` | `wlite_stmt` | A prepared SQL statement. Supports parameter binding, stepping through results, and column access. |
| `wlite.Transaction` | `wlite_tx` | An active database transaction. Supports commit, rollback, savepoints, and rollback-to-savepoint. |
| `wlite.Error` | error code | Exception raised when any wlite operation fails. Carries a numeric code and a human-readable message. |

### Database

`Database` is the central object. You obtain one by calling `Database.open(path)`. It manages the connection lifecycle and provides the main API surface:

- `execute(sql, params)` runs a statement without returning rows.
- `query(sql, params)` returns a list of dictionaries, one per row.
- `query_scalar(sql, params)` returns a single scalar value.
- `prepare(sql)` returns a `Statement` for manual parameter binding.
- `begin()` starts a transaction and returns a `Transaction`.
- `migrate(model)` applies a schema model to the database.
- `close()` releases the connection.

### Model

`Model` represents a parsed `.wlite` schema file. Load it once with `Model.load(path)` and reuse it for multiple databases:

```python
model = wlite.Model.load("app.wlite")
db1 = wlite.Database.open("db1.db")
db1.migrate(model)
db1.close()

db2 = wlite.Database.open("db2.db")
db2.migrate(model)
db2.close()
```

The model provides introspection methods such as `table_count()`, `table_at(i)`, and field accessors on each table.

### Statement

`Statement` wraps a prepared SQL statement. You create one with `db.prepare(sql)` and then:

1. Bind parameters with `stmt.bind(index, value)`.
2. Call `stmt.step()` to execute. For queries this returns `True` if a row is available.
3. Read columns with `stmt.column_int64(i)`, `stmt.column_text(i)`, etc.
4. Call `stmt.reset()` to re-execute with new parameters.
5. Call `stmt.finalize()` when the statement is no longer needed.

### Transaction

`Transaction` is obtained from `db.begin()`. It provides:

- `commit()` to persist all changes.
- `rollback()` to undo all changes.
- `savepoint(name)` to create a nested savepoint.
- `rollback_to(name)` to roll back to a savepoint.
- `free()` to release transaction resources.

Always call `free()` in a `finally` block to avoid leaking resources.

### Error

`Error` is the single exception type raised by all wlite operations. It carries:

- `code`: an integer error code (see the error codes reference).
- `message`: a human-readable description.

Catch it with a standard `try`/`except`:

```python
try:
    db = wlite.Database.open("missing.db")
except wlite.Error as e:
    print(f"Failed: code={e.code} message={e.message}")
```

## Thread safety

The Python binding does not add its own locking. Thread safety depends on the underlying SQLite configuration. By default SQLite allows multiple threads to read but serializes writes. If you access a single `Database` from multiple threads you should use a lock or assign one thread as the owner of the connection.

For most applications the simplest approach is to open a separate `Database` per thread or use a threading lock around database access.

## Memory management

The binding uses reference counting and finalizers to release C resources. When a Python object wrapping a C handle goes out of scope the finalizer calls the corresponding C free function.

To be explicit about cleanup, especially for transactions and statements, use the `close()` or `finalize()` methods and the `with` statement where supported. This avoids waiting for the garbage collector to run.

## Supported platforms

The binding works on any platform where libwlite can be built and Python is available. The primary supported platforms are:

- Linux (x86_64, aarch64)
- macOS (x86_64, arm64)
- Windows (x86_64)

On Linux the binding requires glibc 2.17 or later. On macOS it requires macOS 10.15 or later. On Windows it requires Visual C++ Redistributable 2019 or later.

## Environment variables

The binding respects several environment variables:

### LD_LIBRARY_PATH (Linux)

If libwlite is installed to a non-standard location, add it to the library search path:

```bash
export LD_LIBRARY_PATH=/opt/wlite/lib:$LD_LIBRARY_PATH
```

### DYLD_LIBRARY_PATH (macOS)

On macOS the equivalent variable is:

```bash
export DYLD_LIBRARY_PATH=/opt/wlite/lib:$DYLD_LIBRARY_PATH
```

### PATH (Windows)

On Windows, place `wlite.dll` in a directory that is on the system PATH, or add the directory containing the DLL:

```cmd
set PATH=C:\wlite\bin;%PATH%
```

### WLITE_LIBRARY_PATH

The binding also checks for a `WLITE_LIBRARY_PATH` environment variable. If set, it uses this path to locate the shared library before falling back to system defaults:

```bash
export WLITE_LIBRARY_PATH=/opt/wlite/lib
```

## Module-level constants

The binding exposes several constants at the module level for use in your code:

```python
import wlite

# Column type constants
print(wlite.COLUMN_INTEGER)  # INTEGER column type
print(wlite.COLUMN_FLOAT)    # FLOAT column type
print(wlite.COLUMN_TEXT)     # TEXT column type
print(wlite.COLUMN_BLOB)     # BLOB column type

# Error code constants
print(wlite.WLITE_OK)                # 0
print(wlite.WLITE_ERROR)             # 1
print(wlite.WLITE_INVALID_ARGUMENT)  # 2
print(wlite.WLITE_OUT_OF_MEMORY)     # 3
print(wlite.WLITE_IO_ERROR)          # 4
print(wlite.WLITE_PARSE_ERROR)       # 5
print(wlite.WLITE_MODEL_ERROR)       # 6
print(wlite.WLITE_SQLITE_ERROR)      # 7
print(wlite.WLITE_CONSTRAINT_ERROR)  # 8
print(wlite.WLITE_NOT_FOUND)         # 9
print(wlite.WLITE_BUSY)              # 10
print(wlite.WLITE_TRANSACTION_ERROR) # 11
```

## Complete quick start example

Here is a more complete example that demonstrates creating a schema, migrating a database, inserting data, querying it back, and handling errors:

```python
import wlite
import os


def main():
    # Remove any existing database for a clean start
    if os.path.exists("example.db"):
        os.remove("example.db")

    # Load the schema
    model = wlite.Model.load("app.wlite")

    # Open the database and apply the schema
    db = wlite.Database.open("example.db")
    db.migrate(model)

    # Insert some users
    stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    try:
        users = [
            ("Alice", "alice@example.com"),
            ("Bob", "bob@example.com"),
            ("Charlie", "charlie@example.com"),
        ]
        for name, email in users:
            stmt.bind(1, name)
            stmt.bind(2, email)
            stmt.step()
            stmt.reset()
    finally:
        stmt.finalize()

    # Query all users
    print("All users:")
    rows = db.query("SELECT id, name, email FROM users ORDER BY name")
    for row in rows:
        print(f"  {row['id']}: {row['name']} <{row['email']}>")

    # Query a single user
    user = db.query("SELECT id, name, email FROM users WHERE name = ?", ("Alice",))
    if user:
        print(f"\nFound: {user[0]['name']} <{user[0]['email']}>")

    # Count users
    count = db.query_scalar("SELECT COUNT(*) FROM users")
    print(f"\nTotal users: {count}")

    # Close the database
    db.close()
    print("\nDone!")


if __name__ == "__main__":
    main()
```

## Troubleshooting

### ImportError on import

If you see `ImportError: libwlite.so not found` or similar, the shared library is not on the library search path. Verify that libwlite is installed and that the directory containing `libwlite.so` (or `wlite.dll` on Windows) is in your library path.

### Database is locked

If you get `WLITE_BUSY`, another process or thread is writing to the database. Either wait for the other writer to finish, use WAL journal mode, or implement retry logic with backoff.

### Schema parse error

If `Model.load` raises `WLITE_PARSE_ERROR`, check your `.wlite` file for syntax errors. Common issues include missing closing braces, duplicate field names, or invalid type names.

### Permission denied

If `Database.open` raises an I/O error with a permission denied message, check that your process has read and write access to the database file and its directory.

## Next steps

- Read the migration guide for schema management and model inspection.
- Read the queries guide for prepared statements, column access, and transaction patterns.
- Read the error handling guide for error codes, cleanup patterns, and thread safety details.
