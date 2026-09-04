---
title: wlite - SQLite Schema and Migration Toolkit
description: A lightweight, declarative SQLite schema toolkit inspired by dbwarden. CLI and language bindings for C++, Rust, Python, C, C#, and more.
---

# wlite

Declarative SQLite schema management, without the overhead.

<a href="https://github.com/dbwarden-org/wlite/blob/main/LICENSE">
  <img src="https://img.shields.io/badge/License-MIT-10AC84?style=for-the-badge" alt="License">
</a>
<a href="https://github.com/dbwarden-org/wlite">
  <img src="https://img.shields.io/badge/GitHub-dbwarden--org%2Fwlite-181717?logo=github&style=for-the-badge" alt="GitHub">
</a>
<a href="https://crates.io/crates/wlite">
  <img src="https://img.shields.io/crates/v/wlite?style=for-the-badge&logo=rust" alt="Rust Crate">
</a>
<a href="https://pypi.org/project/wlite/">
  <img src="https://img.shields.io/pypi/v/wlite?style=for-the-badge&logo=python&logoColor=white" alt="PyPI">
</a>
<a href="https://www.nuget.org/packages/wlite">
  <img src="https://img.shields.io/nuget/v/wlite?style=for-the-badge&logo=nuget" alt="NuGet">
</a>

---

