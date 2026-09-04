---
title: Rust Binding Overview
description: Overview, installation, and quick start guide for the wlite Rust binding.
---

# Rust Binding

The `wlite` crate provides safe, ergonomic access to libwlite from Rust. It wraps the C ABI with Rust-native types, error handling, and RAII. The binding is designed to feel idiomatic to Rust developers while maintaining full access to libwlite features.

All resource types implement `Drop` to ensure proper cleanup. Error handling uses Rust's standard `Result` type. There is no need to manually free any resource. The crate compiles libwlite from source using the `cc` crate, so a C11 compiler is required on all platforms.

## Installation

Add the `wlite` crate to your `Cargo.toml`:

```toml
[dependencies]
wlite = "0.2.0"
```

For the latest development version, you can point to the git repository:

```toml
[dependencies]
wlite = { git = "https://github.com/dbwarden-org/wlite.git" }
```

### Prerequisites

The crate builds libwlite from source. You need a C11 compiler and the SQLite3 development library.

### Linux

Debian and Ubuntu:

```bash
sudo apt-get install build-essential libsqlite3-dev
```

Fedora:

```bash
sudo dnf install gcc sqlite-devel
```

Arch Linux:

```bash
sudo pacman -S base-devel sqlite
```

### macOS

Install the Xcode command line tools and SQLite via Homebrew:

```bash
xcode-select --install
brew install sqlite
```

If Homebrew is not installed, install it first:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Windows

Install Visual Studio Build Tools with the C++ workload. Then install the SQLite development libraries using vcpkg:

```bash
vcpkg install sqlite3:x64-windows
```

Alternatively, you can download the SQLite amalgamation and set the `SQLITE3_LIB_DIR` environment variable to the directory containing `sqlite3.c`.

### Verifying the installation

After setting up prerequisites, build your project:

```bash
cargo build
```

If the build succeeds, the `wlite` crate is ready to use.

## Quick start

The core workflow is: load a model, open a database, run migrations, and query data.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let rows = db.query("SELECT * FROM users")?;
    for row in rows {
        let id: i64 = row.get(0)?;
        let name: String = row.get(1)?;
        let email: String = row.get(2)?;
        println!("{id}: {name} <{email}>");
    }

    Ok(())
}
```

This example loads a `.wlite` schema file, opens (or creates) a SQLite database, applies any pending migrations, and then queries all rows from the `users` table.

### Inserting data

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, "Alice")?;
    stmt.bind(2, "alice@example.com")?;
    stmt.step()?;

    stmt.reset()?;
    stmt.bind(1, "Bob")?;
    stmt.bind(2, "bob@example.com")?;
    stmt.step()?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Total users: {count}");

    Ok(())
}
```

### Using transactions

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let tx = db.begin()?;

    db.execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')")?;
    db.execute("INSERT INTO users (name, email) VALUES ('Diana', 'diana@example.com')")?;

    tx.commit()?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Total users: {count}");

    Ok(())
}
```

### Error handling basics

Every fallible operation returns `wlite::Result<T>`. Use the `?` operator to propagate errors up the call stack:

```rust
fn create_user(db: &Database, name: &str, email: &str) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, name)?;
    stmt.bind(2, email)?;
    stmt.step()?;
    Ok(())
}
```

For error matching, use `match` on specific variants:

```rust
use wlite::{Database, Error};

fn open_database(path: &str) {
    match Database::open(path) {
        Ok(db) => {
            println!("Database opened");
        }
        Err(Error::NotFound) => {
            eprintln!("File not found: {path}");
        }
        Err(Error::Io(msg)) => {
            eprintln!("I/O error: {msg}");
        }
        Err(e) => {
            eprintln!("Unexpected error: {e}");
        }
    }
}
```

## Types

The `wlite` crate defines several core types. Each wraps a corresponding C object from libwlite.

| Rust Type | C Equivalent | Description |
|-----------|--------------|-------------|
| `Database` | `wlite_db` | Open database connection. Manages the SQLite connection and provides methods for executing SQL, preparing statements, and beginning transactions. |
| `Model` | `wlite_model` | Loaded `.wlite` schema. Immutable after loading. Can be shared across threads via `Arc<Model>`. |
| `Statement` | `wlite_stmt` | Prepared SQL statement. Supports parameter binding, stepping through results, and reading column values. |
| `Transaction` | `wlite_tx` | Active database transaction. Can be committed or rolled back. Automatically rolled back if dropped without committing. |
| `Row` | result row | Single row returned from a query. Provides typed access to column values by index. |
| `Error` | `wlite_result` | Error result. An enum with variants for each class of error that libwlite can produce. |

### Database

The `Database` type is the primary entry point. It wraps a `wlite_db` handle and manages its lifetime via `Drop`.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("myapp.db")?;

    db.execute("CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, name TEXT)")?;
    db.execute("INSERT INTO items (name) VALUES ('Widget')")?;

    Ok(())
}
```

