---
title: Rust Binding
description: Safe, ergonomic Rust bindings for libwlite.
---

# Rust Binding

The `wlite` crate provides safe, ergonomic access to libwlite from Rust. It wraps the C ABI with Rust-native types, error handling, and RAII.

The binding is designed to feel idiomatic to Rust developers while maintaining full access to libwlite's features. All resource types implement `Drop` to ensure proper cleanup, and error handling uses Rust's standard `Result` type.

## Installation

Add the `wlite` crate to your `Cargo.toml`:

```toml
[dependencies]
wlite = "0.1"
```

The crate compiles libwlite from source using the `cc` crate. You need a C11 compiler and SQLite3 development library installed on your system.

### Linux prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install build-essential libsqlite3-dev

# Fedora
sudo dnf install gcc sqlite-devel

# Arch
sudo pacman -S base-devel sqlite
```

### macOS prerequisites

```bash
# Using Homebrew
brew install sqlite

# Xcode command line tools
xcode-select --install
```

### Windows prerequisites

Install Visual Studio Build Tools and the SQLite development libraries. You can use vcpkg for dependency management:

```bash
vcpkg install sqlite3:x64-windows
```

## Basic usage

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

This example demonstrates the core workflow: load a model, open a database, run migrations, and query data.

### Working with multiple tables

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    db.execute("INSERT INTO users (name, email) VALUES (?, ?)")?;

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

## Types

| Rust Type | C Equivalent | Description |
|-----------|--------------|-------------|
| `Database` | `wlite_db` | Open database connection |
| `Model` | `wlite_model` | Loaded .wlite schema |
| `Statement` | `wlite_stmt` | Prepared SQL statement |
| `Transaction` | `wlite_tx` | Active transaction |
| `Row` | result row | Single row from a query |
| `Error` | `wlite_result` | Error result |

The `Database` type manages the connection lifetime and provides methods for executing SQL, preparing statements, and beginning transactions.

The `Model` type represents a parsed `.wlite` schema file. It is immutable after loading and can be shared across threads.

The `Statement` type wraps a prepared SQL statement. It supports parameter binding, stepping through results, and reading column values.

The `Transaction` type represents an active database transaction. It can be committed or rolled back.

## Database operations

```rust
use wlite::Database;

fn database_operations() -> wlite::Result<()> {
    // Open a database
    let db = Database::open("app.db")?;

    // Execute DDL
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")?;
    db.execute("CREATE INDEX idx_test_name ON test(name)")?;

    // Execute DML
    db.execute("INSERT INTO test (name) VALUES ('hello')")?;
    db.execute("UPDATE test SET name = 'world' WHERE id = 1")?;
    db.execute("DELETE FROM test WHERE id = 1")?;

    // Query returning multiple rows
    let rows = db.query("SELECT * FROM test")?;
    for row in rows {
        let id: i64 = row.get(0)?;
        let name: String = row.get(1)?;
        println!("{id}: {name}");
    }

    // Query returning a single value
    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM test")?;
    println!("Count: {count}");

    // Execute with parameters
    db.execute("INSERT INTO test (name) VALUES (?)")?;

    Ok(())
}
```

### Batch operations

```rust
use wlite::Database;

fn batch_insert(db: &Database) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

    let users = vec![
        ("Alice", "alice@example.com"),
        ("Bob", "bob@example.com"),
        ("Charlie", "charlie@example.com"),
        ("Diana", "diana@example.com"),
    ];

    for (name, email) in users {
        stmt.bind(1, name)?;
        stmt.bind(2, email)?;
        stmt.step()?;
        stmt.reset()?;
    }

    Ok(())
}
```

### Query with column access

```rust
use wlite::Database;

fn query_columns(db: &Database) -> wlite::Result<()> {
    let mut stmt = db.prepare("SELECT id, name, email, created_at FROM users")?;

    println!("Columns: {}", stmt.column_count());

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let name: String = stmt.column_text(1)?;
        let email: String = stmt.column_text(2)?;
        let created: String = stmt.column_text(3)?;

        println!("User {id}: {name} <{email}> created at {created}");
    }

    Ok(())
}
```

## Prepared statements

Prepared statements allow you to compile SQL once and execute it multiple times with different parameters. This improves performance and prevents SQL injection.

```rust
use wlite::Database;

