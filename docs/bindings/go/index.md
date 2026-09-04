---
title: Go Binding Reference
description: Complete reference for the wlite Go cgo binding, including installation, types, and configuration.
---

# Go Binding Reference

The Go binding provides a cgo-based interface to libwlite. It wraps the C ABI with Go-idiomatic types, error handling, and resource management patterns. The binding is designed for use in services, command-line tools, and embedded applications that need schema-driven SQLite management from Go.

## Prerequisites

Before using the Go binding you need:

- Go 1.21 or later
- A C compiler (gcc or clang)
- libwlite installed on your system
- SQLite 3 development libraries
- CGO enabled (it is enabled by default on most platforms)

Install libwlite from source if it is not already available on your system:

```bash
git clone https://github.com/dbwarden-org/libwlite.git
cd libwlite
cmake -B build
cmake --build build
sudo cmake --install build
```

Verify the installation:

```bash
wlite version
```

## Installation

Add the Go binding to your module:

```bash
go get github.com/dbwarden-org/wlite/bindings/go
```

This pulls the binding package into your module and records it in `go.mod` and `go.sum`.

### CGO configuration

CGO must be enabled and the linker and compiler flags must point to libwlite headers and shared library. Set these environment variables before building:

```bash
export CGO_ENABLED=1
export CGO_LDFLAGS="-L/usr/local/lib -lwlite -lsqlite3"
export CGO_CFLAGS="-I/usr/local/include"
```

If libwlite ships a pkg-config file, use it instead:

```bash
export CGO_LDFLAGS="$(pkg-config --libs wlite)"
export CGO_CFLAGS="$(pkg-config --cflags wlite)"
```

The binding's `wlite.go` file includes cgo directives that reference the correct include path and linker flags relative to the repository layout. When you install the package with `go get`, Go resolves these paths automatically as long as libwlite is available to the linker.

### Platform notes

| Platform | Notes |
|----------|-------|
| Linux | Use gcc. Ensure `libsqlite3-dev` is installed. |
| macOS | Use clang (Xcode Command Line Tools). |
| Windows | Use mingw-w64 or MSYS2. Cross-compilation from Linux requires the appropriate cross-compiler toolchain. |

### Building your application

After setting the CGO flags:

```bash
CGO_ENABLED=1 go build -o myapp .
```

Or with pkg-config:

```bash
CGO_LDFLAGS="$(pkg-config --libs wlite)" \
CGO_CFLAGS="$(pkg-config --cflags wlite)" \
go build -o myapp .
```

### Cross-compilation

Cross-compilation requires a C cross-compiler for the target platform:

```bash
CC=x86_64-linux-gnu-gcc \
CGO_ENABLED=1 \
GOOS=linux \
GOARCH=amd64 \
go build -o myapp .
```

## Quick start

