---
title: Data Flow
description: The complete pipeline from .wlite model to executable SQL.
---

# Data Flow

This page walks through the complete pipeline from a `.wlite` model file to executable SQL. Every call to `wlite_migrate` or `wlite_diff` goes through these stages. Understanding each stage helps you predict what wlite will do, debug unexpected behavior, and design schemas that migrate cleanly.

## Pipeline overview

```
    .wlite file
         |
         v
    +-----------+
    |  Stage 1  |  Parse
    |  parser.c |
    +-----------+
         |
         v
    WlSchema (model)
         |
         v
    +-----------+
    |  Stage 2  |  Introspect
    |introspect |
    +-----------+
         |
         v
    WlSchema (database)
         |
         v
    +-----------+
    |  Stage 3  |  Diff
    |   diff.c  |
    +-----------+
         |
         v
      WlDiff
         |
         v
    +-----------+
    |  Stage 4  |  Plan
    | planner.c |
    +-----------+
         |
         v
      WlPlan
         |
         v
    +-----------+
    |  Stage 5  |  Migrate
    | migrate.c |
    +-----------+
         |
         v
   SQL executed, checksum recorded
```

Each stage is independent and can be invoked separately. The CLI commands `wlite diff`, `wlite plan`, and `wlite migrate` expose subsets of the pipeline.

## Stage 1: Parse

The parser reads a `.wlite` file and produces an in-memory `WlSchema`. This is the model representation that flows through the rest of the pipeline.

```
.wlite file  --->  parser.c  --->  WlSchema
```

### What the parser handles

The parser is a hand-written recursive descent parser. It handles:

- **Tokenization**: keywords, identifiers, strings, numbers, operators
- **Recursive descent**: model blocks, field declarations, constraints, indexes, views, triggers
- **Comment stripping**: line comments (`#`) and block comments (`/* */`)
- **Error reporting**: line number and column for syntax errors
- **String handling**: quoted strings with escape sequences
- **Number parsing**: integers and floating-point literals

### WlSchema structure

The parser produces this in-memory representation:

```c
typedef struct WlSchema {
    WlModelConfig config;       /* model_config block (name, version) */
    WlTable *tables;            /* array of tables */
    size_t table_count;         /* number of tables */
    WlIndex *indexes;           /* array of indexes */
    size_t index_count;         /* number of indexes */
    WlView *views;              /* array of views */
    size_t view_count;          /* number of views */
    WlTrigger *triggers;        /* array of triggers */
    size_t trigger_count;       /* number of triggers */
} WlSchema;
```

Each `WlTable` contains:

```c
typedef struct WlTable {
    char *name;                 /* SQLite table name */
    char *comment;              /* table comment (or NULL) */
    WlField *fields;            /* array of columns */
    size_t field_count;         /* number of columns */
    WlConstraint *constraints;  /* array of constraints */
    size_t constraint_count;    /* number of constraints */
    int strict;                 /* STRICT mode flag */
    int without_rowid;          /* WITHOUT ROWID flag */
} WlTable;
```

Each `WlField` contains:

```c
typedef struct WlField {
    char *name;                 /* column name */
    WlColType type;             /* normalized column type */
    char *raw_type;             /* original type string from model */
    int nullable;               /* 1 if nullable, 0 if NOT NULL */
    int primary_key;            /* 1 if part of primary key */
    int autoincrement;          /* 1 if AUTOINCREMENT */
    int unique;                 /* 1 if UNIQUE */
    char *default_value;        /* default expression (or NULL) */
    char *collate;              /* collation name (or NULL) */
    char *references_table;     /* foreign key target table (or NULL) */
    char *references_column;    /* foreign key target column (or NULL) */
    char *generated_expr;       /* generated column expression (or NULL) */
    int generated_stored;       /* 1 if STORED, 0 if VIRTUAL */
    char *comment;              /* column comment (or NULL) */
} WlField;
```

### Parsing example

Given this `.wlite` file:

