---
title: Queries and Data Access
description: Prepared statements, parameter binding, column access, records, transactions, and iterator patterns in the wlite Rust binding.
---

# Queries and Data Access

This guide covers preparing statements, binding parameters, stepping through results, accessing columns, working with records, using transactions and savepoints, and applying the iterator pattern.

## Preparing statements

Use `Database::prepare` to compile a SQL statement. The returned `Statement` can be executed multiple times with different parameters.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT * FROM users WHERE id = ?")?;

    stmt.bind(1, 1i64)?;

    while stmt.step()? {
        let name: String = stmt.column_text(1)?;
        println!("User: {name}");
    }

    Ok(())
}
```

### Preparing INSERT statements

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

    stmt.bind(1, "Alice")?;
    stmt.bind(2, "alice@example.com")?;
    stmt.step()?;

    stmt.reset()?;
    stmt.bind(1, "Bob")?;
    stmt.bind(2, "bob@example.com")?;
    stmt.step()?;

    println!("Two users inserted");
    Ok(())
}
```

### Preparing UPDATE statements

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("UPDATE users SET email = ? WHERE name = ?")?;

    stmt.bind(1, "alice.smith@example.com")?;
    stmt.bind(2, "Alice")?;
    stmt.step()?;

    println!("User updated");
    Ok(())
}
```

## Binding parameters

Parameters are 1-indexed. Use `bind` to set parameter values. The method accepts anything that implements `Into<wlite::Value>`.

### Text parameters

```rust
use wlite::Database;

fn insert_user(db: &Database, name: &str, email: &str) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, name)?;
    stmt.bind(2, email)?;
    stmt.step()?;
    Ok(())
}
```

### Integer parameters

```rust
use wlite::Database;

fn find_user_by_id(db: &Database, id: i64) -> wlite::Result<()> {
    let mut stmt = db.prepare("SELECT * FROM users WHERE id = ?")?;
    stmt.bind(1, id)?;

    while stmt.step()? {
        let name: String = stmt.column_text(1)?;
        println!("Found: {name}");
    }

    Ok(())
}
```

### Double parameters

```rust
use wlite::Database;

fn insert_price(db: &Database, product_id: i64, price: f64) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO prices (product_id, amount) VALUES (?, ?)")?;
    stmt.bind(1, product_id)?;
    stmt.bind(2, price)?;
    stmt.step()?;
    Ok(())
}
```

### Null parameters

```rust
use wlite::Database;

fn insert_nullable(db: &Database, name: &str) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;
    stmt.bind(1, name)?;
    stmt.bind_null(2)?;
    stmt.step()?;
    Ok(())
}
```

### Blob parameters

```rust
use wlite::Database;

fn insert_blob(db: &Database, data: &[u8]) -> wlite::Result<()> {
    let mut stmt = db.prepare("INSERT INTO blobs (data) VALUES (?)")?;
    stmt.bind_blob(1, data)?;
    stmt.step()?;
    Ok(())
}
```

## Stepping through results

The `step` method advances to the next row in the result set. It returns `Ok(true)` if a row is available, `Ok(false)` when the result set is exhausted, and `Err` on error.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let name: String = stmt.column_text(1)?;
        let email: String = stmt.column_text(2)?;
        println!("{id}: {name} <{email}>");
    }

    Ok(())
}
```

### Checking for no results

```rust
use wlite::Database;

fn find_user(db: &Database, name: &str) -> wlite::Result<Option<(i64, String)>> {
    let mut stmt = db.prepare("SELECT id, email FROM users WHERE name = ?")?;
    stmt.bind(1, name)?;

    if stmt.step()? {
        let id = stmt.column_int64(0)?;
        let email = stmt.column_text(1)?;
        Ok(Some((id, email)))
    } else {
        Ok(None)
    }
}
```

## Column access

After a successful `step`, use column accessor methods to read values. Columns are 0-indexed.

### column_text

Read a text value:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT name, email FROM users")?;

    while stmt.step()? {
        let name: String = stmt.column_text(0)?;
        let email: String = stmt.column_text(1)?;
        println!("{name}: {email}");
    }

    Ok(())
}
```

### column_int64

Read an integer value:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, age FROM users")?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let age: i64 = stmt.column_int64(1)?;
        println!("User {id} is {age} years old");
    }

    Ok(())
}
```

### column_double

Read a floating-point value:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, price FROM products")?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let price: f64 = stmt.column_double(1)?;
        println!("Product {id}: ${price:.2}");
    }

    Ok(())
}
```

### column_blob

Read binary data:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, data FROM blobs")?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let data: Vec<u8> = stmt.column_blob(1)?;
        println!("Blob {id}: {} bytes", data.len());
    }

    Ok(())
}
```

### column_count and column_name

