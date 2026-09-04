---
title: wlite - SQLite Schema and Migration Toolkit
description: A lightweight, declarative SQLite schema toolkit inspired by dbwarden. CLI and language bindings for C++, Rust, Python, Go, C#, and Zig.
---

<p align="center">
  <strong style="font-size: 2.5em;">wlite</strong>
</p>
<p align="center">
  <em>Declarative SQLite schema management, without the overhead.</em>
</p>
<p align="center">
  <a href="https://github.com/dbwarden-org/wlite/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-10AC84?style=for-the-badge" alt="License">
  </a>
  <a href="https://github.com/dbwarden-org/wlite">
    <img src="https://img.shields.io/badge/GitHub-dbwarden--org%2Fwlite-181717?logo=github&style=for-the-badge" alt="GitHub">
  </a>
</p>

---

wlite is a lightweight SQLite schema and migration toolkit. It provides a CLI and language bindings (C++, Rust, Python, Go, C#, Zig) for [libwlite](https://github.com/dbwarden-org/libwlite). Define your database in `.wlite`, then use it from any supported language.

## Why wlite exists

[dbwarden](https://github.com/dbwarden-org/dbwarden) is a full-featured declarative schema compiler for SQLAlchemy. It is powerful, but it carries the weight of the Python ecosystem: SQLAlchemy, async drivers, plugin systems, multi-database orchestration.

Not every project needs that. Embedded applications, CLI tools, TUI interfaces, and small services often just need SQLite with proper schema management. wlite exists to fill that gap: the same declarative philosophy as dbwarden, stripped down to what SQLite and C can do alone.

**wlite is dbwarden for projects that do not need the dbwarden machinery.**

## Philosophy

wlite follows the dbwarden principle: **your schema is the source of truth, not migration scripts**. You declare the tables, fields, and constraints you want in a `.wlite` model file. The tooling computes the diff against the live database and generates plain SQL to close the gap.

This is the same approach dbwarden takes with SQLAlchemy models, applied to SQLite specifically. No imperative change scripts to write, maintain, or debug. No hidden ORM behavior. Just a model and the SQL it produces.

The SQLite3 backend in dbwarden (table rebuilds, collapse logic, type normalization, default handling, constraint diffing) is the reference implementation. libwlite mirrors it exactly. A CI workflow enforces behavioral sync between the two projects.

## At a glance

- Declarative `.wlite` model format for schema definition
- CLI for init, diff, migrate, query, and more
- Language bindings for C++, Rust, Python, Go, C#, and Zig
- All bindings go through the libwlite C ABI
- SQLite-native: table rebuilds, type normalization, constraint diffing
- Mirrors dbwarden's SQLite3 backend exactly
- Small binaries, fast builds, zero runtime dependencies beyond SQLite3

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
```

### Python

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

# Prepared statements
stmt = db.prepare("SELECT * FROM users WHERE id = ?")
stmt.bind(1, 42)
while stmt.step():
    print(stmt.column_text(0))
stmt.finalize()
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

    Ok(())
}
```

### C++

```cpp
#include <wlite/wlite.hpp>

int main() {
    auto model = wlite::Model::load("app.wlite");
    auto db = wlite::Database::open("app.db");

    db.migrate(model);

    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        std::cout << name << std::endl;
    }

    return 0;
}
```

### C

```c
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;

    wlite_model_load_file("app.wlite", &model);
    wlite_open("app.db", &db);
    wlite_migrate(db, model);

    wlite_stmt *stmt;
    wlite_prepare(db, "SELECT * FROM users", &stmt);
    while (wlite_step(stmt) == WLITE_OK)
        printf("%s\n", wlite_column_text(stmt, 0));
    wlite_stmt_finalize(stmt);

    wlite_close(db);
    wlite_model_free(model);
    return 0;
}
```

### C\#

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");
db.Migrate(model);

using var stmt = db.Prepare("SELECT * FROM users");
while (stmt.Step())
{
    Console.WriteLine(stmt.ColumnText(0));
}
```

## CLI commands

| Command | Description |
|---------|-------------|
| `wlite init` | Create schema.wlite and migrations/ |
| `wlite diff <db> <schema>` | Compare database against schema |
| `wlite plan <db> <schema>` | Show migration plan |
| `wlite generate <db> <schema>` | Generate migration SQL |
| `wlite compile <schema>` | Compile .wlite to JSON |
| `wlite query <db> <sql>` | Execute SQL query |
| `wlite check <db> <schema>` | Verify schema matches |
| `wlite inspect <db>` | Show database schema |
| `wlite snapshot <db>` | Export schema |
| `wlite hash <db>` | Show schema hash |
| `wlite format <schema>` | Format schema.wlite |
| `wlite version` | Show version |

## Language bindings

| Language | Package | Status |
|----------|---------|--------|
| C | libwlite (separate repo) | Complete |
| C++ | Header-only wrapper | Complete |
| Rust | `wlite` crate | Complete |
| Python | `wlite` on PyPI | Complete |
| Go | cgo binding | Complete |
| C# | `wlite` on NuGet | Complete |
| Zig | C interop | Structured |

## Relationship to dbwarden

dbwarden is a declarative schema compiler for Python and SQLAlchemy. It is feature-rich: multi-database support, plugin systems, async drivers, seed management, and more.

wlite is what happens when you take dbwarden's SQLite3 engine and remove everything except SQLite. The table rebuild algorithms, type normalization, collapse logic, and constraint diffing that make dbwarden's SQLite support production-grade are implemented in libwlite as a standalone C library.

A CI workflow keeps libwlite's behavior synchronized with dbwarden's SQLite backend. When dbwarden improves how it handles a type, default, or constraint, those improvements flow into libwlite automatically.

The result: a SQLite toolkit that carries dbwarden's quality without dbwarden's weight.

## Next steps

- Read the [Architecture](architecture.md) overview
- Learn the [.wlite Grammar](grammar.md)
- Browse the [C API Reference](c-api.md)
- Explore language [Bindings](bindings/rust.md)