```
model_config {
    name "my_application"
    version 1
}

model User {
    table "users"

    field id integer {
        primary_key
        autoincrement
    }

    field name text {
        not_null
    }

    field email text

    field active boolean {
        not_null
        default true
    }
}
```

The parser produces a `WlSchema` with:

| Component | Value |
|-----------|-------|
| config.name | `"my_application"` |
| config.version | `1` |
| table_count | `1` |
| tables[0].name | `"users"` |
| tables[0].field_count | `4` |
| tables[0].fields[0].name | `"id"` |
| tables[0].fields[0].type | `WL_TYPE_INTEGER` |
| tables[0].fields[0].primary_key | `1` |
| tables[0].fields[0].autoincrement | `1` |
| tables[0].fields[1].name | `"name"` |
| tables[0].fields[1].type | `WL_TYPE_TEXT` |
| tables[0].fields[1].nullable | `0` |
| tables[0].fields[3].name | `"active"` |
| tables[0].fields[3].type | `WL_TYPE_INTEGER` |
| tables[0].fields[3].default_value | `"true"` |

### Error cases at parse stage

| Error | Example | Message |
|-------|---------|---------|
| Missing brace | `model User { field id text` | `expected '}' at line 5, column 1` |
| Invalid keyword | `model User { kol id text }` | `unexpected keyword 'kol' at line 2, column 5` |
| Unclosed string | `model User { table "users }` | `unterminated string at line 2, column 13` |
| Duplicate field | `model User { field id text field id integer }` | `duplicate field 'id' at line 2, column 28` |
| Invalid type | `model User { field id varchar255 }` | `unknown type 'varchar255' at line 2, column 16` |
| Missing model name | `model { field id text }` | `expected identifier after 'model' at line 1, column 7` |
| Empty file | (no content) | `unexpected end of file` |

## Stage 2: Introspect

The introspector reads the live SQLite database and produces a `WlSchema` in the same format as the parser output. This lets the diff engine compare two identical structures.

```
SQLite DB  --->  introspect.c  --->  WlSchema
```

### What the introspector reads

The introspector queries SQLite metadata tables and pragmas:

| Source | What it reads |
|--------|---------------|
| `sqlite_master` | Table definitions, CREATE TABLE sql |
| `PRAGMA table_info(name)` | Column name, type, notnull, default, pk |
| `PRAGMA table_xinfo(name)` | Generated columns (extended info) |
| `PRAGMA index_list(name)` | Index names, uniqueness |
| `PRAGMA index_info(name)` | Indexed columns |
| `PRAGMA foreign_key_list(name)` | Foreign key targets and actions |
| `sqlite_master` (WHERE type='view') | View definitions |
| `sqlite_master` (WHERE type='trigger') | Trigger definitions |

### Type normalization

SQLite stores types as strings and uses affinity rules. The introspector normalizes types before comparison so equivalent types do not trigger migrations:

| Stored type | Normalized to | Reason |
|-------------|---------------|--------|
| `INT`, `INTEGER`, `INT2`, `INT4`, `INT8` | `INTEGER` | INTEGER affinity |
| `BIGINT`, `SMALLINT`, `TINYINT` | `INTEGER` | INTEGER affinity |
| `UNSIGNED INT`, `SIGNED`, `MEDIUMINT` | `INTEGER` | INTEGER affinity |
| `BOOLEAN`, `BOOL` | `INTEGER` | SQLite stores booleans as 0/1 |
| `REAL`, `DOUBLE`, `FLOAT` | `REAL` | REAL affinity |
| `NUMERIC`, `DECIMAL` | `REAL` | REAL affinity |
| `TEXT`, `VARCHAR`, `CHARACTER` | `TEXT` | TEXT affinity |
| `CLOB`, `NATIVE CHARACTER` | `TEXT` | TEXT affinity |
| `BLOB` | `BLOB` | No normalization needed |
| `DATE`, `DATETIME`, `TIMESTAMP` | `TEXT` | SQLite stores dates as text |
| `UUID`, `JSON` | `TEXT` | Custom types stored as text |
| Anything else | preserved as-is | Arbitrary type names |

