---
title: Queries and Transactions
description: Prepared statements, parameter binding, column access, records, transactions, and savepoints.
---

# Queries and Transactions

The wlite Python binding gives you fine-grained control over how queries are executed. You can use simple one-line calls for convenience, or drop down to prepared statements for performance and safety. Transactions and savepoints let you group operations atomically.

This document covers prepared statements, parameter binding, stepping through results, column access, record dictionaries, the transaction context manager pattern, savepoints, and parameterized queries.

## Prepared statements

A prepared statement compiles SQL once and lets you execute it multiple times with different parameters. This is faster than re-parsing the SQL on every call and protects against injection.

### Creating a statement

```python
import wlite

db = wlite.Database.open("app.db")
stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
```

The `?` characters are parameter placeholders. Parameters are numbered starting from 1.

### Binding parameters

Use `bind` to assign values to placeholders:

```python
stmt.bind(1, "Alice")
stmt.bind(2, "alice@example.com")
```

Parameters are typed. Passing a string, integer, or float binds to the appropriate SQLite affinity. Pass `None` for NULL:

```python
stmt.bind(1, 42)
stmt.bind(2, 3.14)
stmt.bind(3, None)
stmt.bind(4, b"\x00\x01\x02")  # BLOB
```

### Stepping through results

For INSERT, UPDATE, and DELETE statements, call `step()` once to execute:

```python
stmt.step()
```

For SELECT statements, call `step()` in a loop. It returns `True` if a row is available and `False` when there are no more rows:

```python
while stmt.step():
    name = stmt.column_text(0)
    email = stmt.column_text(1)
    print(f"{name} <{email}>")
```

### Resetting a statement

After stepping, call `reset()` to rewind the statement so you can bind new parameters and execute again:

```python
stmt.bind(1, "Bob")
stmt.bind(2, "bob@example.com")
stmt.step()
stmt.reset()

stmt.bind(1, "Charlie")
stmt.bind(2, "charlie@example.com")
stmt.step()
```

Reset preserves the compiled SQL and any bound parameters that you do not overwrite.

### Finalizing a statement

When the statement is no longer needed, call `finalize()` to release resources:

```python
stmt.finalize()
```

Always finalize statements you are done with. The binding uses finalizers as a safety net, but explicit cleanup is preferred.

### Complete statement lifecycle

```python
import wlite

db = wlite.Database.open("app.db")

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

db.close()
```

The `try`/`finally` ensures the statement is finalized even if an exception occurs during binding or stepping.

## Column access

When a SELECT statement has a row available, you read columns by index. Indices are zero-based.

### Column count and names

```python
stmt = db.prepare("SELECT id, name, email FROM users")
col_count = stmt.column_count()
for i in range(col_count):
    print(f"Column {i}: {stmt.column_name(i)}")
```

### Column types

The `column_type` method returns a type constant:

```python
import wlite

while stmt.step():
    for i in range(stmt.column_count()):
        ctype = stmt.column_type(i)
        if ctype == wlite.COLUMN_INTEGER:
            value = stmt.column_int64(i)
        elif ctype == wlite.COLUMN_FLOAT:
            value = stmt.column_double(i)
        elif ctype == wlite.COLUMN_TEXT:
            value = stmt.column_text(i)
        elif ctype == wlite.COLUMN_BLOB:
            value = stmt.column_blob(i)
        else:
            value = None
        print(f"  {stmt.column_name(i)}: {value}")
```

### Reading specific column types

Use the typed accessors for efficiency:

| Method | Returns | Use when |
|---|---|---|
| `column_int64(i)` | `int` | Column is INTEGER |
| `column_double(i)` | `float` | Column is FLOAT or REAL |
| `column_text(i)` | `str` | Column is TEXT |
| `column_blob(i)` | `bytes` | Column is BLOB |

### Reading a single row

For queries that return exactly one row, `step()` returns `True` once and then `False`:

