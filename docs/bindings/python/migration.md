---
title: Migration and Schema Management
description: Loading models, migrating databases, diffing schemas, and inspecting compiled models.
---

# Migration and Schema Management

The wlite Python binding gives you tools to manage your database schema from Python code. You define your schema in `.wlite` files, compile them into models, and then use those models to migrate databases, inspect structure, and detect drift.

This document covers `Model.load`, `Database.open`, `migrate`, `diff`, `plan`, `check`, `snapshot`, `hash`, and working with compiled models.

## Loading models

A model is the in-memory representation of a `.wlite` schema. Load it once and reuse it across multiple databases or migration runs.

### Basic loading

```python
import wlite

model = wlite.Model.load("app.wlite")
```

The `load` method parses the `.wlite` file and returns an immutable model object. If the file contains syntax errors or is missing the method raises `wlite.Error` with `WLITE_PARSE_ERROR`.

### Loading from a string

If you have the schema content as a string you can load it without writing to disk:

```python
import wlite

schema = """
table users {
    id integer primary key
    name text not null
    email text not null unique
    active integer default 1
}

table posts {
    id integer primary key
    user_id integer references users(id)
    title text not null
    body text default ''
    created_at text default (datetime('now'))
}
"""

model = wlite.Model.load_string(schema)
```

### Loading multiple models

You can load several schema files and merge them if needed:

```python
import wlite

core_model = wlite.Model.load("core.wlite")
auth_model = wlite.Model.load("auth.wlite")

# Load each separately and migrate in order
db = wlite.Database.open("app.db")
db.migrate(core_model)
db.migrate(auth_model)
db.close()
```

Each model is independent. The second migration adds tables or columns defined in the auth schema without touching the core schema.

## Opening databases

`Database.open` creates a new database file or opens an existing one.

### Creating a new database

```python
import wlite

db = wlite.Database.open("app.db")
```

If `app.db` does not exist it is created. If it exists the existing file is opened for reading and writing.

### Opening in read-only mode

```python
import wlite

db = wlite.Database.open_readonly("archive.db")
```

A read-only connection cannot execute DDL or DML. Use it for queries where you want to prevent accidental modification.

### Opening with options

You can pass additional options when opening a database:

```python
import wlite

db = wlite.Database.open("app.db")
```

Consult the API reference for all available options.

## Migrating databases

The `migrate` method applies a model to a database. It compares the model against the current database schema and generates the necessary DDL to bring them in sync.

### Basic migration

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")
db.migrate(model)
db.close()
```

This is idempotent. Running it multiple times with the same model produces no changes after the first run.

### Migration with dry run

To see what SQL would be executed without actually running it, use the `plan` method instead:

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")
plan = db.plan(model)

print("SQL that would be executed:")
for step in plan.steps:
    print(f"  {step.sql}")

print(f"Total steps: {len(plan.steps)}")
db.close()
```

The `plan` method returns a `MigrationPlan` object containing the list of steps that would be executed.

### Migration with logging

You can pass a callback to receive each SQL statement as it runs:

```python
import wlite

def log_migration(sql):
    print(f"MIGRATE: {sql}")

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")
db.migrate(model, on_execute=log_migration)
db.close()
```

### Handling migration errors

Migration can fail if the schema change is incompatible. Catch the error and inspect it:

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

try:
    db.migrate(model)
except wlite.Error as e:
    if e.code == wlite.WLITE_MODEL_ERROR:
        print(f"Schema conflict: {e.message}")
    else:
        raise
finally:
    db.close()
```

## Diffing schemas

The `diff` method compares two models or a model against the current database state and returns the differences.

### Diff two models

```python
import wlite

old_model = wlite.Model.load("v1.wlite")
new_model = wlite.Model.load("v2.wlite")

delta = old_model.diff(new_model)

print(f"Tables added: {len(delta.added_tables)}")
for table in delta.added_tables:
    print(f"  + {table}")