This normalization means changing `INT` to `INTEGER` in your model does not trigger a migration.

### Default value normalization

Default values are also normalized for comparison:

| Stored form | Normalized form | Reason |
|-------------|-----------------|--------|
| `CURRENT_TIMESTAMP` | `CURRENT_TIMESTAMP` | Case-insensitive comparison |
| `current_timestamp` | `CURRENT_TIMESTAMP` | Lowercased variant |
| `0` | `0` | Numeric |
| `FALSE` | `0` | Boolean coercion |
| `TRUE` | `1` | Boolean coercion |
| `'text'` | `text` | String quoting stripped |
| `NULL` | `NULL` | Case-insensitive |

### Introspection example

If the live database has:

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT,
    active INT DEFAULT 1
);
```

The introspector produces a `WlSchema` with:

| Field | Value |
|-------|-------|
| tables[0].name | `"users"` |
| tables[0].fields[0].name | `"id"` |
| tables[0].fields[0].type | `WL_TYPE_INTEGER` |
| tables[0].fields[0].primary_key | `1` |
| tables[0].fields[1].name | `"name"` |
| tables[0].fields[1].type | `WL_TYPE_TEXT` |
| tables[0].fields[1].nullable | `0` |
| tables[0].fields[2].name | `"email"` |
| tables[0].fields[2].type | `WL_TYPE_TEXT` |
| tables[0].fields[2].nullable | `1` |
| tables[0].fields[3].name | `"active"` |
| tables[0].fields[3].type | `WL_TYPE_INTEGER` |
| tables[0].fields[3].default_value | `"1"` |

Note that `INT` is normalized to `INTEGER` and `DEFAULT 1` is preserved as-is.

### Error cases at introspect stage

| Error | Cause | Message |
|-------|-------|---------|
| Database locked | Another process holds a write lock | `database is locked` |
| File not found | Path does not exist | `unable to open database file` |
| Corrupt database | SQLite header损坏 | `database disk image is malformed` |
| Permission denied | Read-only filesystem | `attempt to write a readonly database` |
| Not a database | File exists but is not SQLite | `file is not a database` |

## Stage 3: Diff

The diff engine compares the model schema against the database schema and produces a `WlDiff`. This is the core comparison logic that determines what needs to change.

```
WlSchema (model)  +  WlSchema (db)  --->  diff.c  --->  WlDiff
```

### Diff operations

Each difference between model and database is recorded as an operation:

| Operation | Meaning | Classification |
|-----------|---------|----------------|
| `WL_DIFF_ADD_TABLE` | New table to create | Additive |
| `WL_DIFF_DROP_TABLE` | Table to remove | Subtractive |
| `WL_DIFF_RENAME_TABLE` | Table to rename | Subtractive |
| `WL_DIFF_ADD_COLUMN` | New column to add | Additive |
| `WL_DIFF_DROP_COLUMN` | Column to remove | Subtractive |
| `WL_DIFF_ALTER_COLUMN` | Column type, nullability, default, or comment change | Alterative |
| `WL_DIFF_ADD_INDEX` | New index to create | Additive |
| `WL_DIFF_DROP_INDEX` | Index to remove | Subtractive |
| `WL_DIFF_ADD_CONSTRAINT` | New constraint to add | Alterative |
| `WL_DIFF_DROP_CONSTRAINT` | Constraint to remove | Alterative |
| `WL_DIFF_REBUILD_TABLE` | Full table rebuild required | Rebuild |

### Classification

Each difference is classified by severity and the SQL it requires:

| Classification | Description | SQL pattern |
|----------------|-------------|-------------|
| **Additive** | New tables, columns, indexes | `CREATE TABLE`, `ALTER TABLE ADD COLUMN`, `CREATE INDEX` |
| **Subtractive** | Dropping tables or columns | `DROP TABLE`, `ALTER TABLE DROP COLUMN`, `DROP INDEX` |
| **Alterative** | Changing column properties | `ALTER TABLE ALTER COLUMN` (when supported) |
| **Rebuild** | Changes SQLite cannot express | Full table rebuild sequence |

### Rebuild triggers

A rebuild is triggered when any of these change:

| Change | Example |
|--------|---------|
| Column type | `TEXT` to `INTEGER` |
| Column nullability | `NULL` to `NOT NULL` |
| Column default | `DEFAULT 0` to `DEFAULT 1` |
| Column generated | Adding a generated column |
| Primary key | Adding or changing the PK |
| Unique constraint | Adding or removing UNIQUE |
| Check constraint | Adding or changing a CHECK |
| Foreign key | Adding or changing a REFERENCES |
| STRICT mode | Enabling or disabling STRICT |
| WITHOUT ROWID | Enabling or disabling WITHOUT ROWID |

### When rebuilds do NOT trigger

These changes use SQLite's `ALTER TABLE` directly:

| Change | Action |
|--------|--------|
| Add a column | `ALTER TABLE ... ADD COLUMN` |
| Drop a column | `ALTER TABLE ... DROP COLUMN` (SQLite 3.35+) |
| Rename a table | `ALTER TABLE ... RENAME TO` |
| Rename a column | `ALTER TABLE ... RENAME COLUMN` (SQLite 3.25+) |
| Add an index | `CREATE INDEX` |
| Drop an index | `DROP INDEX` |

### Diff examples

**Example 1: Adding a column**

Model has: `id`, `name`, `email`, `created_at`
Database has: `id`, `name`, `email`

Diff result:

```
WL_DIFF_ADD_COLUMN: created_at (DATETIME, NOT NULL, DEFAULT CURRENT_TIMESTAMP)
Classification: Additive
SQL: ALTER TABLE users ADD COLUMN created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP;
```

**Example 2: Changing a column type**

Model has: `id` (INTEGER), `name` (TEXT)
Database has: `id` (INTEGER), `name` (INTEGER)

Diff result:

```
WL_DIFF_REBUILD_TABLE: users
Classification: Rebuild
Reason: column 'name' type changed from INTEGER to TEXT
```

**Example 3: Dropping a table**

Model does not have: `old_logs`
Database has: `old_logs`

Diff result:

```
WL_DIFF_DROP_TABLE: old_logs
Classification: Subtractive
SQL: DROP TABLE old_logs;
```

**Example 4: Multiple changes on same table**

Model has: `id`, `name` (TEXT, NOT NULL), `bio` (TEXT)
Database has: `id`, `name` (TEXT)

Diff result:

```
WL_DIFF_ALTER_COLUMN: name (nullability changed)
WL_DIFF_ADD_COLUMN: bio (TEXT, nullable)
Classification: Rebuild (nullability change requires rebuild)
```

### Error cases at diff stage

| Error | Cause | Message |
|-------|-------|---------|
| Empty model | No tables in model | `model contains no tables` |
| Schema mismatch | Database has tables not in model | (informational, not an error) |
| Invalid references | Foreign key references nonexistent table | `table 'X' referenced by 'Y' does not exist` |

## Stage 4: Plan

The planner converts the diff into an ordered `WlPlan` of executable SQL statements. This is where rebuilds are expanded, ordering is resolved, and multiple operations on the same table are collapsed.

```
WlDiff  --->  planner.c  --->  WlPlan
```

### Planning rules

The planner follows these rules in order:

1. **Dependency ordering**: Tables are created before columns are added. Foreign key targets are created before referencing tables.
2. **Rebuild expansion**: Table rebuilds are expanded into a sequence: disable FK checks, create staging, copy data, drop old, rename, recreate indexes, enable FK checks.
3. **Collapse**: Multiple rebuilds on the same table are collapsed into one.
4. **Index ordering**: Indexes are created after the tables they reference exist.
5. **View and trigger ordering**: Views and triggers are created after all tables exist.

### WlPlan structure

```c
typedef struct WlPlan {
    WlPlanStep *steps;      /* array of SQL statements */
    size_t step_count;       /* number of steps */
} WlPlan;