Inspect the result set structure:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;

    let col_count = stmt.column_count();
    println!("Result has {col_count} columns");

    for i in 0..col_count {
        let name = stmt.column_name(i)?;
        println!("  Column {i}: {name}");
    }

    Ok(())
}
```

### Column type checking

Check the type of a column before reading:

```rust
use wlite::{ColumnType, Database};

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT * FROM users")?;

    while stmt.step()? {
        for i in 0..stmt.column_count() {
            let col_type = stmt.column_type(i)?;
            let name = stmt.column_name(i)?;
            match col_type {
                ColumnType::Null => println!("{name}: NULL"),
                ColumnType::Integer => {
                    let val: i64 = stmt.column_int64(i)?;
                    println!("{name}: {val}");
                }
                ColumnType::Real => {
                    let val: f64 = stmt.column_double(i)?;
                    println!("{name}: {val}");
                }
                ColumnType::Text => {
                    let val: String = stmt.column_text(i)?;
                    println!("{name}: {val}");
                }
                ColumnType::Blob => {
                    let val: Vec<u8> = stmt.column_blob(i)?;
                    println!("{name}: {} bytes", val.len());
                }
            }
        }
    }

    Ok(())
}
```

## Records

Records provide a higher-level, name-based interface over a result row. A record owns a snapshot of the row data and is independent of the statement.

### Creating a record from a statement

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;

    while stmt.step()? {
        let record = stmt.record()?;

        let id: i64 = record.get(0)?;
        let name: String = record.get(1)?;
        let email: String = record.get(2)?;

        println!("{id}: {name} <{email}>");

        // record is independent of stmt
    }

    Ok(())
}
```

### Accessing record columns by name

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;

    while stmt.step()? {
        let record = stmt.record()?;

        let idx = record.find("name")?;
        let name: String = record.get(idx)?;

        let idx = record.find("email")?;
        let email: String = record.get(idx)?;

        println!("{name}: {email}");
    }

    Ok(())
}
```

### Storing records for later use

Since records own their data, they can be stored in a collection:

```rust
use wlite::{Database, Record};

fn collect_users(db: &Database) -> wlite::Result<Vec<Record>> {
    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;
    let mut records = Vec::new();

    while stmt.step()? {
        records.push(stmt.record()?);
    }

    Ok(records)
}

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    let users = collect_users(&db)?;

    for user in &users {
        let name: String = user.get(1)?;
        let email: String = user.get(2)?;
        println!("{name}: {email}");
    }

    Ok(())
}
```

## Transactions

Transactions ensure that a group of operations either all succeed or all fail. They maintain data consistency and improve performance for batch operations.

### Beginning and committing a transaction

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let tx = db.begin()?;

    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")?;
    db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")?;

    tx.commit()?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Users after commit: {count}");

    Ok(())
}
```

### Rolling back a transaction

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let tx = db.begin()?;

    db.execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')")?;

    // Decide to undo
    tx.rollback()?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Users after rollback: {count}");

    Ok(())
}
```

### Automatic rollback on drop

If a `Transaction` is dropped without being committed, it is automatically rolled back:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    {
        let _tx = db.begin()?;
        db.execute("INSERT INTO users (name, email) VALUES ('Temp', 'temp@example.com')")?;
        // _tx is dropped here without commit, so the INSERT is rolled back
    }

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Users: {count}");

    Ok(())
}
```

### Transaction with conditional commit

```rust
use wlite::{Database, Error};

fn transfer_funds(db: &Database, from: i64, to: i64, amount: i64) -> wlite::Result<()> {
    let tx = db.begin()?;

    db.execute(&format!("UPDATE accounts SET balance = balance - {amount} WHERE id = {from}"))?;
    db.execute(&format!("UPDATE accounts SET balance = balance + {amount} WHERE id = {to}"))?;

    let balance: i64 = db.query_scalar(&format!("SELECT balance FROM accounts WHERE id = {from}"))?;

    if balance < 0 {
        tx.rollback()?;
        return Err(Error::Range);
    }

    tx.commit()?;
    Ok(())
}
```

## Savepoints

Savepoints allow nested transaction control. You can roll back to a savepoint without affecting the entire transaction.

### Creating and releasing savepoints

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let tx = db.begin()?;

    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")?;

    tx.savepoint("sp1")?;

    db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")?;

    // Undo only Bob's insert
    tx.rollback_to("sp1")?;
    tx.release("sp1")?;

    // Alice is still inserted, Bob is not
    tx.commit()?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Users: {count}");

    Ok(())
}
```

### Multiple savepoints

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let tx = db.begin()?;

    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")?;

    tx.savepoint("batch1")?;

    db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")?;
    db.execute("INSERT INTO users (name, email) VALUES ('Charlie', 'charlie@example.com')")?;

    tx.savepoint("batch2")?;

    db.execute("INSERT INTO users (name, email) VALUES ('Diana', 'diana@example.com')")?;

    // Undo Diana
    tx.rollback_to("batch2")?;
    tx.release("batch2")?;

    // Undo Bob and Charlie
    tx.rollback_to("batch1")?;
    tx.release("batch1")?;

    // Only Alice remains
    tx.commit()?;

    Ok(())
}
```

