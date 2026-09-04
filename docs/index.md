---
title: wlite - SQLite Schema and Migration Toolkit
description: CLI and language bindings for libwlite. Define your database in .wlite, then use it from C++, Rust, Python, Go, and Zig.
---

<p align="center">
  <strong style="font-size: 2.5em;">wlite</strong>
</p>
<p align="center">
  <em>A tiny SQLite schema and migration toolkit.</em>
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

wlite provides a command-line tool and language bindings for C++, Rust, Python, Go, and Zig that wrap [libwlite](https://github.com/dbwarden-org/libwlite). Define your database in `.wlite`, then use it from any supported language.

## At a glance

- Declarative `.wlite` model format for schema definition
- CLI for init, diff, migrate, query, and more
- Language bindings for C++, Rust, Python, Go, and Zig
- All bindings go through the libwlite C ABI
- SQLite-native: table rebuilds, type normalization, constraint diffing
- Follows [dbwarden](https://github.com/dbwarden-org/dbwarden) patterns

## Why wlite

Most SQLite schema tools require you to write imperative migration scripts. wlite is declarative: you define the desired schema in a `.wlite` model file, and the tooling derives the migration SQL for you.

- No migration scripts to write or maintain
- Plain SQL output: reviewable, committable, executable anywhere
- Cross-language: use the same schema from C++, Rust, Python, Go, or Zig
- Powered by libwlite, a small C library that mirrors dbwarden's SQLite backend

## Quick start

### CLI

```bash
# Build (requires libwlite installed)
cd libwlite && make install
cd ../wlite && make

# Create a model
cat > app.wlite << 'EOF'
model User {
    table "users"
    field id integer { primary_key autoincrement }
    field name text { not_null }
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

wlite follows [dbwarden](https://github.com/dbwarden-org/dbwarden) and inherits its SQLite3 development patterns. The SQLite backend in dbwarden (table rebuilds, collapse logic, type normalization, default handling, constraint diffing) is the reference implementation that libwlite mirrors. A CI workflow enforces that libwlite's SQLite behavior stays synchronized with dbwarden's SQLite backend.

## Next steps

- Read the [Architecture](architecture.md) overview
- Browse the [C API Reference](c-api.md)
- Learn the [.wlite Grammar](grammar.md)
- Explore language [Bindings](bindings/rust.md)