typedef struct WlPlanStep {
    char *sql;               /* SQL statement to execute */
    int is_rebuild;          /* 1 if part of a table rebuild */
    char *table_name;        /* table name (if rebuild step) */
} WlPlanStep;
```

### Rebuild expansion

A table rebuild is expanded into a sequence of SQL statements:

```sql
-- 1. Disable foreign key checks
PRAGMA foreign_keys = OFF;

-- 2. Create staging table with correct schema
CREATE TABLE _staging_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- 3. Copy data from old table
INSERT INTO _staging_users (id, name, email, created_at)
SELECT id, name, email, created_at FROM users;

-- 4. Drop old table
DROP TABLE users;

-- 5. Rename staging to final name
ALTER TABLE _staging_users RENAME TO users;

-- 6. Recreate indexes
CREATE INDEX IF NOT EXISTS users_email ON users (email);

-- 7. Re-enable foreign key checks
PRAGMA foreign_keys = ON;
```

### Collapse logic

Multiple rebuilds on the same table are collapsed into a single rebuild. This avoids redundant work:

```
Before collapse:
  ALTER TABLE users ADD COLUMN created_at ...;
  ALTER TABLE users ALTER COLUMN name ...;  -- rebuild
  ALTER TABLE users ADD COLUMN bio ...;     -- another rebuild?

