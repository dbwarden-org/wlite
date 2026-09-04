---
title: Python Binding
description: Python ctypes binding for wlite.
---

# Python Binding

The `wlite` Python package provides access to libwlite via ctypes. Define your schema in `.wlite` and use it from Python.

The binding wraps the libwlite C library and exposes it through a Pythonic interface. It supports parameterized queries, prepared statements, and transactions while maintaining compatibility with the standard Python database API.

## Installation

```bash
pip install wlite
```

Requires libwlite to be installed on your system. Build and install it from the libwlite repository:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

### Development installation

For development or to contribute to the binding:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite/bindings/python
pip install -e .
```

### Verifying the installation

```python
import wlite
print(wlite.__version__)
```

## Basic usage

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

# Migrate the database schema
db.migrate(model)

# Query returns a list of dictionaries
rows = db.query("SELECT * FROM users")
for row in rows:
    print(f"{row['id']}: {row['name']} <{row['email']}>")

# Close the database when done
db.close()
```

### Working with context manager

```python
import wlite

with wlite.Database.open("app.db") as db:
    model = wlite.Model.load("app.wlite")
    db.migrate(model)

    rows = db.query("SELECT * FROM users")
    for row in rows:
        print(row["name"])
```

### Full workflow example

```python
import wlite

def setup_database():
    model = wlite.Model.load("app.wlite")
    db = wlite.Database.open("app.db")
    db.migrate(model)
    return db

def seed_data(db):
    users = [
        ("Alice", "alice@example.com"),
        ("Bob", "bob@example.com"),
        ("Charlie", "charlie@example.com"),
    ]

    stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    for name, email in users:
        stmt.bind(1, name)
        stmt.bind(2, email)
        stmt.step()
        stmt.reset()

    stmt.finalize()

def list_users(db):
    rows = db.query("SELECT id, name, email FROM users ORDER BY name")
    for row in rows:
        print(f"{row['id']}: {row['name']} <{row['email']}>")

if __name__ == "__main__":
    db = setup_database()
    seed_data(db)
    list_users(db)
    db.close()
```

## Types

| Python Type | C Equivalent | Description |
|-------------|--------------|-------------|
| `wlite.Database` | `wlite_db` | Open database connection |
| `wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `wlite.Statement` | `wlite_stmt` | Prepared SQL statement |
| `wlite.Transaction` | `wlite_tx` | Active transaction |
| `wlite.Row` | result row | Single row from a query |

The `Database` type manages the connection and provides methods for executing SQL, preparing statements, and querying data.

The `Model` type represents a parsed `.wlite` schema. It is immutable and can be loaded once, then used to migrate multiple databases.

The `Statement` type wraps a prepared SQL statement with parameter binding and column access methods.

## Database operations

```python
import wlite

db = wlite.Database.open("app.db")

# Execute DDL
db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")
db.execute("CREATE INDEX idx_test_name ON test(name)")

# Execute DML with parameters
db.execute("INSERT INTO test (name) VALUES (?)", ("hello",))
db.execute("UPDATE test SET name = ? WHERE id = ?", ("world", 1))
db.execute("DELETE FROM test WHERE id = ?", (1,))

# Query returns list of dicts
rows = db.query("SELECT * FROM test")
for row in rows:
    print(row["id"], row["name"])

# Query with WHERE clause
rows = db.query("SELECT * FROM test WHERE name = ?", ("hello",))

# Single value query
count = db.query_scalar("SELECT COUNT(*) FROM test")
print(f"Total rows: {count}")

db.close()
```

### Batch inserts

```python
import wlite

def batch_insert(db, users):
    stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    try:
        for name, email in users:
            stmt.bind(1, name)
            stmt.bind(2, email)
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
batch_insert(db, users)
db.close()
```

## Prepared statements

```python
import wlite

db = wlite.Database.open("app.db")

# Prepare an INSERT statement
stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)")

# Insert Alice
stmt.bind(1, "Alice")
stmt.bind(2, "alice@example.com")
stmt.step()
stmt.reset()

# Insert Bob
stmt.bind(1, "Bob")
stmt.bind(2, "bob@example.com")
stmt.step()
stmt.reset()

# Insert Charlie
stmt.bind(1, "Charlie")
stmt.bind(2, "charlie@example.com")
stmt.step()

