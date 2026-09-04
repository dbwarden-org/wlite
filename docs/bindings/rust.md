---
title: Rust Binding
description: Rust FFI binding for wlite. Safe, ergonomic access to libwlite from Rust.
---

# Rust Binding

The `wlite` crate provides safe, ergonomic access to libwlite from Rust. It wraps the C ABI with Rust-native types and error handling.

## Installation

```toml
[dependencies]
wlite = "0.1"
```

## Usage

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;
    db.migrate(&model)?;

    let rows = db.query("SELECT * FROM users")?;
    for row in rows {
        println!("{}", row.get::<String>(0)?);
    }

    Ok(())
}
```

## Types

| Rust Type | C Equivalent | Description |
|-----------|--------------|-------------|
| `Database` | `wlite_db` | Open database connection |
| `Model` | `wlite_model` | Loaded .wlite schema |
| `Statement` | `wlite_stmt` | Prepared SQL statement |
| `Transaction` | `wlite_tx` | Active transaction |
| `Error` | `wlite_result` | Error result |

## Error Handling

All fallible operations return `wlite::Result<T>`, which is `Result<T, wlite::Error>`.

```rust
use wlite::{Database, Error};

match Database::open("app.db") {
    Ok(db) => { /* use db */ }
    Err(Error::NotFound) => eprintln!("Database not found"),
    Err(e) => eprintln!("Error: {}", e),
}
```

## Memory Safety

The Rust binding uses `Drop` implementations to ensure proper cleanup. `Database`, `Model`, `Statement`, and `Transaction` are automatically freed when they go out of scope.

Models are immutable after loading and can be shared across threads via `Arc<Model>`. Database connections are not thread-safe; use one per thread.
