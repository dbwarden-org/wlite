---
title: Error Handling
description: Error enum variants, Result type, handling patterns, cleanup with Drop, thread safety, and memory safety guarantees in the wlite Rust binding.
---

# Error Handling

This guide covers the `Error` enum, `wlite::Result`, common error handling patterns, cleanup with `Drop`, thread safety, and memory safety guarantees.

## The Error enum

The `wlite::Error` enum has twelve variants, one for each class of error that libwlite can produce:

```rust
pub enum Error {
    Error,
    InvalidArgument,
    Memory,
    Io(String),
    Parse(String),
    Model(String),
    Sqlite(String),
    Constraint(String),
    NotFound,
    Busy,
    Transaction,
}
```

### Variant descriptions

| Variant | Meaning | Common causes |
|---------|---------|---------------|
| `Error` | General error | Unexpected failure with no specific category |
| `InvalidArgument` | Invalid parameter | Null pointer, bad index, out-of-range value |
| `Memory` | Allocation failed | System out of memory |
| `Io(String)` | I/O error | Disk full, permission denied, file not found |
| `Parse(String)` | Parse error | Malformed `.wlite` schema syntax |
| `Model(String)` | Model error | Invalid table reference, missing field |
| `Sqlite(String)` | SQLite error | Underlying SQLite engine failure |
| `Constraint(String)` | Constraint violation | UNIQUE, CHECK, or FOREIGN KEY failure |
| `NotFound` | Resource not found | Missing table, column, or file |
| `Busy` | Database locked | Another connection holds a lock |
| `Transaction` | Transaction error | Invalid transaction state |

### Display implementation

The `Error` type implements `std::fmt::Display`, so it can be printed directly:

```rust
use wlite::{Database, Error};

fn main() {
    match Database::open("missing.db") {
        Ok(_db) => println!("Opened"),
        Err(e) => println!("Error: {e}"),
    }
}
```

### Error source

The `Error` type implements `std::error::Error`, so it can be used with the standard error handling ecosystem:

```rust
use wlite::{Database, Error};
use std::error::Error as StdError;

fn main() {
    match Database::open("missing.db") {
        Ok(_db) => println!("Opened"),
        Err(e) => {
            println!("Error: {e}");
            if let Some(source) = e.source() {
                println!("Caused by: {source}");
            }
        }
    }
}
```

## wlite::Result

The `wlite::Result` type is an alias for `Result<T, wlite::Error>`:

```rust
pub type Result<T> = std::result::Result<T, Error>;
```

All fallible operations in the crate return `wlite::Result<T>`. This integrates with Rust's `?` operator for error propagation.

### Function returning Result

```rust
use wlite::{Database, Model};

fn initialize_database(model_path: &str, db_path: &str) -> wlite::Result<()> {
    let model = Model::load(model_path)?;
    let db = Database::open(db_path)?;
    db.migrate(&model)?;
    Ok(())
}
```

### Method returning Result

```rust
use wlite::{Database, Error};

impl Database {
    fn ensure_table(&self, name: &str) -> wlite::Result<()> {
        self.execute(&format!(
            "CREATE TABLE IF NOT EXISTS {name} (id INTEGER PRIMARY KEY, created_at TEXT)"
        ))?;
        Ok(())
    }
}
```

## Error handling patterns

### Match on specific variants

The most explicit pattern. Use when different variants require different recovery strategies:

```rust
use wlite::{Database, Error};

fn open_with_recovery(path: &str) -> wlite::Result<Database> {
    match Database::open(path) {
        Ok(db) => Ok(db),
        Err(Error::NotFound) => {
            eprintln!("Database not found, creating new one");
            Database::open(path)
        }
        Err(Error::Busy) => {
            eprintln!("Database is locked, retrying...");
            std::thread::sleep(std::time::Duration::from_millis(100));
            Database::open(path)
        }
        Err(Error::Io(msg)) => {
            eprintln!("I/O error: {msg}");
            Err(Error::Io(msg))
        }
        Err(e) => Err(e),
    }
}
```