fn prepared_statements(db: &Database) -> wlite::Result<()> {
    // Prepare an INSERT statement
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

    // Insert Alice
    stmt.bind(1, "Alice")?;
    stmt.bind(2, "alice@example.com")?;
    stmt.step()?;
    stmt.reset()?;

    // Insert Bob
    stmt.bind(1, "Bob")?;
    stmt.bind(2, "bob@example.com")?;
    stmt.step()?;
    stmt.reset()?;

    // Insert Charlie
    stmt.bind(1, "Charlie")?;
    stmt.bind(2, "charlie@example.com")?;
    stmt.step()?;

    // Prepare a SELECT statement with parameter
    let mut query = db.prepare("SELECT * FROM users WHERE name = ?")?;
    query.bind(1, "Alice")?;

    while query.step()? {
        let id: i64 = query.column_int64(0)?;
        let name: String = query.column_text(1)?;
        println!("Found user {id}: {name}");
    }

    Ok(())
}
```

### Column access methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `column_count()` | `i32` | Number of columns in result |
| `column_name(i)` | `String` | Name of column at index |
| `column_type(i)` | `ColumnType` | Data type of column |
| `column_int64(i)` | `i64` | Integer value |
| `column_double(i)` | `f64` | Floating point value |
| `column_text(i)` | `String` | Text value |
| `column_blob(i)` | `Vec<u8>` | Binary data |

## Transactions

Transactions ensure that a group of operations either all succeed or all fail. This maintains data consistency.

```rust
use wlite::Database;

fn transfer_funds(db: &Database, from: i64, to: i64, amount: i64) -> wlite::Result<()> {
    let tx = db.begin()?;

    // Debit sender
    db.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?")?;

    // Credit receiver
    db.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?")?;

    // Verify sender has sufficient funds
    let balance: i64 = db.query_scalar("SELECT balance FROM accounts WHERE id = ?")?;

    if balance < 0 {
        tx.rollback()?;
        return Err(wlite::Error::Range);
    }

    tx.commit()?;
    Ok(())
}
```

### Nested transaction handling

```rust
use wlite::Database;

fn batch_with_transaction(db: &Database) -> wlite::Result<()> {
    let tx = db.begin()?;

    let result = (|| -> wlite::Result<()> {
        let mut stmt = db.prepare("INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)")?;

        for order in &orders {
            stmt.bind(1, order.user_id)?;
            stmt.bind(2, order.product_id)?;
            stmt.bind(3, order.quantity)?;
            stmt.step()?;
            stmt.reset()?;
        }

        Ok(())
    })();

    match result {
        Ok(()) => tx.commit()?,
        Err(e) => {
            tx.rollback()?;
            return Err(e);
        }
    }

    Ok(())
}
```

## Error handling

All fallible operations return `wlite::Result<T>`, which is `Result<T, wlite::Error>`.

```rust
use wlite::{Database, Error};

fn error_handling_example() {
    match Database::open("app.db") {
        Ok(db) => {
            println!("Database opened successfully");
        }
        Err(Error::NotFound) => {
            eprintln!("Database file not found");
        }
        Err(Error::Io(msg)) => {
            eprintln!("I/O error: {msg}");
        }
        Err(Error::Sqlite(msg)) => {
            eprintln!("SQLite error: {msg}");
        }
        Err(Error::Constraint(msg)) => {
            eprintln!("Constraint violation: {msg}");
        }
        Err(Error::Memory) => {
            eprintln!("Out of memory");
        }
        Err(Error::Busy) => {
            eprintln!("Database is locked");
        }
        Err(Error::Error) => {
            eprintln!("Unknown error occurred");
        }
        _ => {}
    }
}
```

### Error variants

| Variant | Meaning | Common causes |
|---------|---------|---------------|
| `Error::Error` | General error | Unexpected failure |
| `Error::InvalidArgument` | Invalid parameter | Null pointer, bad index |
| `Error::Memory` | Allocation failed | System out of memory |
| `Error::Io(String)` | I/O error | Disk full, permissions, file not found |
| `Error::Parse(String)` | Parse error | Malformed .wlite schema |
| `Error::Model(String)` | Model error | Invalid table or field reference |
| `Error::Sqlite(String)` | SQLite error | Underlying SQLite failure |
| `Error::Constraint(String)` | Constraint violation | UNIQUE or CHECK failure |
| `Error::NotFound` | Resource not found | Missing table, column, or file |
| `Error::Busy` | Database locked | Another connection holds a lock |
| `Error::Transaction` | Transaction error | Invalid transaction state |

### Error propagation with `?` operator

```rust
use wlite::{Database, Model};

fn process_data(path: &str) -> wlite::Result<()> {
    let model = Model::load(path)?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let mut stmt = db.prepare("INSERT INTO logs (message, level) VALUES (?, ?)")?;
    stmt.bind(1, "Processing started")?;
    stmt.bind(2, "info")?;
    stmt.step()?;

    Ok(())
}
```

## Memory management

The Rust binding uses `Drop` implementations to ensure proper cleanup. `Database`, `Model`, `Statement`, and `Transaction` are automatically freed when they go out of scope.

```rust
{
    let db = Database::open("app.db")?;
    // use db...
} // db is closed automatically
```

### Scope-based cleanup

```rust
use wlite::{Database, Model};