wlite is a lightweight SQLite schema and migration toolkit. It provides a CLI and
language bindings (C, C++, Rust, Python, C#, and more) for
[libwlite](https://github.com/dbwarden-org/libwlite). Define your database in
`.wlite`, then use it from any supported language.

The core engine is written in C as a standalone library with zero runtime
dependencies beyond SQLite3 itself. Every language binding goes through the same
C ABI, which means behavior is identical whether you call it from Rust, Python,
C++, or C#.

## Why wlite exists

[dbwarden](https://github.com/dbwarden-org/dbwarden) is a full-featured
declarative schema compiler for SQLAlchemy. It is powerful, but it carries the
weight of the Python ecosystem: SQLAlchemy, async drivers, plugin systems,
multi-database orchestration.

Not every project needs that. Many projects live in a different world entirely:

**Embedded applications.** Firmware, IoT devices, and embedded Linux systems
often use SQLite as their primary data store. These environments have limited
memory, no package manager, and no Python runtime. They need a schema toolkit
that compiles to C and links directly into the application binary. wlite gives
embedded developers a declarative schema workflow without requiring a build
system that can host Python.

**CLI tools.** Command-line utilities that ship with a database benefit from
schema management that runs as a single binary. A developer tool that creates a
SQLite database on first run can use `wlite migrate` in a shell script or embed
the C API directly. There is no interpreter to locate, no virtual environment to
activate, and no dependency to install on the user's machine.

**TUI interfaces.** Terminal user interfaces built with libraries like Bubble Tea,
Ratatui, or ncurses often need a local database for state persistence. These
applications are distributed as static binaries. Linking against libwlite means
the database schema is managed in the same language as the rest of the
application, with no foreign runtime required.

**Small services.** Not every backend is a Django monolith. Lightweight HTTP
servers, worker processes, and background daemons that use SQLite can benefit
from schema management without pulling in SQLAlchemy and its transitive
dependencies. wlite's Python binding is thin enough for these cases, and its C
binding is thin enough for everything else.

**Build-time schema generation.** Some projects generate schema SQL as part of
their build process. wlite can compile a `.wlite` model to raw SQL that gets
bundled into a deployment artifact. This is useful for projects that want
declarative schema definitions but do not want to run a migration tool at
runtime.

The common thread is simplicity. wlite exists for projects that want the
declarative philosophy of dbwarden without the weight of the dbwarden machinery.

**wlite is dbwarden for projects that do not need the dbwarden machinery.**

## Philosophy

wlite follows the dbwarden principle: **your schema is the source of truth, not
migration scripts**. You declare the tables, fields, and constraints you want in
a `.wlite` model file. The tooling computes the diff against the live database
and generates plain SQL to close the gap.

### Declarative vs imperative

Traditional migration tools take an imperative approach. You write a sequence of
`ALTER TABLE`, `CREATE TABLE`, and `DROP COLUMN` statements by hand. Each
migration is a snapshot of a transformation, and the tool applies them in order.
This has several costs:

- You must reason about the current state of the database to write the next
  migration correctly.
- Migrations interact. A column rename in migration 3 might conflict with a
  column add in migration 5 if you are not careful.
- Rollback logic is separate code that must be maintained in parallel.
- CI must test every migration path, not just the final desired state.

The declarative approach is different. You describe the end state:

```
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

Then wlite compares that model against the live database and generates the
minimal SQL to reach the desired state. If the table does not exist, it creates
it. If columns are missing, it adds them. If types have changed, it rebuilds the
table. You never write `ALTER TABLE` by hand.

This means:

- The model file is always self-contained. You can read it to understand the
  complete schema.
- There are no ordering concerns. The diff algorithm computes what needs to
  change regardless of how you got to the current state.
- Rollback is implicit. To go back, you revert the model file and run migrate
  again.
- CI tests the desired state, not a chain of historical transformations.

### The same quality as dbwarden

The SQLite3 backend in dbwarden handles table rebuilds, collapse logic, type
normalization, default handling, and constraint diffing. This is the reference
implementation. libwlite mirrors it exactly. A CI workflow enforces behavioral
sync between the two projects.

When dbwarden improves how it handles a type, default, or constraint, those
improvements flow into libwlite. When libwlite finds an edge case in SQLite
schema diffing, the fix propagates back to dbwarden. The two projects share a
common core of schema management logic, expressed in different languages for
different audiences.

## At a glance

- Declarative `.wlite` model format for schema definition
- CLI for init, diff, migrate, query, plan, generate, and more
- Language bindings for C, C++, Rust, Python, C#, and Zig
- All bindings go through the libwlite C ABI for consistent behavior
- SQLite-native: table rebuilds, type normalization, constraint diffing
- Mirrors dbwarden's SQLite3 backend exactly
- Small binaries, fast builds, zero runtime dependencies beyond SQLite3
- MIT licensed, suitable for commercial and open source projects
- Schema diffing produces minimal, idempotent SQL
- Prepared statement support in all language bindings
- Transaction support across all interfaces
- Schema hashing for integrity verification
- Snapshot export for schema documentation

## Why wlite

### Compared to raw SQLite

Raw SQLite gives you full control but no schema management. You write DDL
statements by hand and track them in files or scripts. There is no diffing, no
validation, and no guarantee that your DDL matches what you think the schema
looks like. wlite adds declarative schema management on top of SQLite without
changing the database engine itself.

### Compared to SQLAlchemy + Alembic

Alembic is the standard migration tool for SQLAlchemy. It works well, but it
requires SQLAlchemy, a Python environment, and a migration chain that must be
carefully maintained. wlite does not require SQLAlchemy, does not require Python
(if you use the C or C++ bindings), and does not maintain a migration chain. You
describe the desired state and wlite computes the diff.

### Compared to other SQLite migration tools

Many SQLite migration tools are thin wrappers around hand-written SQL. They
track which migrations have been applied and skip ones that have already run.
They do not diff your desired schema against the live schema. wlite performs a
full schema diff, including type normalization, default handling, and constraint
analysis. It produces minimal SQL regardless of the current state.

### Compared to no schema management at all

The most common approach to SQLite schema management is no approach at all.
Developers write `CREATE TABLE IF NOT EXISTS` and hope for the best. Columns
are added with `ALTER TABLE ... ADD COLUMN` and never validated against a
reference. This works until it does not, and then debugging is painful. wlite
gives you a single source of truth that can be validated, versioned, and tested.

## Quick start

### CLI

```bash
# Build (requires libwlite installed)
cd libwlite && make install
cd ../wlite && make

# Create a model
cat > app.wlite << 'EOF'
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

    field email text

    field created_at datetime {
        not_null
        default CURRENT_TIMESTAMP
    }
}
EOF

# Initialize the project
wlite init

# See what migration is needed
wlite diff app.db app.wlite

# Apply the migration
wlite migrate app.db app.wlite

# Query
wlite query app.db "SELECT * FROM users"

# Inspect the current schema
wlite inspect app.db

# Check schema matches
wlite check app.db app.wlite

# Generate migration SQL without applying
wlite generate app.db app.wlite

# Export schema as snapshot
wlite snapshot app.db > schema_snapshot.json

# Show schema hash
wlite hash app.db

# Format the model file
wlite format app.wlite
```

### Python

```python
import wlite

# Load the model and open the database
model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")

# Migrate the database to match the model
db.migrate(model)

# Query rows
rows = db.query("SELECT * FROM users")
for row in rows:
    print(row["username"], row["email"])

# Prepared statements with parameters
stmt = db.prepare("SELECT * FROM users WHERE id = ?")
stmt.bind(1, 42)
while stmt.step():
    print(stmt.column_text(0))
stmt.finalize()

# Check if schema matches
if db.check(model):
    print("Schema is up to date")
else:
    print("Schema drift detected")

# Get the diff as SQL
sql = db.diff(model)
print(sql)

# Transactions
with db.transaction():
    db.execute("INSERT INTO users (username, email) VALUES (?, ?)", "alice", "alice@example.com")
    db.execute("INSERT INTO users (username, email) VALUES (?, ?)", "bob", "bob@example.com")

# Close the database
db.close()
```

### Rust

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    // Migrate
    db.migrate(&model)?;

    // Query
    let rows = db.query("SELECT * FROM users")?;
    for row in rows {
        println!("{}", row.get::<String>(0)?);
    }

    // Prepared statements
    let mut stmt = db.prepare("SELECT * FROM users WHERE id = ?")?;
    stmt.bind(1, 42)?;
    while stmt.step()? {
        println!("{}", stmt.column_text(0)?);
    }

    // Check schema
    if db.check(&model)? {
        println!("Schema is up to date");
    }

    // Transaction
    let tx = db.transaction()?;
    tx.execute("INSERT INTO users (username, email) VALUES (?, ?)", ("alice", "alice@example.com"))?;
    tx.execute("INSERT INTO users (username, email) VALUES (?, ?)", ("bob", "bob@example.com"))?;
    tx.commit()?;

    Ok(())
}
```

### C++

```cpp
#include <wlite/wlite.hpp>
#include <iostream>

int main() {
    auto model = wlite::Model::load("app.wlite");
    auto db = wlite::Database::open("app.db");

    // Migrate
    db.migrate(model);

    // Query
    auto rows = db.query("SELECT * FROM users");
    for (auto& row : rows) {
        std::cout << row.get<std::string>(0) << std::endl;
    }

    // Prepared statements
    auto stmt = db.prepare("SELECT * FROM users WHERE id = ?");
    stmt.bind(1, 42);
    while (stmt.step()) {
        std::cout << stmt.column_text(0) << std::endl;
    }

    // Check schema
    if (db.check(model)) {
        std::cout << "Schema is up to date" << std::endl;
    }

    // Transaction
    auto tx = db.transaction();
    tx.execute("INSERT INTO users (username, email) VALUES (?, ?)", "alice", "alice@example.com");
    tx.execute("INSERT INTO users (username, email) VALUES (?, ?)", "bob", "bob@example.com");
    tx.commit();

    return 0;
}
```

### C

```c
#include <wlite/wlite.h>
#include <stdio.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;

    // Load model and open database
    wlite_model_load_file("app.wlite", &model);
    wlite_open("app.db", &db);

    // Migrate
    wlite_migrate(db, model);

    // Query
    wlite_stmt *stmt;
    wlite_prepare(db, "SELECT * FROM users", &stmt);
    while (wlite_step(stmt) == WLITE_OK)
        printf("%s\n", wlite_column_text(stmt, 0));
    wlite_stmt_finalize(stmt);

    // Check schema
    int matches = 0;
    wlite_check(db, model, &matches);
    if (matches)
        printf("Schema is up to date\n");
    else
        printf("Schema drift detected\n");

    // Transaction
    wlite_exec(db, "BEGIN TRANSACTION");
    wlite_exec(db, "INSERT INTO users (username, email) VALUES ('alice', 'alice@example.com')");
    wlite_exec(db, "INSERT INTO users (username, email) VALUES ('bob', 'bob@example.com')");
    wlite_exec(db, "COMMIT");

    // Cleanup
    wlite_close(db);
    wlite_model_free(model);
    return 0;
}
```

### C\#

```csharp
using Wlite;

// Load model and open database
using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

// Migrate
db.Migrate(model);

// Query rows
using var stmt = db.Prepare("SELECT * FROM users");
while (stmt.Step())
{
    Console.WriteLine($"{stmt.ColumnText(0)} {stmt.ColumnText(1)}");
}

// Prepared statements with parameters
using var findStmt = db.Prepare("SELECT * FROM users WHERE id = ?");
findStmt.Bind(1, 42);
while (findStmt.Step())
{
    Console.WriteLine(findStmt.ColumnText(0));
}

// Check schema
if (db.Check(model))
{
    Console.WriteLine("Schema is up to date");
}

// Transaction
using var tx = db.Transaction();
tx.Execute("INSERT INTO users (username, email) VALUES (?, ?)", "alice", "alice@example.com");
tx.Execute("INSERT INTO users (username, email) VALUES (?, ?)", "bob", "bob@example.com");
tx.Commit();
```

## CLI commands

| Command | Description |
|---------|-------------|
| `wlite init` | Create `schema.wlite` and `migrations/` directory in the current project. Seeds the model file with a `model_config` block and a starter model. |
| `wlite diff <db> <schema>` | Compare the live database against the schema model. Prints the SQL needed to bring the database into alignment. Returns a non-zero exit code if the schemas differ. |
| `wlite plan <db> <schema>` | Show a human-readable migration plan. Describes what will be created, altered, or dropped without executing any changes. |
| `wlite generate <db> <schema>` | Generate migration SQL and write it to a file in `migrations/`. The file is timestamped and named with a sequential index. |
| `wlite compile <schema>` | Compile a `.wlite` model to JSON. Useful for debugging the parsed representation of your schema definition. |
| `wlite query <db> <sql>` | Execute an arbitrary SQL query against the database and print the results. Supports SELECT, INSERT, UPDATE, and DDL statements. |
| `wlite check <db> <schema>` | Verify that the database schema matches the model. Exits with code 0 if they match, 1 if they differ. Useful for CI validation. |
| `wlite inspect <db>` | Print the full schema of the database including tables, columns, types, constraints, and indexes. |
| `wlite snapshot <db>` | Export the database schema as a JSON snapshot. Can be saved to a file for documentation or comparison purposes. |
| `wlite hash <db>` | Compute and display a SHA-256 hash of the database schema. Useful for detecting schema changes in CI pipelines. |
| `wlite format <schema>` | Format a `.wlite` model file with consistent indentation and ordering. Does not change semantics, only whitespace. |
| `wlite version` | Print the version of wlite and the underlying libwlite library. |

## Language bindings

| Language | Package | Status | Notes |
|----------|---------|--------|-------|
| C | [libwlite](https://github.com/dbwarden-org/libwlite) | Complete | Core library. All other bindings go through this ABI. |
| C++ | Header-only wrapper | Complete | Wraps the C API with RAII types and standard library containers. |
| Rust | [`wlite` on crates.io](https://crates.io/crates/wlite) | Complete | Idiomatic Rust API with `Result` types and iterator support. |
| Python | [`wlite` on PyPI](https://pypi.org/project/wlite/) | Complete | Thin wrapper around libwlite. Works with Python 3.8+. |
| C# | [`wlite` on NuGet](https://www.nuget.org/packages/wlite) | Complete | P/Invoke binding with `IDisposable` support. Targets .NET 6+. |
| Go | cgo binding | Complete | Uses cgo to call libwlite directly. |
| Zig | C interop | Structured | Uses Zig's C interop to call libwlite. API design in progress. |

## Relationship to dbwarden

dbwarden is a declarative schema compiler for Python and SQLAlchemy. It is
feature-rich: multi-database support, plugin systems, async drivers, seed
management, and more.

wlite is what happens when you take dbwarden's SQLite3 engine and remove
everything except SQLite. The table rebuild algorithms, type normalization,
collapse logic, and constraint diffing that make dbwarden's SQLite support
production-grade are implemented in libwlite as a standalone C library.

### Shared core logic

The schema diffing engine is the same in both projects. When dbwarden handles a
`TEXT` column with a `DEFAULT ''` constraint, libwlite handles it identically.
When dbwarden rebuilds a table to change a column type, libwlite performs the
same rebuild with the same safety guarantees. The implementation language is
different but the algorithm is the same.

### CI synchronization

A CI workflow keeps libwlite's behavior synchronized with dbwarden's SQLite
backend. The workflow runs identical test suites against both implementations
and fails if the outputs diverge. This means improvements to dbwarden's schema
management automatically benefit wlite, and fixes in wlite propagate back to
dbwarden.

### When to use which

Use dbwarden when you need SQLAlchemy integration, multi-database support,
plugin extensibility, or async operation. Use wlite when you need SQLite schema
management without Python, without SQLAlchemy, or without the full dbwarden
runtime. Use wlite when your target environment is embedded, compiled, or
constrained in a way that makes Python impractical.

The two projects are complementary, not competing. They solve the same problem
for different audiences with different constraints.

## Next steps

- Read the [Architecture](architecture/index.md) overview
- Learn the [.wlite Grammar](grammar.md)
- Browse the [C API Reference](c-api.md)
- Explore language [Bindings](bindings/rust.md)
- Understand [SQLite Patterns](architecture/sqlite-patterns.md)
- Join the discussion on [GitHub](https://github.com/dbwarden-org/wlite)