### The ? operator

The `?` operator propagates errors up the call stack. It is the most common pattern:

```rust
use wlite::{Database, Model};

fn process_data(model_path: &str, db_path: &str) -> wlite::Result<()> {
    let model = Model::load(model_path)?;
    let db = Database::open(db_path)?;
    db.migrate(&model)?;

    let mut stmt = db.prepare("INSERT INTO logs (message) VALUES (?)")?;
    stmt.bind(1, "Processing started")?;
    stmt.step()?;

    Ok(())
}
```

### The ? operator with conversion

Implement `From<wlite::Error>` for your own error type to use `?` seamlessly:

```rust
use wlite::{Database, Error};

#[derive(Debug)]
enum AppError {
    Database(Error),
    Other(String),
}

impl From<wlite::Error> for AppError {
    fn from(e: wlite::Error) -> Self {
        AppError::Database(e)
    }
}

fn run() -> Result<(), AppError> {
    let db = Database::open("app.db")?;
    Ok(())
}
```

### Unwrap and expect

Use `unwrap` or `expect` when failure is truly unexpected. These panic on error, so use them only in tests, examples, or initialization code:

```rust
use wlite::Database;

fn main() {
    let db = Database::open("app.db")
        .expect("Failed to open database");

    db.execute("SELECT 1")
        .expect("Failed to execute query");
}
```

###组合 with map_err

Transform error messages while preserving the error type:

```rust
use wlite::{Database, Model};

fn load_model(path: &str) -> wlite::Result<Model> {
    Model::load(path).map_err(|e| {
        wlite::Error::Parse(format!("Failed to load model from {path}: {e}"))
    })
}
```

### Catching and logging

Log errors without propagating them:

```rust
use wlite::{Database, Error};

fn try_optional_operation(db: &Database) {
    match db.execute("INSERT INTO logs (message) VALUES ('test')") {
        Ok(()) => {
            println!("Operation succeeded");
        }
        Err(Error::Busy) => {
            eprintln!("Database busy, skipping log entry");
        }
        Err(Error::Constraint(msg)) => {
            eprintln!("Constraint violation: {msg}");
        }
        Err(e) => {
            eprintln!("Unexpected error: {e}");
        }
    }
}
```

## Cleanup with Drop

All resource types in the `wlite` crate implement `Drop`. This ensures that resources are released when they go out of scope, even if an error occurs.

### Automatic cleanup on scope exit

```rust
use wlite::{Database, Model};

fn example() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    {
        let mut stmt = db.prepare("SELECT * FROM users")?;
        while stmt.step()? {
            let name: String = stmt.column_text(0)?;
            println!("{name}");
        }
    } // stmt is dropped here, finalized automatically

    Ok(())
} // db and model are dropped here, closed automatically
```

### Cleanup on error

Resources are cleaned up even when errors occur:

```rust
use wlite::{Database, Error, Model};

fn example_with_error() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, "Alice")?;
    stmt.bind(2, "alice@example.com")?;
    stmt.step()?;

    // If this fails, stmt, db, and model are all cleaned up
    let mut query = db.prepare("SELECT * FROM users WHERE id = ?")?;
    query.bind(1, 999i64)?;

    while query.step()? {
        // This may not execute if the insert above failed
    }

    Ok(())
}
```

### Explicit drop

You can drop resources explicitly to release them early:

```rust
use wlite::{Database, Model};

fn example_explicit_drop() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    // Use the model for migration
    drop(model); // Model is freed immediately

    // Continue using the database
    db.execute("INSERT INTO users (name) VALUES ('Alice')")?;

    Ok(())
}
```

### Preventing use after drop

Rust's ownership system prevents using a value after it has been dropped:

```rust
use wlite::Database;

fn example_no_use_after_drop() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    db.execute("SELECT 1")?;
    drop(db);

    // This would be a compile error:
    // db.execute("SELECT 2")?;

    Ok(())
}
```

## Thread safety

### Models are thread-safe

