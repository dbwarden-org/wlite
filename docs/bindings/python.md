---
title: Python Binding
description: Python ctypes binding for wlite.
---

# Python Binding

The `wlite` Python package provides access to libwlite via ctypes. Define your schema in `.wlite` and use it from Python.

## Installation

```bash
pip install wlite
```

Requires libwlite to be installed on your system (`make install` from the libwlite repo).

## Basic usage

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

# Migrate
db.migrate(model)

# Query
rows = db.query("SELECT * FROM users")
for row in rows:
    print(row["name"])
```

## Types

| Python Type | C Equivalent | Description |
|-------------|--------------|-------------|
| `wlite.Database` | `wlite_db` | Open database connection |
| `wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `wlite.Statement` | `wlite_stmt` | Prepared SQL statement |
| `wlite.Transaction` | `wlite_tx` | Active transaction |

## Database operations

```python
import wlite

db = wlite.Database.open("app.db")

# Execute DDL/DML
db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")
db.execute("INSERT INTO test (name) VALUES (?)", ("hello",))

# Query returns list of dicts
rows = db.query("SELECT * FROM test")
for row in rows:
    print(row["id"], row["name"])
```

## Prepared statements

```python
stmt = db.prepare("SELECT * FROM users WHERE id = ? AND active = ?")
stmt.bind(1, 42)
stmt.bind(2, True)

while stmt.step():
    print(stmt.column_text(0), stmt.column_int64(1))

stmt.finalize()
```

Column access methods:

| Method | Returns |
|--------|---------|
| `stmt.column_count()` | int |
| `stmt.column_name(i)` | str |
| `stmt.column_type(i)` | int |
| `stmt.column_int64(i)` | int |
| `stmt.column_double(i)` | float |
| `stmt.column_text(i)` | str |

## Transactions

```python
tx = db.begin()
try:
    db.execute("INSERT INTO users (name) VALUES ('Alice')")
    db.execute("INSERT INTO users (name) VALUES ('Bob')")
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
try:
    db = wlite.Database.open("app.db")
except wlite.Error as e:
    print(f"Error: {e}")
```

Error codes:

| Code | Meaning |
|------|---------|
| `WLITE_OK` | Success |
| `WLITE_ERROR` | General error |
| `WLITE_NOT_FOUND` | File not found |
| `WLITE_MEMORY` | Allocation failed |
| `WLITE_IO` | I/O error |
| `WLITE_CORRUPT` | Corrupt data |
| `WLITE_RANGE` | Index out of range |

## Schema inspection

```python
model = wlite.Model.load("app.wlite")

for i in range(model.table_count()):
    table = model.table_at(i)
    print(f"Table: {table.name}")
    for j in range(table.field_count()):
        field = table.field_at(j)
        print(f"  {field.name} (type={field.type}, pk={field.is_primary_key})")
```