After collapse:
  ALTER TABLE users ADD COLUMN created_at ...;
  -- single rebuild with all changes
  CREATE TABLE _staging_users (...) AS SELECT ...;
  DROP TABLE users;
  ALTER TABLE _staging_users RENAME TO users;
```

### Ordering example

Given this diff:

```
ADD_TABLE: orders
ADD_TABLE: order_items (FK references orders)
ADD_INDEX: orders_customer
ADD_COLUMN: users.bio
```

The planner produces this order:

```sql
-- 1. Create tables in dependency order
CREATE TABLE orders (...);
CREATE TABLE order_items (...);  -- references orders

-- 2. Add columns to existing tables
ALTER TABLE users ADD COLUMN bio TEXT;

-- 3. Create indexes after tables exist
CREATE INDEX orders_customer ON orders (customer_id);
```

### Plan examples

**Example 1: Simple add column**

Diff: `ADD_COLUMN: users.created_at (DATETIME, NOT NULL, DEFAULT CURRENT_TIMESTAMP)`

Plan:

```sql
ALTER TABLE users ADD COLUMN created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP;
```

One step. No rebuild needed.

**Example 2: Type change (rebuild)**

Diff: `ALTER_COLUMN: users.name (TEXT -> INTEGER)`

Plan:

```sql
PRAGMA foreign_keys = OFF;
CREATE TABLE _staging_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name INTEGER NOT NULL
) AS SELECT id, name FROM users;
DROP TABLE users;
ALTER TABLE _staging_users RENAME TO users;
PRAGMA foreign_keys = ON;
```

Five steps. Full rebuild required.

**Example 3: New table with index**

Diff: `ADD_TABLE: posts`, `ADD_INDEX: posts_author`

Plan:

```sql
CREATE TABLE posts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    body TEXT NOT NULL,
    author_id INTEGER NOT NULL
);
CREATE INDEX posts_author ON posts (author_id);
```

Two steps. Table created before index.

### Error cases at plan stage

| Error | Cause | Message |
|-------|-------|---------|
| Circular FK | Table A references B, B references A | `circular foreign key dependency` |
| Missing FK target | FK references nonexistent table | `referenced table 'X' not found in model` |
| Invalid rebuild | Rebuild SQL would fail | (diagnostic message) |

## Stage 5: Migrate

The migration engine executes the plan within a transaction. This is the final stage that makes changes to the database.

```
WlPlan  --->  migrate.c  --->  SQL executed, checksum recorded
```

### What migrate does

1. Begin a transaction (`BEGIN IMMEDIATE`)
2. Execute each step in order
3. After each step, optionally verify the result
4. Record a checksum of the final schema state
5. Commit the transaction (or rollback on error)

### Checksums

After migration, libwlite computes a SHA-256 hash of the schema state. This hash is stored and used by `wlite_check` to verify that the database matches the model without re-introspecting the entire schema.

The checksum covers:

| Component | What is hashed |
|-----------|----------------|
| Table names | Alphabetically sorted |
| Column names | Within each table, alphabetically sorted |
| Column types | Normalized types |
| Column constraints | NOT NULL, UNIQUE, PRIMARY KEY, DEFAULT |
| Index definitions | Name, table, columns, uniqueness, WHERE clause |
| Table options | STRICT, WITHOUT ROWID |

### Rollback

Every migration is wrapped in a transaction. If any step fails, the entire migration is rolled back. The database returns to its pre-migration state.

```c
// wlite_migrate wraps everything in:
BEGIN IMMEDIATE;
  CREATE TABLE _staging_users (...) AS SELECT ...;
  DROP TABLE users;
  ALTER TABLE _staging_users RENAME TO users;
  CREATE INDEX ...;
