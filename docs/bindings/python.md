---
title: Python Binding
description: Python ctypes binding for wlite. Use .wlite schemas from Python.
---

# Python Binding

The `wlite` Python package provides access to libwlite via ctypes. Define your schema in `.wlite` and use it from Python.

## Installation

```bash
pip install wlite
```

Requires libwlite to be installed on your system.

## Usage

```python
import wlite

# Load a model
model = wlite.Model.load("app.wlite")

# Open a database
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

## Prepared Statements

```python
stmt = db.prepare("SELECT * FROM users WHERE id = ?")
stmt.bind(1, 42)
while stmt.step():
    print(stmt.column_text(0))
stmt.finalize()
```

## Transactions

```python
with db.transaction() as tx:
    db.execute("INSERT INTO users (name) VALUES (?)", ("Alice",))
    # Automatically committed on clean exit, rolled back on exception
```

## Error Handling

All operations raise `wlite.Error` on failure:

```python
try:
    db = wlite.Database.open("app.db")
except wlite.Error as e:
    print(f"Error: {e}")
```