## Iterator pattern

The `Statement` type implements an iterator-like interface via `step`. Combine this with Rust idioms for clean iteration.

### While loop

The most common pattern:

```rust
use wlite::Database;

fn list_users(db: &Database) -> wlite::Result<()> {
    let mut stmt = db.prepare("SELECT id, name FROM users ORDER BY name")?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let name: String = stmt.column_text(1)?;
        println!("{id}: {name}");
    }

    Ok(())
}
```

### Collecting results into a vector

```rust
use wlite::Database;

#[derive(Debug)]
struct User {
    id: i64,
    name: String,
    email: String,
}

fn get_all_users(db: &Database) -> wlite::Result<Vec<User>> {
    let mut stmt = db.prepare("SELECT id, name, email FROM users")?;
    let mut users = Vec::new();

    while stmt.step()? {
        users.push(User {
            id: stmt.column_int64(0)?,
            name: stmt.column_text(1)?,
            email: stmt.column_text(2)?,
        });
    }

    Ok(users)
}

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    let users = get_all_users(&db)?;

    for user in &users {
        println!("{user:?}");
    }

    Ok(())
}
```

### Using query for simple cases

For simple queries, `Database::query` returns an iterator over rows:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let rows = db.query("SELECT name FROM users")?;
    for row in rows {
        let name: String = row.get(0)?;
        println!("{name}");
    }

    Ok(())
}
```

### Using query_scalar for single values

For queries that return a single value:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    println!("Total users: {count}");

    let name: String = db.query_scalar("SELECT name FROM users WHERE id = 1")?;
    println!("First user: {name}");

    Ok(())
}
```

## Batch operations

### Batch insert with prepared statement

```rust
use wlite::Database;

fn batch_insert(db: &Database) -> wlite::Result<()> {
    let users = vec![
        ("Alice", "alice@example.com"),
        ("Bob", "bob@example.com"),
        ("Charlie", "charlie@example.com"),
        ("Diana", "diana@example.com"),
        ("Eve", "eve@example.com"),
    ];

    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

    for (name, email) in &users {
        stmt.bind(1, *name)?;
        stmt.bind(2, *email)?;
        stmt.step()?;
        stmt.reset()?;
    }

    Ok(())
}
```

### Batch insert with transaction

Wrapping batch operations in a transaction improves performance and ensures atomicity:

```rust
use wlite::Database;

fn batch_insert_with_transaction(db: &Database) -> wlite::Result<()> {
    let tx = db.begin()?;

    let mut stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")?;

    let users = vec![
        ("Alice", "alice@example.com"),
        ("Bob", "bob@example.com"),
        ("Charlie", "charlie@example.com"),
    ];

    for (name, email) in &users {
        stmt.bind(1, *name)?;
        stmt.bind(2, *email)?;
        stmt.step()?;
        stmt.reset()?;
    }

    tx.commit()?;
    Ok(())
}
```

## Query patterns

### Parameterized search

```rust
use wlite::Database;

fn search_users(db: &Database, query: &str) -> wlite::Result<()> {
    let pattern = format!("%{query}%");
    let mut stmt = db.prepare(
        "SELECT id, name, email FROM users WHERE name LIKE ? OR email LIKE ?"
    )?;
    stmt.bind(1, &pattern)?;
    stmt.bind(2, &pattern)?;

    while stmt.step()? {
        let id: i64 = stmt.column_int64(0)?;
        let name: String = stmt.column_text(1)?;
        let email: String = stmt.column_text(2)?;
        println!("{id}: {name} <{email}>");
    }

    Ok(())
}
```

### Aggregation queries

```rust
use wlite::Database;

fn get_stats(db: &Database) -> wlite::Result<()> {
    let total: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
    let with_email: i64 = db.query_scalar(
        "SELECT COUNT(*) FROM users WHERE email IS NOT NULL"
    )?;
    let without_email = total - with_email;

    println!("Total: {total}");
    println!("With email: {with_email}");
    println!("Without email: {without_email}");

    Ok(())
}
```

### Join queries

```rust
use wlite::Database;

fn list_orders_with_users(db: &Database) -> wlite::Result<()> {
    let mut stmt = db.prepare(
        "SELECT orders.id, users.name, orders.product, orders.quantity \
         FROM orders JOIN users ON orders.user_id = users.id \
         ORDER BY orders.id"
    )?;

    while stmt.step()? {
        let order_id: i64 = stmt.column_int64(0)?;
        let user_name: String = stmt.column_text(1)?;
        let product: String = stmt.column_text(2)?;
        let quantity: i64 = stmt.column_int64(3)?;
        println!("Order {order_id}: {user_name} ordered {quantity}x {product}");
    }

    Ok(())
}
```