COMMIT;
// If any step fails: ROLLBACK
```

### Error cases at migrate stage

| Error | Cause | Message |
|-------|-------|---------|
| Constraint violation | NOT NULL on column with NULLs | `CHECK constraint failed` |
| Disk full | No space for new table | `database or disk is full` |
| Permission denied | Read-only database | `attempt to write a readonly database` |
| Busy timeout | Database locked by another process | `database is locked` |
| Foreign key violation | FK references nonexistent row | `FOREIGN KEY constraint failed` |
| Syntax error | Generated SQL is malformed | `near "X": syntax error` |

## Complete C example

This example shows all five stages using the C API:

```c
#include <stdio.h>
#include <string.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_result rc;

    /* ==========================================
     * Stage 1: Parse
     * Load the .wlite model file into WlSchema
     * ========================================== */
    rc = wlite_model_load_file("app.wlite", &model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Parse error: %s\n", wlite_strerror(rc));
        return 1;
    }

    /* Validate the model structure */
    rc = wlite_model_validate(model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Model validation failed: %s\n", wlite_strerror(rc));
        wlite_model_free(model);
        return 1;
    }

    printf("Stage 1 complete: model loaded\n");
    printf("  Model: %s v%d\n",
           wl_schema_model_name(model->schema),
           wl_schema_model_version(model->schema));
    printf("  Tables: %zu\n", wlite_model_table_count(model));

    /* Print table details */
    for (size_t i = 0; i < wlite_model_table_count(model); i++) {
        const wlite_table *table = wlite_model_table(model, "users");
        if (!table) continue;
        printf("  Table: %s (%zu fields)\n",
               wlite_table_name(table),
               wlite_table_field_count(table));
        for (size_t j = 0; j < wlite_table_field_count(table); j++) {
            const wlite_field *field = wlite_table_field_at(table, j);
            if (!field) continue;
            printf("    %s: type=%d nullable=%d pk=%d\n",
                   wlite_field_name(field),
                   wlite_field_type(field),
                   wlite_field_is_nullable(field),
                   wlite_field_is_primary_key(field));
        }
    }

    /* ==========================================
     * Stage 2: Introspect
     * Open the database and read its schema
     * ========================================== */
    rc = wlite_open("app.db", &db);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Database error: %s\n", wlite_strerror(rc));
        wlite_model_free(model);
        return 1;
    }

    printf("\nStage 2 complete: database opened and introspected\n");

    /* ==========================================
     * Stage 3: Diff
     * Compare model against database
     * ========================================== */
    /* Diff is computed internally by wlite_migrate */

    /* ==========================================
     * Stage 4: Plan
     * The plan is computed internally
     * ========================================== */
    printf("\nStage 3 & 4: diff and plan computed internally\n");

    /* ==========================================
     * Stage 5: Migrate
     * Execute the plan
     * ========================================== */
    rc = wlite_migrate(db, model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Migration failed: %s\n", wlite_strerror(rc));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }
    printf("\nStage 5 complete: migration executed and committed\n");

    /* ==========================================
     * Query the database
     * ========================================== */
    wlite_stmt *stmt = NULL;
    rc = wlite_prepare(db,
        "INSERT INTO todos (title, completed, created_at) "
        "VALUES (?, 0, strftime('%s','now'))", &stmt);
    if (rc == WLITE_OK) {
        wlite_bind_text(stmt, 1, "Buy groceries");
        wlite_step(stmt);
        wlite_stmt_finalize(stmt);
    }

    rc = wlite_prepare(db,
        "SELECT id, title, completed FROM todos", &stmt);
    if (rc == WLITE_OK) {
        printf("\nTodos:\n");
        while (wlite_step(stmt) == WLITE_OK) {
            printf("  [%s] %s (id=%lld)\n",
                wlite_column_int64(stmt, 2) ? "x" : " ",
                wlite_column_text(stmt, 1),
                (long long)wlite_column_int64(stmt, 0));
        }
        wlite_stmt_finalize(stmt);
    }

    /* Cleanup */
    wlite_close(db);
    wlite_model_free(model);

    printf("\nDone.\n");
    return 0;
}
```

### Compile and run

```bash
gcc -o app main.c -lwlite -lsqlite3
./app
```

### Expected output

```
Stage 1 complete: model loaded
  Model: my_application v1
  Tables: 1
  Table: users (4 fields)
    id: type=1 nullable=0 pk=1
    name: type=3 nullable=0 pk=0
    email: type=3 nullable=1 pk=0
    active: type=1 nullable=0 pk=0

