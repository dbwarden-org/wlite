---
title: C++ Binding
description: Header-only C++ wrapper for libwlite.
---

# C++ Binding

The C++ binding is a header-only wrapper around the libwlite C API. It provides RAII types, exception-based error handling, and C++ idioms while delegating to the C ABI.

## Usage

Include the header and link against libwlite:

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
    // use db...
} // db is closed automatically
```

This eliminates manual cleanup and prevents resource leaks.

## Database operations

```cpp
auto db = wlite::Database::open("app.db");

// Execute DDL/DML
db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
db.execute("INSERT INTO test (name) VALUES ('hello')");

// Prepare and query
auto stmt = db.prepare("SELECT * FROM test WHERE id = ?");
stmt.bind(1, 1LL);
while (stmt.step()) {
    auto name = stmt.column_text(0);
    auto id = stmt.column_int64(0);
    std::cout << id << ": " << name << std::endl;
}
```

## Prepared statements

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
stmt.bind(1, "Alice");
stmt.bind(2, "alice@example.com");
stmt.step();
stmt.reset();

stmt.bind(1, "Bob");
stmt.bind(2, "bob@example.com");
stmt.step();
```

## Transactions

```cpp
auto tx = db.begin();

db.execute("INSERT INTO users (name) VALUES ('Alice')");
db.execute("INSERT INTO users (name) VALUES ('Bob')");

if (error_occurred) {
    tx.rollback();
} else {
    tx.commit();
}
```

## Error handling

Errors throw `wlite::Error`:

```cpp
try {
    auto db = wlite::Database::open("app.db");
} catch (const wlite::Error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## Thread safety

Models are immutable after loading and can be shared via `std::shared_ptr`. Database connections are not thread-safe; use one per thread.

```cpp
#include <memory>

auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

// Share across threads
std::thread t([model]() {
    auto db = wlite::Database::open("app.db");
    db.migrate(*model);
});
```
