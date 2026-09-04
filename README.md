# wlite

A tiny SQLite schema and migration toolkit. CLI and language bindings for libwlite.

## What It Does

wlite provides a command-line tool and bindings for C++, Rust, Python, Go, and Zig that wrap libwlite. Define your database in `.wlite`, then use it from any supported language.

## Relationship to dbwarden

wlite follows [dbwarden](https://github.com/dbwarden-org/dbwarden) and inherits its SQLite3 development patterns. The SQLite backend in dbwarden (table rebuilds, collapse logic, type normalization, default handling, constraint diffing) is the reference implementation that libwlite mirrors. A CI workflow enforces that libwlite's SQLite behavior stays synchronized with dbwarden's SQLite backend.

## Quick Start

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
    zig/                     Zig C interop
  tests/conformance.c        Cross-language conformance tests
  examples/                  Example applications
  docs/                      API reference, grammar, architecture
```

## License

MIT