```python
stmt = db.prepare("SELECT COUNT(*) FROM users")
try:
    if stmt.step():
        count = stmt.column_int64(0)
        print(f"Total users: {count}")
finally:
    stmt.finalize()
```

## Records as dictionaries

The `record()` method converts the current row into a dictionary keyed by column name:

```python
stmt = db.prepare("SELECT id, name, email FROM users")
try:
    while stmt.step():
        row = stmt.record()
        print(f"{row['id']}: {row['name']} <{row['email']}>")
finally:
    stmt.finalize()
```

This is convenient when you want to pass rows around as plain data structures without worrying about column indices.

### Building a list of records

```python
stmt = db.prepare("SELECT id, name, email FROM users ORDER BY name")
try:
    results = []
    while stmt.step():
        results.append(stmt.record())
finally:
    stmt.finalize()

for user in results:
    print(user["name"])
```

### Accessing records with missing values

Columns with NULL values are represented as `None` in the dictionary:

```python
stmt = db.prepare("SELECT id, name, bio FROM users")
try:
    while stmt.step():
        row = stmt.record()
        bio = row["bio"] or "No bio provided"
        print(f"{row['name']}: {bio}")
finally:
    stmt.finalize()
```

## Parameterized queries

The binding supports positional parameters with `?` and named parameters with `:name`, `$name`, or `@name`.

### Positional parameters

```python
db.execute(
    "INSERT INTO users (name, email, active) VALUES (?, ?, ?)",
    ("Alice", "alice@example.com", 1)
)
```

You can pass parameters as a tuple:

```python
rows = db.query("SELECT * FROM users WHERE active = ? AND name LIKE ?", (1, "%Ali%"))
```

### Named parameters

```python
db.execute(
    "INSERT INTO users (name, email) VALUES (:name, :email)",
    {"name": "Alice", "email": "alice@example.com"}
)
```

Named parameters are useful when a query has many placeholders and you want to avoid counting question marks.

### Mixed parameter styles

You can use positional and named parameters in the same query, though this is unusual:

```python
rows = db.query(
    "SELECT * FROM users WHERE active = :active AND id > ?",
    {"active": 1, 10}
)
```

### Binding NULL explicitly

```python
stmt = db.prepare("INSERT INTO users (name, bio) VALUES (?, ?)")
stmt.bind(1, "Alice")
stmt.bind(2, None)
stmt.step()
stmt.finalize()
```

### Binding binary data

BLOB values are bound as `bytes`:

```python
stmt = db.prepare("INSERT INTO blobs (data) VALUES (?)")
stmt.bind(1, b"\x89PNG\r\n\x1a\n")
stmt.step()
stmt.finalize()
```

## High-level query methods

For simple cases, the `Database` type provides convenience methods that skip the prepare/bind/step cycle.

### execute

Runs a statement that does not return rows:

```python
db.execute("DELETE FROM users WHERE active = 0")
db.execute("UPDATE users SET name = ? WHERE id = ?", ("Alice Smith", 1))
```

### query

Returns a list of dictionaries, one per row:

```python
rows = db.query("SELECT id, name FROM users ORDER BY name")
for row in rows:
    print(row["id"], row["name"])
```

### query_scalar

Returns a single scalar value from the first column of the first row:

```python
count = db.query_scalar("SELECT COUNT(*) FROM users")
print(f"Total: {count}")

name = db.query_scalar("SELECT name FROM users WHERE id = ?", (1,))
print(f"User: {name}")
```

## Transactions

Transactions group multiple operations so that they either all succeed or all fail. The `Database.begin()` method starts a transaction and returns a `Transaction` object.

### Basic transaction

```python
import wlite

db = wlite.Database.open("app.db")

tx = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")
    db.execute("INSERT INTO users (name) VALUES ('Bob')")

    count = db.query_scalar("SELECT COUNT(*) FROM users")
    if count > 100:
        raise ValueError("Too many users")

    tx.commit()
except Exception:
    tx.rollback()
    raise
finally:
    tx.free()
```

