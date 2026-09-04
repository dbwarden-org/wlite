---
title: Go Binding
description: Go cgo binding for wlite.
---

# Go Binding

The Go binding uses cgo to call libwlite's C ABI. It provides Go-idiomatic types and error handling.

## Installation

```bash
go get github.com/dbwarden-org/wlite/bindings/go
```

Requires libwlite to be installed on your system.

## Basic usage

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
    defer model.Free()

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }

    rows, err := db.Query("SELECT * FROM users")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var name string
        rows.Scan(&name)
        fmt.Println(name)
    }
}
```

## Types

| Go Type | C Equivalent | Description |
|---------|--------------|-------------|
| `wlite.DB` | `wlite_db` | Open database connection |
| `wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `wlite.Stmt` | `wlite_stmt` | Prepared SQL statement |
| `wlite.Tx` | `wlite_tx` | Active transaction |
| `wlite.Rows` | result set | Query result iterator |

## Database operations

```go
db, _ := wlite.Open("app.db")
defer db.Close()

// Execute DDL/DML
db.Execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")
db.Execute("INSERT INTO test (name) VALUES (?)", "hello")

// Query
rows, _ := db.Query("SELECT * FROM test")
defer rows.Close()

for rows.Next() {
    var id int64
    var name string
    rows.Scan(&id, &name)
    fmt.Printf("%d: %s\n", id, name)
}
```

## Prepared statements

```go
stmt, _ := db.Prepare("SELECT * FROM users WHERE id = ?")
defer stmt.Finalize()

stmt.BindInt64(1, 42)
for stmt.Step() {
    name := stmt.ColumnText(0)
    fmt.Println(name)
}
```

## Transactions

```go
tx, err := db.Begin()
if err != nil {
    log.Fatal(err)
}

db.Execute("INSERT INTO users (name) VALUES ('Alice')")
db.Execute("INSERT INTO users (name) VALUES ('Bob')")

if err := tx.Commit(); err != nil {
    tx.Rollback()
    log.Fatal(err)
}
```

## Error handling

All operations return `error` as the second return value:

```go
db, err := wlite.Open("app.db")
if err != nil {
    log.Printf("Failed to open database: %v", err)
    return
}
```

## Memory management

Use `defer` to ensure resources are freed:

```go
model, _ := wlite.LoadModel("app.wlite")
defer model.Free()

db, _ := wlite.Open("app.db")
defer db.Close()

stmt, _ := db.Prepare("SELECT * FROM users")
defer stmt.Finalize()
```

## CGO configuration

When building, you may need to set CGO flags:

```bash
CGO_ENABLED=1
CGO_LDFLAGS="-L/usr/local/lib -lwlite -lsqlite3"
CGO_CFLAGS="-I/usr/local/include"
```