# Finalize when done
stmt.finalize()

# Prepare a SELECT statement
query = db.prepare("SELECT * FROM users WHERE id = ?")
query.bind(1, 1)

while query.step():
    name = query.column_text(0)
    email = query.column_text(1)
    print(f"User: {name} <{email}>")

query.finalize()
db.close()
```

### Column access methods

| Method | Returns | Description |
|--------|---------|-------------|
| `stmt.column_count()` | `int` | Number of columns in result |
| `stmt.column_name(i)` | `str` | Name of column at index |
| `stmt.column_type(i)` | `int` | Data type constant |
| `stmt.column_int64(i)` | `int` | Integer value |
| `stmt.column_double(i)` | `float` | Floating point value |
| `stmt.column_text(i)` | `str` | Text value |

### Reading all columns

```python
import wlite

db = wlite.Database.open("app.db")

stmt = db.prepare("SELECT id, name, email FROM users")
col_count = stmt.column_count()

while stmt.step():
    row = {}
    for i in range(col_count):
        col_name = stmt.column_name(i)
        col_type = stmt.column_type(i)
        if col_type == wlite.COLUMN_INTEGER:
            row[col_name] = stmt.column_int64(i)
        elif col_type == wlite.COLUMN_FLOAT:
            row[col_name] = stmt.column_double(i)
        elif col_type == wlite.COLUMN_TEXT:
            row[col_name] = stmt.column_text(i)
        else:
            row[col_name] = None
    print(row)

stmt.finalize()
db.close()
```

## Transactions

```python
import wlite

db = wlite.Database.open("app.db")

# Using try/finally for transaction safety
tx = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")
    db.execute("INSERT INTO users (name) VALUES ('Bob')")

    # Verify before committing
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

### Transaction with savepoints

```python
import wlite

def batch_operation(db):
    tx = db.begin()
    try:
        db.execute("INSERT INTO users (name) VALUES ('Alice')")

        # Savepoint for partial rollback
        tx.savepoint("sp1")

        try:
            db.execute("INSERT INTO users (name) VALUES ('Bob')")
            # This might fail
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

## Error handling

All operations raise `wlite.Error` on failure:

```python
import wlite

try:
    db = wlite.Database.open("app.db")
except wlite.Error as e:
    print(f"Error opening database: {e}")

try:
    db = wlite.Database.open("nonexistent.db")
except wlite.Error as e:
    print(f"Error code: {e.code}")
    print(f"Error message: {e.message}")
```

### Error codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WLITE_OK` | Success |
| 1 | `WLITE_ERROR` | General error |
| 2 | `WLITE_NOT_FOUND` | File not found |
| 3 | `WLITE_MEMORY` | Allocation failed |
| 4 | `WLITE_IO` | I/O error |
| 5 | `WLITE_CORRUPT` | Corrupt data |
| 6 | `WLITE_RANGE` | Index out of range |

### Custom error handling

```python
import wlite

def safe_query(db, sql, params=None):
    try:
        if params:
            stmt = db.prepare(sql)
            for i, param in enumerate(params, 1):
                stmt.bind(i, param)
            results = []
            while stmt.step():
                row = {stmt.column_name(i): stmt.column_text(i)
                       for i in range(stmt.column_count())}
                results.append(row)
            stmt.finalize()
            return results
        else:
            return db.query(sql)
    except wlite.Error as e:
        print(f"Query failed: {e}")
        return []
```

## Schema inspection

```python
import wlite

model = wlite.Model.load("app.wlite")

print(f"Tables: {model.table_count()}")
for i in range(model.table_count()):
    table = model.table_at(i)
    print(f"\nTable: {table.name}")
    print(f"  Fields: {table.field_count()}")
    for j in range(table.field_count()):
        field = table.field_at(j)
        pk = " (PRIMARY KEY)" if field.is_primary_key else ""
        print(f"  - {field.name}: {field.type}{pk}")
```

## Complete example

Here is a complete, working program that demonstrates all major features:

```python
import wlite


class UserDatabase:
    def __init__(self, db_path, model_path):
        self.model = wlite.Model.load(model_path)
        self.db = wlite.Database.open(db_path)
        self.db.migrate(self.model)

    def close(self):
        if self.db:
            self.db.close()
            self.db = None

    def create_tables(self):
        self.db.execute("""
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                active INTEGER DEFAULT 1,
                created_at TEXT DEFAULT (datetime('now'))
            )
        """)

    def insert_user(self, name, email):
        stmt = self.db.prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        )
        try:
            stmt.bind(1, name)
            stmt.bind(2, email)
            stmt.step()
        finally:
            stmt.finalize()

    def insert_users(self, users):
        stmt = self.db.prepare(
            "INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)"
        )
        try:
            for name, email in users:
                stmt.bind(1, name)
                stmt.bind(2, email)
                stmt.step()
                stmt.reset()
        finally:
            stmt.finalize()

    def get_user(self, user_id):
        stmt = self.db.prepare("SELECT id, name, email FROM users WHERE id = ?")
        try:
            stmt.bind(1, user_id)
            if stmt.step():
                return {
                    "id": stmt.column_int64(0),
                    "name": stmt.column_text(1),
                    "email": stmt.column_text(2),
                }
            return None
        finally:
            stmt.finalize()

    def search_users(self, pattern):
        stmt = self.db.prepare(
            "SELECT id, name, email FROM users WHERE name LIKE ? OR email LIKE ?"
        )
        try:
            stmt.bind(1, f"%{pattern}%")
            stmt.bind(2, f"%{pattern}%")
            results = []
            while stmt.step():
                results.append({
                    "id": stmt.column_int64(0),
                    "name": stmt.column_text(1),
                    "email": stmt.column_text(2),
                })
            return results
        finally:
            stmt.finalize()

    def update_user(self, user_id, name=None, email=None):
        if name and email:
            stmt = self.db.prepare(
                "UPDATE users SET name = ?, email = ? WHERE id = ?"
            )
            stmt.bind(1, name)
            stmt.bind(2, email)
            stmt.bind(3, user_id)
        elif name:
            stmt = self.db.prepare("UPDATE users SET name = ? WHERE id = ?")
            stmt.bind(1, name)
            stmt.bind(2, user_id)
        elif email:
            stmt = self.db.prepare("UPDATE users SET email = ? WHERE id = ?")
            stmt.bind(1, email)
            stmt.bind(2, user_id)
        else:
            return

        try:
            stmt.step()
        finally:
            stmt.finalize()

    def delete_user(self, user_id):
        stmt = self.db.prepare("DELETE FROM users WHERE id = ?")
        try:
            stmt.bind(1, user_id)
            stmt.step()
        finally:
            stmt.finalize()

    def list_users(self):
        rows = self.db.query(
            "SELECT id, name, email, active FROM users ORDER BY name"
        )
        return [
            {
                "id": row["id"],
                "name": row["name"],
                "email": row["email"],
                "active": row["active"],
            }
            for row in rows
        ]

    def count_users(self):
        return self.db.query_scalar("SELECT COUNT(*) FROM users")

    def batch_transfer(self, transfers):
        tx = self.db.begin()
        try:
            for from_id, to_id, amount in transfers:
                self.db.execute(
                    "UPDATE accounts SET balance = balance - ? WHERE id = ?",
                    (amount, from_id),
                )
                self.db.execute(
                    "UPDATE accounts SET balance = balance + ? WHERE id = ?",
                    (amount, to_id),
                )

                balance = self.db.query_scalar(
                    "SELECT balance FROM accounts WHERE id = ?", (from_id,)
                )
                if balance < 0:
                    raise ValueError(f"Insufficient funds for account {from_id}")

            tx.commit()
        except Exception:
            tx.rollback()
            raise
        finally:
            tx.free()


def main():
    db = UserDatabase("app.db", "app.wlite")

    try:
        db.create_tables()

        users = [
            ("Alice", "alice@example.com"),
            ("Bob", "bob@example.com"),
            ("Charlie", "charlie@example.com"),
        ]
        db.insert_users(users)

        print(f"Total users: {db.count_users()}")
        print("\nAll users:")
        for user in db.list_users():
            print(f"  {user['id']}: {user['name']} <{user['email']}>")

        print("\nSearch results for 'Ali':")
        for user in db.search_users("Ali"):
            print(f"  {user['name']}")

        alice = db.get_user(1)
        if alice:
            print(f"\nGot user: {alice['name']}")

    finally:
        db.close()


if __name__ == "__main__":
    main()
```