The `try`/`except`/`finally` pattern ensures that on failure the transaction is rolled back and resources are freed.

### Transaction with context manager

The `Transaction` type supports the context manager protocol:

```python
import wlite

db = wlite.Database.open("app.db")

with db.begin() as tx:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")
    db.execute("INSERT INTO users (name) VALUES ('Bob')")

    count = db.query_scalar("SELECT COUNT(*) FROM users")
    if count > 100:
        raise ValueError("Too many users")

    tx.commit()
```

When the `with` block exits, if `commit` has not been called the transaction is rolled back automatically. If an exception is raised inside the block the transaction is rolled back before the exception propagates.

Note that you still need to call `tx.commit()` explicitly when you want to persist changes. The context manager handles rollback on failure, not automatic commit.

### Nested transaction attempt

SQLite does not support true nested transactions. The wlite binding models nested transactions as savepoints. If you call `begin()` inside an active transaction the new transaction object is a savepoint:

```python
import wlite

db = wlite.Database.open("app.db")

tx_outer = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")

    tx_inner = db.begin()
    try:
        db.execute("INSERT INTO users (name) VALUES ('Bob')")
        tx_inner.commit()
    except Exception:
        tx_inner.rollback()
        raise
    finally:
        tx_inner.free()

    tx_outer.commit()
except Exception:
    tx_outer.rollback()
    raise
finally:
    tx_outer.free()
```

In practice, prefer explicit savepoints for clarity.

## Savepoints

Savepoints let you roll back part of a transaction without losing earlier work.

### Creating a savepoint

```python
import wlite

db = wlite.Database.open("app.db")

tx = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")

    tx.savepoint("before_batch")

    db.execute("INSERT INTO users (name) VALUES ('Bob')")
    db.execute("INSERT INTO users (name) VALUES ('Charlie')")

    tx.commit()
except Exception:
    tx.rollback()
    raise
finally:
    tx.free()
```

### Rolling back to a savepoint

If something goes wrong after creating a savepoint you can roll back to it without undoing the entire transaction:

```python
import wlite

db = wlite.Database.open("app.db")

tx = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")

    tx.savepoint("sp1")

    try:
        db.execute("INSERT INTO users (name) VALUES ('Bob')")
        db.execute("INSERT INTO invalid_table (col) VALUES (1)")
    except wlite.Error:
        tx.rollback_to("sp1")

    db.execute("INSERT INTO users (name) VALUES ('Charlie')")
    tx.commit()
except Exception:
    tx.rollback()
    raise
finally:
    tx.free()
```

After rolling back to `sp1`, Alice is still inserted. Bob and the failed statement are undone. Charlie is then inserted and the whole transaction commits.

### Multiple savepoints

You can create multiple savepoints and roll back to any of them:

```python
import wlite

db = wlite.Database.open("app.db")

tx = db.begin()
try:
    tx.savepoint("sp1")
    db.execute("INSERT INTO users (name) VALUES ('Alice')")

    tx.savepoint("sp2")
    db.execute("INSERT INTO users (name) VALUES ('Bob')")

    tx.savepoint("sp3")
    db.execute("INSERT INTO users (name) VALUES ('Charlie')")

    tx.rollback_to("sp2")

    # Charlie is gone, Bob and Alice remain
    db.execute("INSERT INTO users (name) VALUES ('Diana')")

    tx.commit()
except Exception:
    tx.rollback()
    raise
finally:
    tx.free()
```

### Releasing savepoints

If you no longer need a savepoint you can release it. This merges it into the parent transaction:

```python
tx.savepoint("sp1")
db.execute("INSERT INTO users (name) VALUES ('Alice')")

tx.release("sp1")

# Rolling back now would undo Alice too
```

Once released, a savepoint cannot be rolled back to.

## Batch operations

### Batch insert with prepared statement

