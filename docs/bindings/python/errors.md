---
title: Error Handling and Safety
description: Error class, error codes, exception patterns, cleanup, thread safety, and memory management.
---

# Error Handling and Safety

The wlite Python binding uses exceptions to communicate failures. Every operation that interacts with the C library can raise `wlite.Error` when something goes wrong. This document covers the error class, all twelve error codes, exception handling patterns, cleanup strategies, thread safety, and memory management.

## The Error class

`wlite.Error` is the single exception type raised by the binding. It inherits from `Exception` and carries two attributes:

- `code` (int): A numeric error code corresponding to a `WLITE_*` constant.
- `message` (str): A human-readable description of the failure.

```python
import wlite

try:
    db = wlite.Database.open("missing.db")
except wlite.Error as e:
    print(f"Error code: {e.code}")
    print(f"Error message: {e.message}")
```

The error code lets you programmatically distinguish between different failure modes. The message is suitable for logging or display to users.

### String representation

`str(e)` produces a formatted string combining the code and message:

```python
try:
    db = wlite.Database.open("missing.db")
except wlite.Error as e:
    print(str(e))
    # Output: [5] I/O error: unable to open database file
```

### Re-raising errors

You can catch `wlite.Error`, inspect it, and re-raise it:

```python
import wlite

def open_database(path):
    try:
        return wlite.Database.open(path)
    except wlite.Error as e:
        if e.code == wlite.WLITE_NOT_FOUND:
            print(f"Database not found at {path}, creating new one")
            return wlite.Database.create(path)
        raise
```

## Error codes

The binding defines twelve error codes as module-level constants. Each corresponds to a specific failure mode in the underlying C library.

### WLITE_OK (0)

Success. This code is never raised as an exception because operations that succeed do not raise errors. You may encounter it when inspecting return values from lower-level calls.

### WLITE_ERROR (1)

A general or unspecified error. This is a catch-all for failures that do not have a more specific code. Check the message for details.

```python
try:
    # Some operation that fails generically
    pass
except wlite.Error as e:
    if e.code == wlite.WLITE_ERROR:
        print(f"General error: {e.message}")
```

### WLITE_INVALID_ARGUMENT (2)

A null pointer or invalid parameter was passed to the C library. This typically indicates a programming error such as passing `None` where a string is expected or using an out-of-range index.

```python
try:
    stmt = db.prepare("SELECT * FROM users")
    # Binding to index 0 is invalid (indices start at 1)
    stmt.bind(0, "value")
except wlite.Error as e:
    if e.code == wlite.WLITE_INVALID_ARGUMENT:
        print(f"Invalid argument: {e.message}")
    stmt.finalize()
```

### WLITE_OUT_OF_MEMORY (3)

Memory allocation failed. This is rare on modern systems but can occur with very large result sets or under memory pressure.

```python
try:
    rows = db.query("SELECT * FROM huge_table")
except wlite.Error as e:
    if e.code == wlite.WLITE_OUT_OF_MEMORY:
        print("Out of memory processing query results")
```

### WLITE_IO_ERROR (4)

An I/O error occurred while reading or writing the database file. This can happen if the disk is full, the file system is corrupted, or the file was removed while the database was open.

```python
try:
    db.execute("INSERT INTO logs (message) VALUES (?)", ("test",))
except wlite.Error as e:
    if e.code == wlite.WLITE_IO_ERROR:
        print(f"I/O error: {e.message}")
        print("Check disk space and file system health")
```

### WLITE_PARSE_ERROR (5)

A `.wlite` schema file could not be parsed. The message contains details about the syntax error.

```python
try:
    model = wlite.Model.load("broken.wlite")
except wlite.Error as e:
    if e.code == wlite.WLITE_PARSE_ERROR:
        print(f"Schema parse error: {e.message}")
```

Common causes include missing closing braces, duplicate field names, or invalid type names.

### WLITE_MODEL_ERROR (6)

A schema model is internally inconsistent or conflicts with the existing database schema. This can happen when a migration would require data loss or when the model references tables or columns that cannot be reconciled.