Models are immutable after loading. They can be shared across threads via `Arc<Model>`:

```rust
use std::sync::Arc;
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Arc::new(Model::load("app.wlite")?);

    let model_clone = Arc::clone(&model);
    std::thread::spawn(move || {
        // model_clone is safe to use in another thread
        println!("Model hash: {:?}", model_clone.hash());
    }).join().unwrap();

    Ok(())
}
```

### Database connections are not thread-safe

Each thread must have its own database connection. Do not share a `Database` across threads:

```rust
use std::sync::Arc;
use std::thread;
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Arc::new(Model::load("app.wlite")?);

    let handles: Vec<_> = (0..4)
        .map(|i| {
            let model = Arc::clone(&model);
            thread::spawn(move || {
                // Each thread gets its own connection
                let db = Database::open("app.db").unwrap();
                db.migrate(&model).unwrap();

                for _ in 0..100 {
                    db.execute(&format!(
                        "INSERT INTO work_items (thread_id) VALUES ({i})"
                    )).unwrap();
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

### Statements and transactions are not thread-safe

Statements and transactions belong to a database connection. Do not share them across threads:

```rust
use std::thread;
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    // This would be incorrect:
    // let stmt = db.prepare("SELECT * FROM users")?;
    // thread::spawn(move || {
    //     while stmt.step().unwrap() { ... }
    // });

    // Each thread should prepare its own statements
    let db_clone = Database::open("app.db")?;
    thread::spawn(move || {
        let mut stmt = db_clone.prepare("SELECT * FROM users").unwrap();
        while stmt.step().unwrap() {
            // ...
        }
    }).join().unwrap();

    Ok(())
}
```

### Thread safety summary

| Object | Thread-safe? | Notes |
|--------|-------------|-------|
| `Model` | Yes | Immutable after loading |
| `Arc<Model>` | Yes | Shared ownership across threads |
| `Database` | No | One connection per thread |
| `Statement` | No | Belongs to a connection |
| `Transaction` | No | Belongs to a connection |
| `Row` | No | Belongs to a statement |
| `Record` | No | Snapshot of a row, but not safe to share |

## Memory safety guarantees

### No double-free

Rust's ownership system prevents double-free. Each value has exactly one owner, and is freed when that owner goes out of scope:

```rust
use wlite::Database;

fn example_no_double_free() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    drop(db);
    // db is now invalid, and the compiler prevents further use

    // This would be a compile error:
    // drop(db);

    Ok(())
}
```

### No use-after-free

Rust's borrow checker prevents use-after-free. You cannot use a value after it has been moved or dropped:

```rust
use wlite::Database;

fn example_no_use_after_free() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    let db2 = db; // db is moved to db2

    // This would be a compile error:
    // db.execute("SELECT 1")?;

    db2.execute("SELECT 1")?;
    Ok(())
}
```

### No null pointer dereference

Rust does not have null pointers. All optional values are represented with `Option<T>`, and the compiler enforces handling:

```rust
use wlite::Database;

fn example_no_null() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    // All operations return Result, not nullable pointers
    let mut stmt = db.prepare("SELECT * FROM users")?;
    while stmt.step()? {
        // column_text returns Result<String>, not Option<*const c_char>
        let name: String = stmt.column_text(0)?;
        println!("{name}");
    }

    Ok(())
}
```

### No data races

Rust's type system prevents data races at compile time. Mutable access is exclusive, and shared access is immutable:

```rust
use std::sync::Arc;
use std::thread;
use wlite::{Database, Model};

fn example_no_data_races() -> wlite::Result<()> {
    let model = Arc::new(Model::load("app.wlite")?);

    // Multiple threads can read the model (immutable)
    let handles: Vec<_> = (0..4)
        .map(|_| {
            let model = Arc::clone(&model);
            thread::spawn(move || {
                // Reading is safe from multiple threads
                let _hash = model.hash();
            })
        })
        .collect();

    for handle in handles {
        handle.join().unwrap();
    }

    Ok(())
}
```

### RAII for all resources

Every resource type in the `wlite` crate follows RAII (Resource Acquisition Is Initialization). Resources are acquired in constructors and released in destructors:

```rust
use wlite::{Database, Model};

