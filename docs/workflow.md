---
title: Migration Workflow
description: Complete migration workflow from init to snapshot, including CLI output, --json mode, schema hashing, and CI/CD integration.
---

# Migration Workflow

This page walks through the complete wlite migration workflow: from project
initialization to schema snapshots. Every CLI command is covered with its actual
output format, flags, and integration patterns.

## Overview

The standard wlite workflow follows a predictable sequence:

```
init -> write model -> diff -> migrate -> check -> snapshot
```

Each step is optional depending on your use case. You can jump in at any point.
If the database already exists, `diff` tells you what needs to change. If the
database is empty, `migrate` creates everything from scratch.

## Step 1: Initialize a project

```bash
wlite init
```

This creates two things in the current directory:

- `schema.wlite` (the model file, seeded with a `database` block)
- `migrations/` directory (for generated SQL files)

Output:

```
Created schema.wlite
Created migrations/
```

If either file already exists, wlite prints a warning and skips creation:

```
schema.wlite already exists, skipping
migrations/ already exists, skipping
```

The seeded `schema.wlite` looks like this:

```
# wlite schema

database {
}
```

You edit `schema.wlite` to define your tables, fields, indexes, views, and
triggers. This file is the single source of truth for your database schema.

## Step 2: Write your model

Define the schema you want in `schema.wlite`. The format is documented in the
[Grammar Reference](grammar.md). Here is a minimal example:

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

    field username text {
        not_null
        unique
    }

    field email text {
        not_null
    }

    field created_at datetime {
        not_null
        default CURRENT_TIMESTAMP
    }
}
```

This defines a `users` table with four columns. The model is declarative: you
describe the end state, not the sequence of changes to get there.

## Step 3: See the diff

```bash
wlite diff mydb.db schema.wlite
```

This compares the live database against the model and prints the SQL needed to
close the gap.

### Default output (human-readable)

If the database does not exist yet or is missing tables:

```sql
-- upgrade
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    email TEXT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

If the database already has the schema:

```
Schemas are identical.
```

If there are column differences:

```sql
-- upgrade
ALTER TABLE users ADD COLUMN bio TEXT;
```

If a table rebuild is required:

```sql
-- upgrade (table rebuild)
CREATE TABLE _staging_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    email TEXT NOT NULL,
    bio TEXT,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);
INSERT INTO _staging_users (id, username, email, created_at)
SELECT id, username, email, created_at FROM users;
DROP TABLE users;
ALTER TABLE _staging_users RENAME TO users;
```

### JSON output

Add `--json` to get structured output:

```bash
wlite diff mydb.db schema.wlite --json
```

```json
{
  "change_count": 2,
  "changes": [
    {
      "op": "ADD_TABLE",
      "table": "users",
      "safety": "SAFE",
      "detail": "create table"
    },
    {
      "op": "ADD_INDEX",
      "table": "users",
      "object": "idx_users_email",
      "safety": "SAFE",
      "detail": "CREATE INDEX idx_users_email ON users (email)"
    }
  ]
}
```

When no differences are found:

```json
{
  "change_count": 0,
  "changes": []
}
```

Each change object contains:

| Field | Description |
|-------|-------------|
| `op` | Operation type: `ADD_TABLE`, `DROP_TABLE`, `RENAME_TABLE`, `ADD_COLUMN`, `DROP_COLUMN`, `RENAME_COLUMN`, `ALTER_COLUMN`, `ADD_INDEX`, `DROP_INDEX`, `ALTER_INDEX`, `ADD_CHECK`, `DROP_CHECK`, `ADD_UNIQUE`, `DROP_UNIQUE`, `ADD_FKEY`, `DROP_FKEY`, `ALTER_TABLE_OPTIONS`, `ALTER_VIEW`, `ALTER_TRIGGER`, `REBUILD_TABLE` |
| `table` | The affected table name |
| `object` | The affected sub-object (column, index, etc.), if applicable |
| `safety` | One of `SAFE`, `REBUILD`, `DESTRUCTIVE`, `CONDITIONAL`, `IRREVERSIBLE` |
| `detail` | Human-readable description of the change |

The JSON format is useful for scripting and CI pipelines where you need to
inspect the diff programmatically.

## Step 4: Apply the migration