The following program loads a schema model, opens a database, runs a migration, and queries data:

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    model, err := wlite.LoadModel("app.wlite")
    if err != nil {
        log.Fatal(err)
    }
    defer model.Close()

    if err := model.Validate(); err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Model loaded: %d table(s)\n", model.TableCount())

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }
    fmt.Println("Migration complete.")

    _, err = db.Execute(
        "INSERT INTO todos (title, completed, created_at) VALUES (?, 0, datetime('now'))",
        "Buy groceries",
    )
    if err != nil {
        log.Fatal(err)
    }

    rows, err := db.Query("SELECT id, title, completed FROM todos")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var id int64
        var title string
        var completed int64
        if err := rows.Scan(&id, &title, &completed); err != nil {
            log.Fatal(err)
        }
        status := " "
        if completed != 0 {
            status = "x"
        }
        fmt.Printf("  [%s] %s (id=%d)\n", status, title, id)
    }

    if err := rows.Err(); err != nil {
        log.Fatal(err)
    }
}
```

Run it with:

```bash
CGO_ENABLED=1 go run main.go
```

## Types

The Go binding exposes five primary types. Each wraps a C-level opaque type and manages its lifecycle.

| Go Type | C Equivalent | Description |
|---------|--------------|-------------|
| `wlite.DB` | `wlite_db` | An open database connection. Provides methods for executing SQL, preparing statements, running migrations, and starting transactions. |
| `wlite.Model` | `wlite_model` | A parsed `.wlite` schema file. Immutable after loading. Can be used to migrate multiple databases. Must be freed with `Close()`. |
| `wlite.Stmt` | `wlite_stmt` | A prepared SQL statement. Supports parameter binding, stepping through results, and reading column values. Must be finalized with `Close()`. |
| `wlite.Tx` | `wlite_tx` | An active database transaction. Supports commit, rollback, and savepoints. Must be closed or rolled back. |
| `wlite.Rows` | result set | An iterator over query results returned by `db.Query()`. Supports `Next()`, `Scan()`, and `Close()`. |

### wlite.DB

The `DB` type manages a single database connection. Create one with `wlite.Open` or `wlite.OpenMemory`. Every `DB` must be closed when no longer needed:

```go
db, err := wlite.Open("app.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

`DB` is not safe for concurrent use. Each goroutine that needs database access should open its own connection.

### wlite.Model

The `Model` type holds a parsed schema definition loaded from a `.wlite` file. Models are immutable and can be shared across goroutines. Load a model with `wlite.LoadModel`:

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()
```

You can also load a model from raw bytes with `wlite.ModelFromBytes`, which is useful when the schema is embedded in the binary or fetched from a remote source.

### wlite.Stmt

The `Stmt` type wraps a prepared SQL statement. Prepare one from a `DB`:

```go
stmt, err := db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()
```

Bind parameters with `BindInt64`, `BindDouble`, `BindText`, or `BindNull`, then call `Step` to execute. After stepping, read results with the column access methods.

### wlite.Tx

The `Tx` type represents an active transaction. Start one with `db.Transaction()`:

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}
```

Call `tx.Commit()` to persist changes or `tx.Rollback()` to discard them. You can also create savepoints within a transaction.

### wlite.Rows

The `Rows` type is an iterator over query results. It is returned by `db.Query()`:

```go
rows, err := db.Query("SELECT id, name FROM users")
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

for rows.Next() {
    var id int64
    var name string
    if err := rows.Scan(&id, &name); err != nil {
        log.Fatal(err)
    }
    fmt.Printf("%d: %s\n", id, name)
}
```

Always check `rows.Err()` after the loop and close the rows when done.

## Database creation and opening

### Open a file database

```go
db, err := wlite.Open("mydata.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

### Open an in-memory database

```go
db, err := wlite.OpenMemory()
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

In-memory databases are useful for testing and temporary data processing. They exist only for the lifetime of the process.

### Open with options

The underlying C API supports additional open options (read-only mode, foreign key enforcement, busy timeout). Use `wlite.Open` for the common case and configure the database after opening if you need advanced options.

## Executing SQL

The `Execute` method runs SQL that does not return rows. Use it for DDL and DML statements:

```go
err := db.Execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
if err != nil {
    log.Fatal(err)
}

err = db.Execute(
    "INSERT INTO users (name) VALUES (?)",
    "Alice",
)
if err != nil {
    log.Fatal(err)
}
```

`Execute` accepts variadic arguments that are bound as parameters. The argument types supported are `int`, `int64`, `float64`, `string`, and `nil`.

## Prepared statements

Preparing a statement once and executing it multiple times is more efficient than executing raw SQL repeatedly:

```go
stmt, err := db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

users := []struct{ Name, Email string }{
    {"Alice", "alice@example.com"},
    {"Bob", "bob@example.com"},
    {"Charlie", "charlie@example.com"},
}

for _, u := range users {
    stmt.BindText(1, u.Name)
    stmt.BindText(2, u.Email)
    stmt.Step()
    stmt.Reset()
}
```

Always finalize statements with `defer stmt.Close()` to release the underlying C resources.

## Querying data

Use `db.Query` for queries that return multiple rows and `db.QueryRow` for single-value results:

```go
// Multiple rows
rows, err := db.Query("SELECT id, name, email FROM users ORDER BY name")
if err != nil {
    log.Fatal(err)
}
defer rows.Close()

for rows.Next() {
    var id int64
    var name, email string
    rows.Scan(&id, &name, &email)
    fmt.Printf("%d: %s <%s>\n", id, name, email)
}

// Single value
var count int64
err = db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Total users: %d\n", count)
```

## Transactions

Wrap multiple related operations in a transaction for atomicity:

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}