When `db` goes out of scope, the underlying SQLite connection is closed automatically. There is no need to call any close function.

### Model

The `Model` type represents a parsed `.wlite` schema file. It is immutable after loading and can be shared across threads using `Arc<Model>`.

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Model::load("schema.wlite")?;
    println!("Model loaded successfully");

    // model is immutable and thread-safe
    Ok(())
}
```

### Statement

The `Statement` type wraps a prepared SQL statement. It supports binding parameters, stepping through result rows, and reading column values.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("myapp.db")?;

    let mut stmt = db.prepare("SELECT * FROM items WHERE id = ?")?;
    stmt.bind(1, 1i64)?;

    while stmt.step()? {
        let name: String = stmt.column_text(1)?;
        println!("Item: {name}");
    }

    Ok(())
}
```

When a `Statement` is dropped, it is finalized automatically. You can also call `reset` to reuse a statement with different parameters.

### Transaction

The `Transaction` type represents an active database transaction. It can be committed or rolled back. If dropped without committing, it is automatically rolled back.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("myapp.db")?;

    let tx = db.begin()?;
    db.execute("INSERT INTO items (name) VALUES ('Gadget')")?;
    tx.commit()?;

    Ok(())
}
```

### Row

The `Row` type represents a single row from a query result. It provides typed accessors for column values.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("myapp.db")?;

    let rows = db.query("SELECT id, name FROM items")?;
    for row in rows {
        let id: i64 = row.get(0)?;
        let name: String = row.get(1)?;
        println!("{id}: {name}");
    }

    Ok(())
}
```

### Error

The `Error` type is an enum with variants for each class of error. It implements `std::fmt::Display` and `std::error::Error`.

```rust
use wlite::Error;

fn describe_error(err: &Error) {
    match err {
        Error::Error => println!("General error"),
        Error::InvalidArgument => println!("Invalid argument"),
        Error::Memory => println!("Out of memory"),
        Error::Io(msg) => println!("I/O error: {msg}"),
        Error::Parse(msg) => println!("Parse error: {msg}"),
        Error::Model(msg) => println!("Model error: {msg}"),
        Error::Sqlite(msg) => println!("SQLite error: {msg}"),
        Error::Constraint(msg) => println!("Constraint violation: {msg}"),
        Error::NotFound => println!("Not found"),
        Error::Busy => println!("Database is locked"),
        Error::Transaction => println!("Transaction error"),
    }
}
```

## Crate features

The `wlite` crate exposes optional features that can be enabled in `Cargo.toml`:

```toml
[dependencies]
wlite = { version = "0.2", features = ["unstable"] }
```

### Default features

By default, the crate enables all stable features. The default feature set includes the core API for database operations, prepared statements, transactions, model loading, and migration.

### Feature list

| Feature | Description |
|---------|-------------|
| (default) | Core API: Database, Model, Statement, Transaction, Error |
| `unstable` | Unstable API additions that may change between minor versions |

When building without default features, you get a minimal crate with only the types needed for basic SQL execution:

```toml
[dependencies]
wlite = { version = "0.2", default-features = false }
```

### Conditional compilation

The crate uses `cfg` attributes internally to handle platform differences. On Windows, it links against the appropriate SQLite library. On Linux and macOS, it uses pkg-config or direct linking.

You can check the active features at compile time:

```rust
#[cfg(feature = "unstable")]
fn unstable_feature_example() {
    println!("Unstable features are enabled");
}
```

## Platform notes

### Linux

The crate links against `libsqlite3` by default. If your system uses a different path, set the `SQLITE3_LIB_DIR` environment variable:

```bash
export SQLITE3_LIB_DIR=/usr/lib/x86_64-linux-gnu
cargo build
```

### macOS

On Apple Silicon, the crate builds for the native architecture. For universal binaries, set the appropriate environment variables:

```bash
export CARGO_TARGET_AARCH64_APPLE_DARWIN_LINKER="clang -arch arm64"
export CARGO_TARGET_X86_64_APPLE_DARWIN_LINKER="clang -arch x86_64"
```

### Windows

The crate requires the MSVC toolchain. Make sure you are using the correct target:

```bash
rustup default stable-x86_64-pc-windows-msvc
cargo build
```

## Version compatibility

The `wlite` crate tracks the libwlite ABI version. Version 0.2.x of the crate is compatible with ABI version 1. Check the crate documentation for the latest compatibility matrix.

```rust
fn check_compatibility() {
    println!("wlite crate version: {}", env!("CARGO_PKG_VERSION"));
}
```

## Next steps

After installation, proceed to the migration guide for schema management, the queries guide for data access patterns, or the error handling guide for detailed error handling strategies.