```bash
wlite migrate mydb.db schema.wlite
```

This applies the diff SQL to the database. It creates the `_wlite_migrations`
tracking table if it does not exist, computes a checksum of the migration, and
records it.

Output on success:

```
Migration applied successfully.
```

Output on error:

```
Migration failed: <error message>
```

The migration runs inside a transaction. If any SQL statement fails, the
transaction rolls back and the database is unchanged.

## Step 5: Verify with check

```bash
wlite check mydb.db schema.wlite
```

This verifies that the database schema matches the model. It prints a result
message and exits with code 0 if they match, 1 if they differ.

On success:

```
Schema check: OK
```

On failure:

```
Schema check: FAILED

  ADD TABLE             users  [SAFE]
  ADD COLUMN            users.bio  [SAFE]

2 change(s)
```

This is the primary command for CI validation. It does not modify the database.
It only reads the live schema and compares it against the model.

## Step 6: Export a snapshot

```bash
wlite snapshot mydb.db
```

`wlite snapshot` is an alias for `wlite inspect`. Both commands export the full
schema as JSON to stdout:

```json
{
  "tables": [
    {
      "name": "users",
      "columns": [
        {"name": "id", "type": "INTEGER", "not_null": false, "primary_key": true, "default": null, "collate": null},
        {"name": "username", "type": "TEXT", "not_null": true, "primary_key": false, "default": null, "collate": null},
        {"name": "email", "type": "TEXT", "not_null": true, "primary_key": false, "default": null, "collate": null},
        {"name": "created_at", "type": "DATETIME", "not_null": true, "primary_key": false, "default": "CURRENT_TIMESTAMP", "collate": null}
      ],
      "indexes": [],
      "foreign_keys": [],
      "check_constraints": []
    }
  ]
}
```

Save to a file:

```bash
wlite snapshot mydb.db > schema_snapshot.json
```

The snapshot is a point-in-time export of the database schema. It is useful for
documentation, comparison between environments, and debugging.

### JSON output

The snapshot command also supports `--json`:

```bash
wlite snapshot mydb.db --json
```

This produces identical output to the default format since the default output
is already JSON.

## Inspecting the schema

```bash
wlite inspect mydb.db
```

`wlite inspect` is the canonical form of the schema inspection command. `wlite
snapshot` is an alias that behaves identically. Prints the full schema of the
database in human-readable form:

```
Table: users
  id          INTEGER    PK
  username    TEXT       NOT NULL UNIQUE
  email       TEXT       NOT NULL
  created_at  DATETIME   NOT NULL DEFAULT CURRENT_TIMESTAMP
```

### JSON output

```bash
wlite inspect mydb.db --json
```

```json
{
  "tables": [
    {
      "name": "users",
      "columns": [
        {"name": "id", "type": "INTEGER", "pk": true, "not_null": false, "default": null, "collate": null},
        {"name": "username", "type": "TEXT", "pk": false, "not_null": true, "default": null, "collate": null},
        {"name": "email", "type": "TEXT", "pk": false, "not_null": true, "default": null, "collate": null},
        {"name": "created_at", "type": "DATETIME", "pk": false, "not_null": true, "default": "CURRENT_TIMESTAMP", "collate": null}
      ],
      "indexes": [],
      "foreign_keys": [],
      "check_constraints": []
    }
  ]
}
```

## Planning migrations

```bash
wlite plan mydb.db schema.wlite
```

Shows a human-readable migration plan without executing anything:

```
PLAN (2 steps)

  1. CREATE TABLE users — create table
  2. CREATE INDEX idx_users_email ON users (email) — create index
```

### JSON output

```bash
wlite plan mydb.db schema.wlite --json
```

```json
{
  "step_count": 2,
  "steps": [
    {
      "type": "CREATE_TABLE",
      "table": "users",
      "safety": "SAFE",
      "sql": "CREATE TABLE users (...)",
      "detail": "create table"
    },
    {
      "type": "CREATE_INDEX",
      "table": "users",
      "safety": "SAFE",
      "sql": "CREATE INDEX idx_users_email ON users (email)",
      "detail": "create index"
    }
  ]
}
```

Each step object contains:

| Field | Description |
|-------|-------------|
| `type` | Step type: `CREATE_TABLE`, `DROP_TABLE`, `RENAME_TABLE`, `ADD_COLUMN`, `DROP_COLUMN`, `RENAME_COLUMN`, `ALTER_COLUMN`, `REBUILD_TABLE`, `CREATE_INDEX`, `DROP_INDEX`, `ADD_CHECK`, `DROP_CHECK`, `ADD_UNIQUE`, `DROP_UNIQUE`, `ADD_FKEY`, `DROP_FKEY`, `CUSTOM` |
| `table` | The affected table name, if applicable |
| `safety` | One of `SAFE`, `REBUILD`, `DESTRUCTIVE`, `CONDITIONAL`, `IRREVERSIBLE` |
| `sql` | The SQL statement to execute, if applicable |
| `detail` | Human-readable description of the step |
| `non_atomic` | `true` if the step requires a table rebuild (non-atomic) |

The plan shows what wlite would do without doing it. Use this to review
migrations before applying them.

## Generating migration SQL files

```bash
wlite generate mydb.db schema.wlite
```

Generates the migration SQL and writes it to a file in the `migrations/`
directory. The file is named with a sequential index and a slug derived from the
migration name:

```
migrations/0001_migration.sql
```

You can provide a custom name with `--name`:

```bash
wlite generate mydb.db schema.wlite --name "add users table"
```

This produces:

```
migrations/0001_add_users_table.sql
```

The name is slugified: spaces and hyphens become underscores, and non-alphanumeric
characters are stripped. The sequential index is always `0001` for the first
migration.

Contents of the generated file:

```sql
-- wlite migration
-- name: add users table

-- upgrade

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    email TEXT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- rollback

DROP TABLE IF EXISTS users;
```

Generated files are plain SQL. You can review them, commit them to version
control, and apply them with any SQLite client. They are not required for the
wlite workflow but are useful for audit trails and manual review.

## Compiling models to .wlitem

```bash
wlite compile schema.wlite
```

Compiles the `.wlite` model to a `.wlitem` binary file. The output goes to
`schema.wlitem` by default. You can override the output path with `-o`:

```bash
wlite compile schema.wlite -o custom.wlitem
```

### JSON output

```bash
wlite compile schema.wlite --json
```

```json
{
  "status": "compiled",
  "output": "schema.wlitem",
  "size": 2048,
  "tables": 3,
  "columns": 12
}
```

