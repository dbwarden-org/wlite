# wlite

Declarative SQLite schema management, without the overhead.

## Why wlite exists

[dbwarden](https://github.com/dbwarden-org/dbwarden) is a full-featured declarative schema compiler for SQLAlchemy. It is powerful, but it carries the weight of the Python ecosystem: SQLAlchemy, async drivers, plugin systems, multi-database orchestration.

Not every project needs that. Embedded applications, CLI tools, TUI interfaces, and small services often just need SQLite with proper schema management. wlite exists to fill that gap: the same declarative philosophy as dbwarden, stripped down to what SQLite and C can do alone.

**wlite is dbwarden for projects that do not need the dbwarden machinery.**

## What It Does

wlite provides a command-line tool and bindings for C++, Rust, Python, Go, C#, and Zig that wrap libwlite. Define your database in `.wlite`, then use it from any supported language.

## Philosophy

wlite follows the dbwarden principle: **your schema is the source of truth, not migration scripts**. You declare the tables, fields, and constraints you want in a `.wlite` model file. The tooling computes the diff against the live database and generates plain SQL to close the gap.

The SQLite3 backend in dbwarden (table rebuilds, collapse logic, type normalization, default handling, constraint diffing) is the reference implementation. libwlite mirrors it exactly. A CI workflow enforces behavioral sync between the two projects.

## Quick Start

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
}
EOF

# Initialize and migrate
wlite init
wlite diff app.db app.wlite
wlite migrate app.db app.wlite

# Query
wlite query app.db "SELECT * FROM users"
```

### Python

```python
import wlite

model = wlite.Model.load("app.wlite")
db = wlite.Database.open("app.db")
db.migrate(model)
rows = db.query("SELECT * FROM users")
for row in rows:
    print(row["name"])
```

### Rust

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;
    db.migrate(&model)?;
    Ok(())
}
```

### C++

```cpp
#include <wlite/wlite.hpp>

auto model = wlite::Model::load("app.wlite");
auto db = wlite::Database::open("app.db");
db.migrate(model);
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
```

## Build

```bash
make              # builds wlite CLI
```

Requires: libwlite installed, C11 compiler, SQLite3 library.

## .wlite Model Format

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

    field email text

    field active boolean {
        not_null
        default true
    }
}
```

## CLI Commands

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

## Language Bindings

| Language | Package | Status |
|----------|---------|--------|
| C | libwlite (separate repo) | Complete |
| C++ | Header-only wrapper | Complete |
| Rust | `wlite` crate | Complete |
| Python | `wlite` on PyPI | Complete |
| Go | cgo binding | Complete |
| C# | `wlite` on NuGet | Complete |
| Zig | C interop | Structured |

## Repository Layout

```
wlite/
  cli/main.c                 CLI entry point
  bindings/
    cpp/include/wlite/       C++ header-only wrapper
    rust/                    Rust FFI binding
    python/wlite/            Python ctypes binding
    go/                      Go cgo binding
    csharp/                  C# P/Invoke binding
    zig/                     Zig C interop
  tests/conformance.c        Cross-language conformance tests
  docs/                      API reference, grammar, architecture
```

## Relationship to dbwarden

dbwarden is a declarative schema compiler for Python and SQLAlchemy. It is feature-rich: multi-database support, plugin systems, async drivers, seed management, and more.

wlite is what happens when you take dbwarden's SQLite3 engine and remove everything except SQLite. The table rebuild algorithms, type normalization, collapse logic, and constraint diffing that make dbwarden's SQLite support production-grade are implemented in libwlite as a standalone C library.

A CI workflow keeps libwlite's behavior synchronized with dbwarden's SQLite backend. When dbwarden improves how it handles a type, default, or constraint, those improvements flow into libwlite automatically.

The result: a SQLite toolkit that carries dbwarden's quality without dbwarden's weight.

## License

MIT