_, err = db.Execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", 100, 1)
if err != nil {
    tx.Rollback()
    log.Fatal(err)
}

_, err = db.Execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", 100, 2)
if err != nil {
    tx.Rollback()
    log.Fatal(err)
}

if err := tx.Commit(); err != nil {
    log.Fatal(err)
}
```

## CGO configuration reference

The binding's cgo directives in `wlite.go` are:

```
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../. -lwlite -lsqlite3
```

These relative paths work when building from within the repository. When the package is installed via `go get`, Go uses the cached module source and the system-installed libwlite. Ensure the library and headers are in the standard system paths (`/usr/local/lib` and `/usr/local/include`) or set the CGO flags explicitly.

### Environment variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `CGO_ENABLED` | Enable CGO compilation | `1` |
| `CGO_CFLAGS` | C compiler flags | `-I/usr/local/include` |
| `CGO_LDFLAGS` | Linker flags | `-L/usr/local/lib -lwlite -lsqlite3` |
| `CC` | C compiler for cross-compilation | `x86_64-linux-gnu-gcc` |
| `GOOS` | Target operating system | `linux` |
| `GOARCH` | Target architecture | `amd64` |

### Troubleshooting

If you see linker errors like `cannot find -lwlite`, verify that `libwlite.so` (or `libwlite.a`) is in a path that the linker can find. You can check with:

```bash
ldconfig -p | grep wlite
```

If the library is installed in a non-standard location, add it to `LD_LIBRARY_PATH` or set `CGO_LDFLAGS` with the full path:

```bash
export CGO_LDFLAGS="-L/path/to/lib -lwlite -lsqlite3"
```

If you see compiler errors about missing headers, verify the include path:

```bash
ls /usr/local/include/wlite/wlite.h
```

### Build tags

The Go binding does not use build tags. The same source works on all platforms that have a C compiler and libwlite available.

## Complete example

The following program demonstrates model loading, validation, migration, inserts, queries, and transactions in a single file:

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

type User struct {
    ID     int64
    Name   string
    Email  string
    Active bool
}

func main() {
    model, err := wlite.LoadModel("app.wlite")
    if err != nil {
        log.Fatal(err)
    }
    defer model.Close()

    if err := model.Validate(); err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Model: %d table(s)\n", model.TableCount())

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }
    fmt.Println("Migration complete.")

    _, err = db.Execute(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        "Alice", "alice@example.com",
    )
    if err != nil {
        log.Fatal(err)
    }

    _, err = db.Execute(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        "Bob", "bob@example.com",
    )
    if err != nil {
        log.Fatal(err)
    }

    var count int64
    err = db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Users: %d\n", count)

    rows, err := db.Query("SELECT id, name, email FROM users ORDER BY name")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var u User
        if err := rows.Scan(&u.ID, &u.Name, &u.Email); err != nil {
            log.Fatal(err)
        }
        fmt.Printf("  %d: %s <%s>\n", u.ID, u.Name, u.Email)
    }
    if err := rows.Err(); err != nil {
        log.Fatal(err)
    }

    fmt.Println("Done.")
}
```

## Version information

Query the library version at runtime:

```go
fmt.Println("wlite version:", wlite.Version())
fmt.Println("ABI version:", wlite.ABIVersion())
```

This is useful for diagnostics and for ensuring that the installed library matches the version your application was built against.

## Further reading

- [Migration guide](migration.md) for schema management with `LoadModel`, `Migrate`, `Diff`, `Plan`, and `Snapshot`.
- [Query guide](queries.md) for prepared statements, binding, column access, and transactions.
- [Error handling guide](errors.md) for the `Error` type, error constants, and patterns for safe resource management.