fn memory_management_example() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    // Statement is dropped at end of scope
    {
        let mut stmt = db.prepare("SELECT * FROM users")?;
        while stmt.step()? {
            let name: String = stmt.column_text(0)?;
            println!("{name}");
        }
    } // stmt is finalized here

    Ok(())
} // db and model are dropped here
```

### Preventing double-free

The Rust binding prevents double-free by design. Once a value is dropped, it cannot be used again:

```rust
let db = Database::open("app.db")?;
db.execute("SELECT 1")?;
drop(db);
// db.execute("SELECT 1")?; // Compile error: value used after being moved
```

## Thread safety

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

### Multi-threaded worker pool

```rust
use std::sync::Arc;
use std::thread;

fn multi_threaded_example() -> wlite::Result<()> {
    let model = Arc::new(Model::load("app.wlite")?);

    let handles: Vec<_> = (0..4)
        .map(|i| {
            let model = Arc::clone(&model);
            thread::spawn(move || {
                let db = Database::open("app.db").unwrap();
                db.migrate(&model).unwrap();

                for _ in 0..100 {
                    db.execute("INSERT INTO work_items (thread_id, data) VALUES (?, ?)")
                        .unwrap();
                }
            })
        })
        .collect();

    for handle in handles {
        handle.join().unwrap();
    }

    Ok(())
}
```

Database connections are not thread-safe. Use one connection per thread, or protect access with a mutex.

### Using a connection pool pattern

```rust
use std::sync::{Arc, Mutex};

struct ConnectionPool {
    connections: Mutex<Vec<Database>>,
}

impl ConnectionPool {
    fn get(&self) -> Option<Database> {
        let mut conns = self.connections.lock().unwrap();
        conns.pop()
    }

    fn put(&self, db: Database) {
        let mut conns = self.connections.lock().unwrap();
        conns.push(db);
    }
}
```

## Complete example

Here is a complete, working program that demonstrates all major features:

```rust
use wlite::{Database, Model, Error};

#[derive(Debug)]
struct User {
    id: i64,
    name: String,
    email: String,
}

fn main() -> wlite::Result<()> {
    // Load the schema model
    let model = Model::load("app.wlite")?;

    // Open the database
    let db = Database::open("app.db")?;

    // Run migrations
    db.migrate(&model)?;

    // Create a table manually if not using migrations
    db.execute(
        "CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT NOT NULL UNIQUE,
            created_at TEXT DEFAULT (datetime('now'))
        )",
    )?;

    // Insert users using prepared statements
    let users = vec![
        ("Alice", "alice@example.com"),
        ("Bob", "bob@example.com"),
        ("Charlie", "charlie@example.com"),
        ("Diana", "diana@example.com"),
        ("Eve", "eve@example.com"),
    ];

    let mut insert_stmt = db.prepare("INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)")?;
    for (name, email) in &users {
        insert_stmt.bind(1, *name)?;
        insert_stmt.bind(2, *email)?;
        insert_stmt.step()?;
        insert_stmt.reset()?;
    }

    // Query all users
    println!("All users:");
    let mut query_stmt = db.prepare("SELECT id, name, email FROM users ORDER BY name")?;
    while query_stmt.step()? {
        let user = User {
            id: query_stmt.column_int64(0)?,
            name: query_stmt.column_text(1)?,
            email: query_stmt.column_text(2)?,
        };
        println!("  {user:?}");
    }

    // Use a transaction for batch operations
    let tx = db.begin()?;

    let mut update_stmt = db.prepare("UPDATE users SET name = ? WHERE email = ?")?;
    update_stmt.bind(1, "Alice Smith")?;
    update_stmt.bind(2, "alice@example.com")?;
    update_stmt.step()?;

    update_stmt.reset()?;
    update_stmt.bind(1, "Bob Jones")?;
    update_stmt.bind(2, "bob@example.com")?;
    update_stmt.step()?;

    tx.commit()?;

    // Count users
    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Total users: {count}");

    // Search with parameters
    let search = "Alice";
    let mut search_stmt = db.prepare("SELECT id, name, email FROM users WHERE name LIKE ?")?;
    search_stmt.bind(1, format!("%{search}%"))?;

    println!("Search results for '{search}':");
    while search_stmt.step()? {
        let id = search_stmt.column_int64(0)?;
        let name = search_stmt.column_text(1)?;
        let email = search_stmt.column_text(2)?;
        println!("  {id}: {name} <{email}>");
    }

    Ok(())
}
```