```python
import wlite

def batch_insert(db, table, columns, rows):
    placeholders = ", ".join(["?"] * len(columns))
    sql = f"INSERT INTO {table} ({', '.join(columns)}) VALUES ({placeholders})"
    stmt = db.prepare(sql)
    try:
        for row in rows:
            for i, value in enumerate(row, 1):
                stmt.bind(i, value)
            stmt.step()
            stmt.reset()
    finally:
        stmt.finalize()


db = wlite.Database.open("app.db")
users = [
    ("Alice", "alice@example.com"),
    ("Bob", "bob@example.com"),
    ("Charlie", "charlie@example.com"),
]
batch_insert(db, "users", ["name", "email"], users)
db.close()
```

### Batch insert inside a transaction

```python
import wlite

def batch_insert_transactional(db, users):
    with db.begin() as tx:
        stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
        try:
            for name, email in users:
                stmt.bind(1, name)
                stmt.bind(2, email)
                stmt.step()
                stmt.reset()
        finally:
            stmt.finalize()
        tx.commit()
```

Wrapping in a transaction makes the batch atomic. If any insert fails the entire batch is rolled back.

### Batch update

```python
import wlite

def deactivate_users(db, user_ids):
    with db.begin() as tx:
        stmt = db.prepare("UPDATE users SET active = 0 WHERE id = ?")
        try:
            for uid in user_ids:
                stmt.bind(1, uid)
                stmt.step()
                stmt.reset()
        finally:
            stmt.finalize()
        tx.commit()
```

## Iterating over large result sets

For queries that return many rows, avoid building a list in memory. Instead iterate directly over the statement:

```python
import wlite

db = wlite.Database.open("app.db")
stmt = db.prepare("SELECT id, name, email FROM users")
try:
    while stmt.step():
        uid = stmt.column_int64(0)
        name = stmt.column_text(1)
        email = stmt.column_text(2)
        process_user(uid, name, email)
finally:
    stmt.finalize()
db.close()
```

This processes one row at a time and keeps memory usage constant regardless of result set size.

## Error handling in queries

All query methods raise `wlite.Error` on failure. Handle it at the appropriate level:

```python
import wlite

db = wlite.Database.open("app.db")

try:
    rows = db.query("SELECT * FROM nonexistent_table")
except wlite.Error as e:
    if e.code == wlite.WLITE_SQLITE_ERROR:
        print(f"Table does not exist: {e.message}")
    else:
        raise

db.close()
```

### Parameter validation

The binding validates parameter types before binding. Passing an unsupported type raises a Python `TypeError`, not a `wlite.Error`:

```python
stmt = db.prepare("INSERT INTO test (val) VALUES (?)")
try:
    stmt.bind(1, [1, 2, 3])  # lists are not supported
except TypeError as e:
    print(f"Bad parameter: {e}")
finally:
    stmt.finalize()
```

## Summary of query methods

| Method | Returns | Use case |
|---|---|---|
| `db.execute(sql, params)` | `None` | DDL and DML without results |
| `db.query(sql, params)` | `list[dict]` | SELECT returning multiple rows |
| `db.query_scalar(sql, params)` | scalar | SELECT returning one value |
| `db.prepare(sql)` | `Statement` | Manual parameter binding and iteration |
| `stmt.bind(i, value)` | `None` | Set parameter at index |
| `stmt.step()` | `bool` | Execute or advance to next row |
| `stmt.record()` | `dict` | Current row as dictionary |
| `stmt.column_int64(i)` | `int` | Read integer column |
| `stmt.column_double(i)` | `float` | Read float column |
| `stmt.column_text(i)` | `str` | Read text column |
| `stmt.column_blob(i)` | `bytes` | Read blob column |
| `stmt.reset()` | `None` | Rewind for re-execution |
| `stmt.finalize()` | `None` | Release statement resources |

This reference is a quick summary. See the preceding sections for full examples of each operation.