The `.wlitem` format is a compiled binary representation of the model. It skips
parsing on subsequent loads, which is useful for large schemas where parsing
time matters. See the [`.wlitem` Format](#the-wlitem-compiled-model-format) section
below for the full specification.

## Schema hashing

```bash
wlite hash mydb.db
```

Computes an FNV-1a hash of the database schema and prints it:

```
a1b2c3d4e5f6a7b8
```

The hash is a 16-character hexadecimal string representing a 64-bit FNV-1a
digest. It captures the complete schema structure: table names, column names,
types, constraints, defaults, indexes, and more.

### What the hash covers

The hash is computed over:

- Schema version and table count
- For each table: name, column count, strict mode, without rowid flag
- For each column: name, type, not_null, primary_key, unique, autoincrement,
  default expression, collation, foreign key table and column
- Index count, names, and table references
- View count and trigger count

The hash does not cover:

- Row data
- Table statistics
- Triggers and views (beyond their count)

### Using hashes in CI

```bash
# Store the expected hash
EXPECTED="a1b2c3d4e5f6a7b8"

# Compute the actual hash
ACTUAL=$(wlite hash mydb.db)

# Compare
if [ "$EXPECTED" != "$ACTUAL" ]; then
    echo "Schema drift detected"
    exit 1
fi
```

The hash uses FNV-1a, not SHA-256. FNV-1a is fast, has excellent distribution,
and is sufficient for schema fingerprinting. There is no need for cryptographic
strength here.

## Querying the database

```bash
wlite query mydb.db "SELECT * FROM users"
```

Default output (tab-separated):

```
1	alice	alice@example.com	2026-09-04T12:00:00Z
2	bob	bob@example.com	2026-09-04T12:01:00Z
```

### JSON output

```bash
wlite query mydb.db "SELECT * FROM users" --json
```

```json
{
  "columns": ["id", "username", "email", "created_at"],
  "rows": [
    [1, "alice", "alice@example.com", "2026-09-04T12:00:00Z"],
    [2, "bob", "bob@example.com", "2026-09-04T12:01:00Z"]
  ],
  "row_count": 2
}
```

## Formatting the model

```bash
wlite format schema.wlite
```

Formats the model file with consistent indentation and ordering. This does not
change semantics, only whitespace. The formatted output is written to stdout,
not back into the file. Redirect to save:

```bash
wlite format schema.wlite > schema.wlite
```

Or write to a temporary file and replace:

```bash
wlite format schema.wlite > schema.wlite.tmp && mv schema.wlite.tmp schema.wlite
```

## The .wlitem compiled model format

The `.wlitem` format is a binary representation of a parsed `.wlite` model. It
is produced by `wlite compile` and consumed by `wlite_model_load_compiled` (C
API) or equivalent functions in language bindings.

### Why compile?

Parsing a `.wlite` file involves lexical analysis, tokenization, and recursive
descent parsing. For small models this takes microseconds. For large models with
hundreds of tables and thousands of columns, parsing can take milliseconds. The
`.wlitem` format skips parsing entirely and loads the schema directly into
memory.

### Binary layout

The file starts with a header, followed by per-table records:

```
Header:
  magic:          4 bytes ("WLIT" = 0x54494C57)
  version:        4 bytes (uint32, currently 1)
  model_name:     length-prefixed string
  model_version:  4 bytes (int32)
  table_count:    4 bytes (uint32)

Per table:
  name:           length-prefixed string
  flags:          1 byte (bit 0 = strict, bit 1 = without_rowid)
  comment:        length-prefixed string
  column_count:   4 bytes (uint32)
  columns:
    name:              length-prefixed string
    type_name:         length-prefixed string
    flags:             1 byte (bit 0 = not_null, bit 1 = primary_key,
                               bit 2 = unique, bit 3 = autoincrement,
                               bit 4 = is_generated, bit 5 = is_stored)
    default_expr:      length-prefixed string
    collate:           length-prefixed string
    generated_expr:    length-prefixed string
    fk_table:          length-prefixed string
    fk_column:         length-prefixed string
  pk_count:       4 bytes (uint32)
  pk_columns:     pk_count length-prefixed strings
  fk_count:       4 bytes (uint32)
  foreign_keys:
    column_count:    4 bytes (uint32)
    columns:         column_count length-prefixed strings
    ref_table:       length-prefixed string
    ref_column_count: 4 bytes (uint32)
    ref_columns:     ref_column_count length-prefixed strings
    on_delete:       1 byte (enum value)
    on_update:       1 byte (enum value)
  check_count:    4 bytes (uint32)
  checks:
    name:            length-prefixed string
    expression:      length-prefixed string
  unique_count:   4 bytes (uint32)
  uniques:
    name:            length-prefixed string
    column_count:    4 bytes (uint32)
    columns:         column_count length-prefixed strings
```

A length-prefixed string is stored as a 4-byte uint32 length followed by that
many bytes of UTF-8 data. An empty or NULL string has length 0 and no data
bytes.

### Example

Given this model:

```
model_config {
    name "blog"
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
}
```

The compiled `.wlitem` file contains:

- Header: `WLIT` magic, version 1, name "blog", version 1, 1 table
- Table "users": flags 0, no comment, 2 columns
- Column "id": type "INTEGER", flags 0x0A (primary_key + autoincrement), no
  default, no collation, no FK
- Column "name": type "TEXT", flags 0x01 (not_null), no default, no collation,
  no FK

### Loading compiled models

From C:

```c
wlite_model *model = NULL;
wlite_model_load_compiled("schema.wlitem", &model);
```

From Python:

```python
model = wlite.Model.load_compiled("schema.wlitem")
```

From Rust:

```rust
let model = Model::load_compiled("schema.wlitem")?;
```

The compiled model is loaded into the same in-memory representation as a parsed
`.wlite` file. There is no difference in behavior.

## The _wlite_migrations tracking table

When you run `wlite migrate`, wlite creates a tracking table in the database:

```sql
CREATE TABLE IF NOT EXISTS _wlite_migrations (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    checksum TEXT NOT NULL,
    applied_at INTEGER NOT NULL
);
```

### Columns

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER | Auto-incrementing migration identifier |
| `name` | TEXT | Name of the migration (typically the model name) |
| `checksum` | TEXT | FNV-1a hash of the migration SQL |
| `applied_at` | INTEGER | Unix timestamp when the migration was applied |

### How it works

1. Before applying a migration, wlite checks if a row with the same checksum
   already exists in `_wlite_migrations`.
2. If the checksum exists, the migration is skipped (idempotent).
3. If the checksum does not exist, the migration SQL runs inside a transaction.
4. After successful execution, a new row is inserted with the checksum and
   timestamp.
5. If the migration fails, the transaction rolls back and no row is inserted.

### Querying migration history

```sql
SELECT id, name, checksum, datetime(applied_at, 'unixepoch') as applied
FROM _wlite_migrations
ORDER BY id;
```

```
1|User|a1b2c3d4e5f6a7b8|2026-09-04 12:00:00
2|Post|f8e7d6c5b4a39281|2026-09-04 12:05:00
```

### Rolling back

```bash
wlite rollback mydb.db
```

This removes the most recent migration record from `_wlite_migrations`. It does
not reverse the SQL changes. It only marks the migration as not applied so that
the next `wlite migrate` will re-apply it.

## The --json flag

Several wlite commands support the `--json` flag for structured output:

| Command | --json behavior |
|---------|-----------------|
| `wlite inspect <db>` | JSON schema export |
| `wlite snapshot <db>` | JSON schema export (alias for inspect) |
| `wlite diff <db> <schema>` | JSON diff with changes array |
| `wlite plan <db> <schema>` | JSON plan with steps array |
| `wlite query <db> <sql>` | JSON rows and columns |
| `wlite compile <schema>` | JSON compilation metadata |

The `--json` flag must appear after all positional arguments:

```bash
# Correct
wlite diff mydb.db schema.wlite --json

# Incorrect (flag before arguments)
wlite diff --json mydb.db schema.wlite
```

All JSON output is valid JSON (one object per command). No trailing commas, no
comments, no JSONP. Parse it with any standard JSON library.

## CI/CD integration

### GitHub Actions

```yaml
name: Schema Validation

on: [push, pull_request]

jobs:
  schema-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install wlite
        run: |
          # Install from source or download binary
          make -C libwlite install
          make -C wlite

      - name: Check schema matches
        run: wlite check mydb.db schema.wlite

      - name: Validate model parses
        run: wlite compile schema.wlite --json
```

### Pre-commit hook

```bash
#!/bin/sh
# .git/hooks/pre-commit

# Check if schema.wlite changed
if git diff --cached --name-only | grep -q 'schema.wlite'; then
    # Validate the model parses
    wlite compile schema.wlite --json > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "schema.wlite failed to compile"
        exit 1
    fi

    # Check schema matches the database (if it exists)
    if [ -f mydb.db ]; then
        wlite check mydb.db schema.wlite
        if [ $? -ne 0 ]; then
            echo "Schema drift detected. Run 'wlite migrate mydb.db schema.wlite'"
            exit 1
        fi
    fi
fi
```

### Docker

```dockerfile
FROM alpine:3.20

RUN apk add --no-cache gcc make sqlite-dev git

# Build libwlite and wlite
COPY libwlite /src/libwlite
COPY wlite /src/wlite

RUN cd /src/libwlite && make install && cd /src/wlite && make

# Copy model
COPY schema.wlite /app/schema.wlite

# Migrate on container start
CMD ["sh", "-c", "wlite migrate /app/data.db /app/schema.wlite && exec your-app"]
```

### Makefile integration

```makefile
DB = mydb.db
SCHEMA = schema.wlite

.PHONY: migrate check diff snapshot

migrate: $(SCHEMA)
	wlite migrate $(DB) $(SCHEMA)

check: $(SCHEMA)
	wlite check $(DB) $(SCHEMA)

diff: $(SCHEMA)
	wlite diff $(DB) $(SCHEMA)

snapshot:
	wlite snapshot $(DB) > schema_snapshot.json

hash:
	wlite hash $(DB)
```

## YAML frontmatter

Every documentation page in this site uses YAML frontmatter. The frontmatter
block appears at the top of the file between `---` delimiters:

```yaml
---
title: Migration Workflow
description: Complete migration workflow from init to snapshot.
---
```

Frontmatter fields:

| Field | Required | Description |
|-------|----------|-------------|
| `title` | Yes | Page title used in navigation and meta tags |
| `description` | Yes | Page description used in meta tags and search |

The frontmatter is processed by the zensical documentation system. It is not
part of the rendered content.

## Complete workflow example

Here is the full sequence for a new project:

```bash
# 1. Initialize
wlite init

# 2. Edit schema.wlite (or write it programmatically)

# 3. See what needs to happen
wlite diff app.db schema.wlite

# 4. Apply the migration
wlite migrate app.db schema.wlite

# 5. Verify
wlite check app.db schema.wlite

# 6. Inspect the result
wlite inspect app.db

# 7. Query
wlite query app.db "SELECT * FROM users"

# 8. Export snapshot
wlite snapshot app.db > schema_snapshot.json

# 9. Check the hash
wlite hash app.db
```

For an existing database:

```bash
# Check what changed
wlite diff app.db schema.wlite

# Review the plan
wlite plan app.db schema.wlite

# Generate SQL file for review
wlite generate app.db schema.wlite

# Apply when ready
wlite migrate app.db schema.wlite

# Confirm
wlite check app.db schema.wlite
```

## CI validation pattern

The most common CI use case is verifying that the database schema matches the
model. This catches schema drift before it reaches production:

```bash
# Exit code 0 = schema matches
# Exit code 1 = schema differs
wlite check mydb.db schema.wlite
```

For richer output in CI logs:

```bash
# Show the diff
wlite diff mydb.db schema.wlite

# Show the hash for comparison
wlite hash mydb.db
```

The hash approach works well for detecting changes across commits. Store the
expected hash in a file or environment variable and compare it in CI:

```bash
# In CI
ACTUAL=$(wlite hash mydb.db)
EXPECTED=${SCHEMA_HASH:-""}

if [ "$EXPECTED" != "$ACTUAL" ]; then
    echo "Schema hash changed: $EXPECTED -> $ACTUAL"
    echo "Run 'wlite diff mydb.db schema.wlite' to see what changed"
    exit 1
fi
```

## Migration naming conventions

Generated migration files follow this pattern:

```
migrations/NNNN_<slug>.sql
```

- `NNNN` is a zero-padded sequential index (0001, 0002, 0003)
- `<slug>` is derived from the migration name with spaces and hyphens replaced
  by underscores and non-alphanumeric characters stripped

For example, `--name "add users table"` produces `0001_add_users_table.sql`.
The default name (when `--name` is omitted) is `migration`, producing
`0001_migration.sql`.

## Error handling

wlite returns specific exit codes:

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Schema differs, migration failed, or general error |

Error messages go to stderr. Normal output goes to stdout. This makes it safe
to parse stdout in scripts:

```bash
SQL=$(wlite diff app.db schema.wlite)
if [ $? -eq 0 ] && [ -z "$SQL" ]; then
    echo "Schema is up to date"
elif [ $? -eq 0 ]; then
    echo "Migration needed:"
    echo "$SQL"
else
    echo "Error checking schema"
fi
```

## Thread safety

wlite operations are not thread-safe at the database level. SQLite connections
should not be shared between threads without external synchronization. Each
thread should open its own database connection.

Model loading is thread-safe. You can parse a `.wlite` file in one thread and
use it from another, as long as you do not modify it concurrently.

## Memory management

The C API requires explicit resource management:

```c
wlite_model *model = NULL;
wlite_db *db = NULL;

wlite_model_load_file("schema.wlite", &model);
wlite_open("app.db", &db);

wlite_migrate(db, model);

// Clean up in reverse order
wlite_close(db);
wlite_model_free(model);
```

Language bindings (Python, Rust, C++) handle cleanup automatically through
their respective RAII or garbage collection mechanisms.

## Next steps

- Read the [.wlite Grammar](grammar.md) for the complete model specification
- Browse the [C API Reference](c-api.md) for programmatic access
- Understand [Migration Internals](architecture/migration-internals.md) for
  how diffs become SQL
- Explore [SQLite Patterns](architecture/sqlite-patterns.md) for rebuild
  mechanics