print(f"Tables removed: {len(delta.removed_tables)}")
for table in delta.removed_tables:
    print(f"  - {table}")

print(f"Tables modified: {len(delta.modified_tables)}")
for table in delta.modified_tables:
    print(f"  ~ {table.name}")
    for col in table.added_columns:
        print(f"    + {col.name} {col.type}")
    for col in table.removed_columns:
        print(f"    - {col.name} {col.type}")
```

### Diff model against database

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

delta = db.diff(model)

if delta.has_changes():
    print("Database schema is out of date")
    print(delta.summary())
else:
    print("Database schema matches the model")

db.close()
```

## Planning migrations

The `plan` method generates a migration plan without executing it. Use it to review changes before applying them.

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

plan = db.plan(model)

print("Migration plan:")
for i, step in enumerate(plan.steps, 1):
    print(f"  {i}. {step.description}")
    print(f"     SQL: {step.sql}")

if not plan.steps:
    print("  No changes needed")

db.close()
```

Each step in the plan has a `description` (human-readable summary) and `sql` (the DDL to execute).

### Comparing plans across versions

```python
import wlite

model_v1 = wlite.Model.load("v1.wlite")
model_v2 = wlite.Model.load("v2.wlite")

db = wlite.Database.open("app.db")

plan_v1 = db.plan(model_v1)
plan_v2 = db.plan(model_v2)

print(f"Changes from v1 to v2:")
print(f"  v1 requires {len(plan_v1.steps)} steps")
print(f"  v2 requires {len(plan_v2.steps)} steps")

if len(plan_v2.steps) > 0:
    print("  New steps in v2:")
    for step in plan_v2.steps:
        print(f"    {step.description}")

db.close()
```

## Checking schema consistency

The `check` method verifies that the current database schema matches the expected model. It returns a result indicating whether the schema is consistent.

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

result = db.check(model)

if result.is_consistent():
    print("Schema is consistent with the model")
else:
    print("Schema drift detected:")
    for issue in result.issues:
        print(f"  {issue.kind}: {issue.message}")
```

### Handling check failures

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

try:
    result = db.check(model)
    if not result.is_consistent():
        print("Schema drift detected, running migration...")
        db.migrate(model)
except wlite.Error as e:
    print(f"Check failed: {e.message}")
finally:
    db.close()
```

## Taking snapshots

A snapshot captures the current schema state of the database. You can compare snapshots to detect drift over time.

### Creating a snapshot

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

snapshot = db.snapshot()

print(f"Snapshot created at {snapshot.timestamp}")
print(f"Tables: {snapshot.table_count}")
print(f"Hash: {snapshot.hash}")

db.close()
```

### Comparing snapshots

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

snapshot_before = db.snapshot()

# Apply some migration
db.migrate(model)

snapshot_after = db.snapshot()

if snapshot_before.hash != snapshot_after.hash:
    print("Schema changed between snapshots")
else:
    print("Schema is unchanged")

db.close()
```

## Hashing schemas

The `hash` method computes a deterministic hash of the model or database schema. Use it to quickly check if schemas match without a full diff.

### Hash a model

```python
import wlite

model = wlite.Model.load("app.wlite")
schema_hash = model.hash()
print(f"Model hash: {schema_hash}")
```

### Hash the database schema

To hash the current database schema, first introspect it into a schema object, then hash that:

```python
import wlite

db = wlite.Database.open("app.db")
schema = db.introspect()
db_hash = schema.hash()
print(f"Database schema hash: {db_hash}")
db.close()
```

### Using hashes for version checks

```python
import wlite

model = wlite.Model.load("app.wlite")
expected_hash = model.hash()

db = wlite.Database.open("app.db")
schema = db.introspect()
actual_hash = schema.hash()

if actual_hash == expected_hash:
    print("Schema is up to date")