fn example_raii() -> wlite::Result<()> {
    // Resources acquired
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;
    let mut stmt = db.prepare("SELECT * FROM users")?;

    // Use resources
    while stmt.step()? {
        let name: String = stmt.column_text(0)?;
        println!("{name}");
    }

    // Resources released in reverse order:
    // stmt dropped, then db dropped, then model dropped
    Ok(())
}
```

## Common error patterns

### Handling busy errors with retry

```rust
use wlite::{Database, Error};
use std::thread;
use std::time::Duration;

fn open_with_retry(path: &str, max_retries: u32) -> wlite::Result<Database> {
    for attempt in 0..max_retries {
        match Database::open(path) {
            Ok(db) => return Ok(db),
            Err(Error::Busy) => {
                eprintln!("Attempt {attempt}: Database busy, retrying...");
                thread::sleep(Duration::from_millis(100 * (attempt + 1)));
            }
            Err(e) => return Err(e),
        }
    }
    Err(Error::Busy)
}
```

### Handling constraint violations

```rust
use wlite::{Database, Error};

fn insert_user_safe(db: &Database, name: &str, email: &str) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, name)?;
    stmt.bind(2, email)?;

    match stmt.step() {
        Ok(()) => Ok(()),
        Err(Error::Constraint(msg)) => {
            if msg.contains("UNIQUE") {
                eprintln!("User with email {email} already exists");
                Ok(()) // Treat as non-fatal
            } else {
                Err(Error::Constraint(msg))
            }
        }
        Err(e) => Err(e),
    }
}
```

### Mapping errors to application types

```rust
use wlite::{Database, Error};

#[derive(Debug)]
enum AppError {
    SchemaError(String),
    DatabaseError(String),
    ValidationError(String),
}

impl From<wlite::Error> for AppError {
    fn from(e: wlite::Error) -> Self {
        match e {
            Error::Parse(msg) => AppError::SchemaError(msg),
            Error::Model(msg) => AppError::SchemaError(msg),
            Error::Sqlite(msg) => AppError::DatabaseError(msg),
            Error::Constraint(msg) => AppError::ValidationError(msg),
            Error::Busy => AppError::DatabaseError("Database is locked".to_string()),
            other => AppError::DatabaseError(other.to_string()),
        }
    }
}

fn run() -> Result<(), AppError> {
    let db = Database::open("app.db")?;
    Ok(())
}
```

### Logging and continuing

```rust
use wlite::{Database, Error};

fn batch_with_logging(db: &Database) -> wlite::Result<()> {
    let items = vec!["Alice", "Bob", "Charlie"];

    for name in items {
        let mut stmt = db.prepare("INSERT INTO users (name) VALUES (?)")?;
        stmt.bind(1, name)?;

        match stmt.step() {
            Ok(()) => {
                println!("Inserted {name}");
            }
            Err(Error::Constraint(_)) => {
                eprintln!("Skipping {name}: already exists");
            }
            Err(e) => {
                eprintln!("Failed to insert {name}: {e}");
                return Err(e);
            }
        }
    }

    Ok(())
}
```

## Debugging errors

### Printing the full error chain

```rust
use wlite::{Database, Error};
use std::error::Error as StdError;

fn debug_error(e: &Error) {
    println!("Error: {e}");

    let mut source = e.source();
    while let Some(cause) = source {
        println!("  Caused by: {cause}");
        source = cause.source();
    }
}

fn main() {
    match Database::open("missing.db") {
        Ok(_) => {}
        Err(e) => debug_error(&e),
    }
}
```

### Usingdbg for debugging

```rust
use wlite::Database;

fn debug_example() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT * FROM users")?;
    while stmt.step()? {
        let name: String = stmt.column_text(0)?;
        dbg!(&name);
    }

    Ok(())
}
```
