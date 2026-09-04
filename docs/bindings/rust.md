---
title: Rust Binding
description: Safe, ergonomic Rust bindings for libwlite.
---

# Rust Binding

The `wlite` crate provides safe, ergonomic access to libwlite from Rust. It wraps the C ABI with Rust-native types, error handling, and RAII.

## Installation

```toml
[dependencies]
wlite = "0.1"
```

The crate compiles libwlite from source using the `cc` crate. You need a C11 compiler and SQLite3 development library installed.

## Basic usage

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

## Database operations

```rust
use wlite::Database;

// Open a database
let db = Database::open("app.db")?;

// Execute DDL/DML
db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")?;
db.execute("INSERT INTO test (name) VALUES ('hello')")?;

// Prepare and query
let mut stmt = db.prepare("SELECT * FROM test WHERE id = ?")?;
stmt.bind(1, 1_i64)?;
while stmt.step()? {
    let name: String = stmt.column_text(0)?;
    let id: i64 = stmt.column_int64(0)?;
    println!("{id}: {name}");
}
```

## Prepared statements

```rust
let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

stmt.bind(1, "Alice")?;
stmt.bind(2, "alice@example.com")?;
stmt.step()?;
stmt.reset()?;

stmt.bind(1, "Bob")?;
stmt.bind(2, "bob@example.com")?;
stmt.step()?;
```

## Transactions

```rust
let tx = db.begin()?;

db.execute("INSERT INTO users (name) VALUES ('Alice')")?;
db.execute("INSERT INTO users (name) VALUES ('Bob')")?;

if error_occurred {
    tx.rollback()?;
} else {
    tx.commit()?;
}
```

## Error handling

All fallible operations return `wlite::Result<T>`, which is `Result<T, wlite::Error>`.

```rust
use wlite::{Database, Error};

match Database::open("app.db") {
    Ok(db) => { /* use db */ }
    Err(Error::NotFound) => eprintln!("Database not found"),
    Err(Error::Io(msg)) => eprintln!("I/O error: {msg}"),
    Err(e) => eprintln!("Error: {e}"),
}
```

Error variants:

| Variant | Meaning |
|---------|---------|
| `Error::Unknown` | General error |
| `Error::NotFound` | File or resource not found |
| `Error::Memory` | Allocation failed |
| `Error::Io(String)` | I/O error |
| `Error::Corrupt` | Corrupt data |
| `Error::Range` | Index out of range |

## Memory safety

The Rust binding uses `Drop` implementations to ensure proper cleanup. `Database`, `Model`, `Statement`, and `Transaction` are automatically freed when they go out of scope.

```rust
{
    let db = Database::open("app.db")?;
    // use db...
} // db is closed automatically
```

Models are immutable after loading and can be shared across threads via `Arc<Model>`:

```rust
use std::sync::Arc;

let model = Arc::new(Model::load("app.wlite")?);

// Clone for another thread
let model_clone = Arc::clone(&model);
std::thread::spawn(move || {
    let db = Database::open("app.db").unwrap();
    db.migrate(&model_clone).unwrap();
});
```

Database connections are not thread-safe. Use one connection per thread.
