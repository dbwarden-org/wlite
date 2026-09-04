---
title: C++ Binding
description: Header-only C++ wrapper for wlite. Modern C++ access to libwlite.
---

# C++ Binding

The C++ binding is a header-only wrapper around the libwlite C API. It provides RAII types and C++ idioms while delegating to the C ABI.

## Usage

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

## Types

| C++ Type | C Equivalent | Description |
|----------|--------------|-------------|
| `wlite::Database` | `wlite_db` | Open database connection |
| `wlite::Model` | `wlite_model` | Loaded .wlite schema |
| `wlite::Statement` | `wlite_stmt` | Prepared SQL statement |
| `wlite::Transaction` | `wlite_tx` | Active transaction |

## RAII

All types use RAII for resource management. Resources are automatically freed when objects go out of scope:

```cpp
{
    auto db = wlite::Database::open("app.db");
    // db is closed automatically when leaving scope
}
```

## Error Handling

Errors throw `wlite::Error`:

```cpp
try {
    auto db = wlite::Database::open("app.db");
} catch (const wlite::Error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## Thread Safety

Models are immutable after loading and can be shared via `std::shared_ptr`. Database connections are not thread-safe; use one per thread.