Stage 2 complete: database opened and introspected

Stage 3 & 4: diff and plan computed internally

Stage 5 complete: migration executed and committed

Todos:
  [ ] Buy groceries (id=1)

Done.
```

## Complete Python example

This example shows the same five stages using the Python API:

```python
#!/usr/bin/env python3
"""Complete data flow example using wlite Python bindings."""

import wlite

def main():
    # ==========================================
    # Stage 1: Parse
    # Load the .wlite model file into WlSchema
    # ==========================================
    model = wlite.Model.load("app.wlite")
    model.validate()

    print("Stage 1 complete: model loaded")
    print(f"  Tables: {model.table_count()}")

    for i in range(model.table_count()):
        table = model.table_at(i)
        print(f"  Table: {table.name()} ({table.field_count()} fields)")

    # ==========================================
    # Stage 2: Introspect
    # Open the database and read its schema
    # ==========================================
    db = wlite.Database.open("app.db")

    print("\nStage 2 complete: database opened and introspected")

    # ==========================================
    # Stage 3: Diff
    # Compare model against database
    # ==========================================
    plan = db.diff(model)

    print(f"\nStage 3 complete: diff computed")
    print(f"  Migration steps: {plan.step_count()}")

    # ==========================================
    # Stage 4: Plan
    # The plan is now available for inspection
    # ==========================================
    print(f"\nStage 4 complete: plan ready")

    if plan.step_count() == 0:
        print("  No changes needed.")
    else:
        print(f"  Plan has {plan.step_count()} SQL statements")
        for i in range(plan.step_count()):
            step = plan.step_at(i)
            print(f"    Step {i + 1}: {step.sql[:60]}...")

    # ==========================================
    # Stage 5: Migrate
    # Execute the plan
    # ==========================================
    if plan.step_count() > 0:
        db.migrate(model)
        print(f"\nStage 5 complete: migration executed and committed")
    else:
        print(f"\nStage 5: nothing to migrate")

    # ==========================================
    # Query the database
    # ==========================================
    db.execute(
        "INSERT INTO todos (title, completed, created_at) "
        "VALUES (?, 0, strftime('%s','now'))",
        ("Buy groceries",),
    )

    rows = db.query("SELECT id, title, completed FROM todos")
    print("\nTodos:")
    for row in rows:
        status = "x" if row["completed"] else " "
        print(f"  [{status}] {row['title']} (id={row['id']})")

    # Cleanup
    plan.free()
    db.close()
    model.free()

    print("\nDone.")

