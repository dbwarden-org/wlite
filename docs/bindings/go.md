---
title: Go Binding
description: Go cgo binding for wlite. Use .wlite schemas from Go.
---

# Go Binding

The Go binding uses cgo to call libwlite's C ABI. It provides Go-idiomatic types and error handling.

## Usage

```go
package main

import (
    "fmt"
    "log"
    "github.com/dbwarden-org/wlite/bindings/go"
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

## Error Handling

All operations return `error` as the second return value:

```go
db, err := wlite.Open("app.db")
if err != nil {
    // handle error
}
```

## Memory Management

Use `defer` to ensure resources are freed:

```go
model, _ := wlite.LoadModel("app.wlite")
defer model.Free()

db, _ := wlite.Open("app.db")
defer db.Close()
```