else:
    print(f"Schema mismatch: expected {expected_hash}, got {actual_hash}")
    db.migrate(model)
    print("Migration complete")

db.close()
```

## Compiled models

A compiled model is a pre-parsed representation of a schema that can be loaded faster than re-parsing the `.wlite` source. The binding supports saving and loading compiled models.

### Compiling a model

```python
import wlite

model = wlite.Model.load("app.wlite")
model.compile("app.wlitem")
```

The compiled file has a `.wlitem` extension. It contains the parsed schema in a binary format optimized for fast loading.

### Loading a compiled model

```python
import wlite

model = wlite.Model.load_compiled("app.wlitem")
db = wlite.Database.open("app.db")
db.migrate(model)
db.close()
```

Loading a compiled model skips the parsing step. For large schemas this can be noticeably faster.

### Compiling and loading in one step

```python
import wlite

# First run: compile
model = wlite.Model.load("app.wlite")
model.compile("app.wlitem")

# Subsequent runs: load compiled
model = wlite.Model.load_compiled("app.wlitem")
db = wlite.Database.open("app.db")
db.migrate(model)
db.close()
```

### Verifying a compiled model

You can check that a compiled model matches its source:

```python
import wlite

source_model = wlite.Model.load("app.wlite")
compiled_model = wlite.Model.load_compiled("app.wlitem")

if source_model.hash() == compiled_model.hash():
    print("Compiled model is up to date")
else:
    print("Compiled model is stale, recompiling...")
    source_model.compile("app.wlitem")
```

## Inspecting model structure

The model object provides methods to enumerate tables and fields.

### Listing tables

```python
import wlite

model = wlite.Model.load("app.wlite")

print(f"Total tables: {model.table_count()}")

for i in range(model.table_count()):
    table = model.table_at(i)
    print(f"  {table.name}")
```

### Listing fields in a table

```python
import wlite

model = wlite.Model.load("app.wlite")
table = model.table_at(0)

print(f"Table: {table.name}")
print(f"Fields: {table.field_count()}")

for i in range(table.field_count()):
    field = table.field_at(i)
    pk = " PRIMARY KEY" if field.is_primary_key else ""
    nn = " NOT NULL" if field.is_not_null else ""
    print(f"  {field.name}: {field.type}{pk}{nn}")
```

### Checking for a specific table

```python
import wlite

model = wlite.Model.load("app.wlite")

table_names = [model.table_at(i).name for i in range(model.table_count())]

if "users" in table_names:
    print("Users table exists")
else:
    print("Users table is missing")
```

### Checking for a specific field

```python
import wlite

model = wlite.Model.load("app.wlite")

for i in range(model.table_count()):
    table = model.table_at(i)
    if table.name == "users":
        field_names = [table.field_at(j).name for j in range(table.field_count())]
        if "email" in field_names:
            print("Email field exists in users table")
        else:
            print("Email field is missing from users table")
        break
```

## Migration workflow example

Here is a complete workflow that demonstrates model loading, migration, checking, and snapshotting:

```python
import wlite
import sys


def ensure_schema(db_path, model_path):
    model = wlite.Model.load(model_path)
    db = wlite.Database.open(db_path)

    result = db.check(model)
    if result.is_consistent():
        print("Schema is current")
    else:
        print("Schema is out of date, migrating...")
        plan = db.plan(model)
        for step in plan.steps:
            print(f"  {step.description}")
        db.migrate(model)
        print("Migration complete")

    snapshot = db.snapshot()
    print(f"Schema hash: {snapshot.hash}")

    return db


def main():
    db = ensure_schema("app.db", "app.wlite")
    try:
        rows = db.query("SELECT name FROM sqlite_master WHERE type='table'")
        print("Tables in database:")
        for row in rows:
            print(f"  {row['name']}")
    finally:
        db.close()


if __name__ == "__main__":
    main()
```

This pattern is suitable for application startup. It ensures the schema is always current before the application begins processing requests.