```python
try:
    db.migrate(model)
except wlite.Error as e:
    if e.code == wlite.WLITE_MODEL_ERROR:
        print(f"Schema conflict: {e.message}")
```

### WLITE_SQLITE_ERROR (7)

The underlying SQLite operation returned an error. This is a wrapper around SQLite error codes and can indicate many different problems such as syntax errors, missing tables, or constraint violations.

```python
try:
    db.execute("SELECT * FROM nonexistent_table")
except wlite.Error as e:
    if e.code == wlite.WLITE_SQLITE_ERROR:
        print(f"SQLite error: {e.message}")
```

### WLITE_CONSTRAINT_ERROR (8)

A database constraint was violated. This includes UNIQUE constraints, NOT NULL constraints, CHECK constraints, and foreign key constraints.

```python
try:
    db.execute("INSERT INTO users (email) VALUES (?)", ("duplicate@example.com",))
except wlite.Error as e:
    if e.code == wlite.WLITE_CONSTRAINT_ERROR:
        print(f"Constraint violation: {e.message}")
```

You can use `INSERT OR IGNORE` or `INSERT OR REPLACE` to handle expected constraint violations gracefully.

### WLITE_NOT_FOUND (9)

A requested resource does not exist. This can be a missing database file, a missing table, or a missing row when using methods that expect one.

```python
try:
    db = wlite.Database.open("nonexistent.db")
except wlite.Error as e:
    if e.code == wlite.WLITE_NOT_FOUND:
        print("Database file does not exist")
```

### WLITE_BUSY (10)

The database is locked by another process or connection. This typically occurs when another writer is active. SQLite serializes writes, so concurrent writers must wait or retry.

```python
import time

def open_with_retry(path, retries=3, delay=1.0):
    for attempt in range(retries):
        try:
            return wlite.Database.open(path)
        except wlite.Error as e:
            if e.code == wlite.WLITE_BUSY and attempt < retries - 1:
                time.sleep(delay)
                continue
            raise
```

### WLITE_TRANSACTION_ERROR (11)

A transaction operation failed. This can happen if you attempt to commit a transaction that is already committed, roll back a transaction that was already rolled back, or create a savepoint with a duplicate name.

```python
try:
    tx = db.begin()
    tx.commit()
    tx.commit()  # second commit is an error
except wlite.Error as e:
    if e.code == wlite.WLITE_TRANSACTION_ERROR:
        print(f"Transaction error: {e.message}")
    tx.free()
```

## Complete error code table

| Code | Constant | Meaning |
|---|---|---|
| 0 | `WLITE_OK` | Success |
| 1 | `WLITE_ERROR` | General error |
| 2 | `WLITE_INVALID_ARGUMENT` | Null pointer or invalid parameter |
| 3 | `WLITE_OUT_OF_MEMORY` | Allocation failed |
| 4 | `WLITE_IO_ERROR` | I/O error |
| 5 | `WLITE_PARSE_ERROR` | Schema parse error |
| 6 | `WLITE_MODEL_ERROR` | Schema model error |
| 7 | `WLITE_SQLITE_ERROR` | SQLite error |
| 8 | `WLITE_CONSTRAINT_ERROR` | Constraint violation |
| 9 | `WLITE_NOT_FOUND` | Resource not found |
| 10 | `WLITE_BUSY` | Database locked |
| 11 | `WLITE_TRANSACTION_ERROR` | Transaction error |

## Exception handling patterns

### Catching specific errors

Use the error code to handle different failures differently:

```python
import wlite

def safe_insert(db, name, email):
    try:
        stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
        try:
            stmt.bind(1, name)
            stmt.bind(2, email)
            stmt.step()
        finally:
            stmt.finalize()
    except wlite.Error as e:
        if e.code == wlite.WLITE_CONSTRAINT_ERROR:
            print(f"User {email} already exists")
            return False
        elif e.code == wlite.WLITE_BUSY:
            print("Database is locked, try again later")
            return False
        else:
            raise
    return True
```

### Catching all wlite errors

```python
import wlite

try:
    db = wlite.Database.open("app.db")
    model = wlite.Model.load("app.wlite")
    db.migrate(model)
    rows = db.query("SELECT * FROM users")
except wlite.Error as e:
    print(f"Wlite error [{e.code}]: {e.message}")
finally:
    if "db" in dir() and db:
        db.close()
```