if __name__ == "__main__":
    main()
```

### Expected output

```
Stage 1 complete: model loaded
  Tables: 1
  Table: users (4 fields)

Stage 2 complete: database opened and introspected

Stage 3 complete: diff computed
  Migration steps: 1

Stage 4 complete: plan ready
  Plan has 1 SQL statements
    Step 1: ALTER TABLE users ADD COLUMN created_at DATETIME NOT NULL DEFAUL...

Stage 5 complete: migration executed and committed

Todos:
  [ ] Buy groceries (id=1)

Done.
```

## Error cases at each stage

The following table summarizes all error cases across the pipeline:

| Stage | Error | Code | Recovery |
|-------|-------|------|----------|
| Parse | Syntax error | `WLITE_PARSE_ERROR` | Fix the `.wlite` file |
| Parse | File not found | `WLITE_NOT_FOUND` | Check the file path |
| Parse | Out of memory | `WLITE_OUT_OF_MEMORY` | Reduce model size |
| Parse | Model validation | `WLITE_MODEL_ERROR` | Fix model structure |
| Introspect | Database locked | `WLITE_BUSY` | Wait and retry |
| Introspect | File not found | `WLITE_NOT_FOUND` | Check the database path |
| Introspect | Corrupt database | `WLITE_SQLITE_ERROR` | Restore from backup |
| Introspect | Permission denied | `WLITE_IO_ERROR` | Check file permissions |
| Diff | Empty model | `WLITE_MODEL_ERROR` | Add tables to model |
| Diff | Invalid FK references | `WLITE_MODEL_ERROR` | Fix references |
| Plan | Circular FK | `WLITE_ERROR` | Break the cycle |
| Plan | Missing FK target | `WLITE_ERROR` | Add referenced table |
| Migrate | Constraint violation | `WLITE_CONSTRAINT_ERROR` | Fix data or schema |
| Migrate | Disk full | `WLITE_IO_ERROR` | Free disk space |
| Migrate | Permission denied | `WLITE_IO_ERROR` | Check write permissions |
| Migrate | Busy timeout | `WLITE_BUSY` | Wait and retry |
| Migrate | FK violation | `WLITE_CONSTRAINT_ERROR` | Check FK data |
| Migrate | Syntax error | `WLITE_SQLITE_ERROR` | Report as bug |

## Thread safety across stages

| Object | Thread-safe? | Notes |
|--------|-------------|-------|
| `WlSchema` (parsed) | Yes | Immutable after parsing |
| `WlSchema` (introspected) | Yes | Immutable after introspection |
| `WlDiff` | Yes | Immutable after computation |
| `WlPlan` | Yes | Immutable after planning |
| `wlite_db` | No | One connection per thread |
| `wlite_model` | Yes | Immutable after loading |

Models can be shared across threads. Database connections cannot. This means you can parse a model once and use it to migrate multiple databases concurrently, each on its own thread with its own connection.

## Memory ownership across stages

| Stage | Output | Owner | Free function |
|-------|--------|-------|---------------|
| Parse | `WlSchema` | Caller (via `wlite_model`) | `wlite_model_free()` |
| Introspect | `WlSchema` | `wlite_db` | `wlite_close()` |
| Diff | `WlDiff` | Caller (via `wlite_plan`) | `wlite_plan_free()` |
| Plan | `WlPlan` | Caller | `wlite_plan_free()` |
| Migrate | (side effects) | N/A | N/A |

The introspected schema is owned by the database connection. Do not use it after closing the connection. The parsed model is owned by the caller and must be freed explicitly.