### Distinguishing wlite errors from other exceptions

```python
import wlite

try:
    db = wlite.Database.open("app.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ("Alice",))
except wlite.Error as e:
    print(f"Database error: {e}")
except ValueError as e:
    print(f"Value error: {e}")
except Exception as e:
    print(f"Unexpected error: {type(e).__name__}: {e}")
finally:
    if "db" in dir() and db:
        db.close()
```

### Retry with exponential backoff

For `WLITE_BUSY` errors, implement a retry strategy:

```python
import wlite
import time


def open_with_backoff(path, max_retries=5, base_delay=0.1):
    delay = base_delay
    for attempt in range(max_retries):
        try:
            return wlite.Database.open(path)
        except wlite.Error as e:
            if e.code == wlite.WLITE_BUSY:
                if attempt < max_retries - 1:
                    time.sleep(delay)
                    delay *= 2
                    continue
            raise
```

## Cleanup patterns

### The with statement

`Database` and `Transaction` support the context manager protocol. Use `with` to ensure cleanup:

```python
import wlite

with wlite.Database.open("app.db") as db:
    model = wlite.Model.load("app.wlite")
    db.migrate(model)

    with db.begin() as tx:
        db.execute("INSERT INTO users (name) VALUES ('Alice')")
        tx.commit()
```

When the `with` block exits, `close()` or `free()` is called automatically.

### try/finally for statements

Statements do not support the context manager protocol. Use `try`/`finally`:

```python
import wlite

db = wlite.Database.open("app.db")
stmt = db.prepare("INSERT INTO users (name) VALUES (?)")
try:
    stmt.bind(1, "Alice")
    stmt.step()
finally:
    stmt.finalize()
db.close()
```

### Nested cleanup

When you have multiple resources, nest the cleanup:

```python
import wlite

db = wlite.Database.open("app.db")
try:
    tx = db.begin()
    try:
        stmt = db.prepare("INSERT INTO users (name) VALUES (?)")
        try:
            stmt.bind(1, "Alice")
            stmt.step()
        finally:
            stmt.finalize()
        tx.commit()
    except Exception:
        tx.rollback()
        raise
    finally:
        tx.free()
except Exception:
    raise
finally:
    db.close()
```

This looks verbose but guarantees that each resource is freed in the correct order regardless of exceptions.

### Cleanup helper function

Reduce boilerplate with a helper:

```python
import wlite
from contextlib import contextmanager


@contextmanager
def open_statement(db, sql):
    stmt = db.prepare(sql)
    try:
        yield stmt
    finally:
        stmt.finalize()


def insert_user(db, name):
    with open_statement(db, "INSERT INTO users (name) VALUES (?)") as stmt:
        stmt.bind(1, name)
        stmt.step()
```

### Ensuring transaction rollback on failure

The most common cleanup pattern is transaction safety:

```python
import wlite

def transfer_funds(db, from_id, to_id, amount):
    tx = db.begin()
    try:
        db.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?", (amount, from_id))
        db.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?", (amount, to_id))

        balance = db.query_scalar("SELECT balance FROM accounts WHERE id = ?", (from_id,))
        if balance < 0:
            raise ValueError("Insufficient funds")

        tx.commit()
    except Exception:
        tx.rollback()
        raise
    finally:
        tx.free()
```

The `finally` block guarantees `tx.free()` runs. The `except` block catches any exception and rolls back before re-raising.

## Thread safety

### The threading model

The wlite binding does not add its own thread synchronization. The underlying SQLite library supports multi-threading but with constraints:

- Multiple threads can read from the same database simultaneously.
- Only one thread can write at a time.
- A single `Database` object should not be shared between threads without synchronization.

### Per-thread connections

The simplest safe pattern is one connection per thread:

```python
import threading
import wlite

local = threading.local()


def get_db():
    if not hasattr(local, "db"):
        local.db = wlite.Database.open("app.db")
    return local.db


def thread_worker():
    db = get_db()
    rows = db.query("SELECT * FROM users")
    for row in rows:
        print(row["name"])
```

Each thread gets its own `Database` object via thread-local storage.

### Shared connection with a lock

If you must share a connection, protect it with a lock:

```python
import threading
import wlite

db = wlite.Database.open("app.db")
db_lock = threading.Lock()


def thread_worker():
    with db_lock:
        rows = db.query("SELECT * FROM users")
    for row in rows:
        print(row["name"])
```

This serializes all access to the database. It is safe but limits concurrency.

### WAL mode for concurrency

Using WAL journal mode allows concurrent readers while a single writer is active:

```python
import wlite

db = wlite.Database.open("app.db", journal_mode="wal")
```

With WAL mode, readers do not block writers and writers do not block readers. Only concurrent writers are serialized.

### Avoiding common thread pitfalls

Do not do this:

```python
import threading
import wlite

db = wlite.Database.open("app.db")


def thread_worker():
    # UNSAFE: sharing db across threads without synchronization
    rows = db.query("SELECT * FROM users")
    for row in rows:
        print(row["name"])


threads = [threading.Thread(target=thread_worker) for _ in range(5)]
for t in threads:
    t.start()
for t in threads:
    t.join()
```

This can corrupt data or cause crashes. Always use per-thread connections or a lock.

## Memory management

### Reference counting and finalizers

The binding wraps C pointers in Python objects. When a Python object goes out of scope the garbage collector frees the C memory via a finalizer.

For most applications this works without intervention. However, relying on garbage collection for cleanup can cause problems:

- Finalizers run at unpredictable times.
- If many resources accumulate before collection you may run out of file descriptors or hit memory limits.
- In CPython, finalizers run when the reference count drops to zero. In other implementations (PyPy, GraalPy) they may run at the GC's discretion.

### Explicit cleanup

Always prefer explicit cleanup:

```python
import wlite

db = wlite.Database.open("app.db")
try:
    stmt = db.prepare("SELECT * FROM users")
    try:
        while stmt.step():
            print(stmt.column_text(0))
    finally:
        stmt.finalize()
finally:
    db.close()
```

### Avoiding reference cycles

Reference cycles can delay garbage collection. Avoid holding references to database objects in long-lived structures:

```python
import wlite

class DataAccess:
    def __init__(self, db_path):
        self.db = wlite.Database.open(db_path)

    def close(self):
        if self.db:
            self.db.close()
            self.db = None

    def __del__(self):
        self.close()
```

The `__del__` method ensures cleanup if the user forgets to call `close()`, but do not rely on it. Call `close()` explicitly.

### Bulk operations and memory

When inserting many rows, the binding processes them one at a time through the C API. Memory usage stays constant regardless of batch size. However, if you build a large list of result rows in Python, that list consumes memory:

```python
# Memory-efficient: iterate without storing
stmt = db.prepare("SELECT * FROM large_table")
try:
    while stmt.step():
        process_row(stmt.record())
finally:
    stmt.finalize()

# Memory-inefficient: stores all rows in memory
rows = db.query("SELECT * FROM large_table")
for row in rows:
    process_row(row)
```

Use the prepared statement approach for large result sets.

### Blob memory

BLOB values are copied from the C library into Python `bytes` objects. Large BLOBs can consume significant memory:

```python
stmt = db.prepare("SELECT data FROM blobs WHERE id = ?")
try:
    stmt.bind(1, blob_id)
    if stmt.step():
        data = stmt.column_blob(0)
        # data is a Python bytes object, safe to use
finally:
    stmt.finalize()
```

The C library releases its copy after `column_blob` returns, so there is only one copy in memory at a time.

## Summary of safety practices

- Always catch `wlite.Error` and inspect `e.code` for the specific failure.
- Use `try`/`finally` or `with` statements to guarantee cleanup of statements and transactions.
- Never share a single `Database` object between threads without a lock.
- Prefer per-thread connections for concurrent access.
- Use WAL journal mode when concurrent readers and a single writer are needed.
- Call `close()`, `finalize()`, and `free()` explicitly rather than relying on garbage collection.
- For large result sets, iterate with `step()` instead of loading all rows into memory.
- Use retry with backoff for `WLITE_BUSY` errors in concurrent environments.
