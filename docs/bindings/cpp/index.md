---
title: C++ Binding Reference
description: Header-only C++ wrapper for libwlite with RAII types, exception handling, and modern C++ idioms.
---

# C++ Binding Reference

The wlite C++ binding is a header-only wrapper around the libwlite C API. It provides RAII types, exception-based error handling, and C++ idioms while delegating to the C ABI. The binding is designed to feel native to C++ developers. It uses modern C++ features like move semantics, smart pointers, and range-based for loops where appropriate.

This document covers installation, build configuration, the core type system, and the RAII resource management model.

## Installation

The C++ binding consists of a single header file. No separate library compilation is required. The binding does depend on the libwlite shared library and SQLite at link time.

### Header location

After installing libwlite, the C++ header is available at:

```
/usr/local/include/wlite/wlite.hpp
```

or, if installed via a package manager:

```
<prefix>/include/wlite/wlite.hpp
```

Include the header in your source files:

```cpp
#include <wlite/wlite.hpp>
```

### Linking

Even though the binding is header-only, you must link against libwlite and SQLite3 at compile time. The binding calls into the libwlite C ABI, which in turn calls into SQLite.

### CMake integration

Using CMake is the recommended way to build projects that use the wlite C++ binding. The `find_package` command locates the wlite installation and sets up include paths and linker flags automatically.

```cmake
cmake_minimum_required(VERSION 3.15)
project(myapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wlite REQUIRED)

add_executable(myapp
    src/main.cpp
    src/database.cpp
    src/models.cpp
)

target_link_libraries(myapp PRIVATE wlite::wlite)
```

The `wlite::wlite` target provides the correct include directories, compile definitions, and linker libraries. You do not need to specify `-lwlite` or `-lsqlite3` separately when using this target.

### pkg-config

If CMake is not available, use pkg-config:

```bash
g++ -std=c++17 main.cpp -o myapp $(pkg-config --cflags --libs wlite)
```

### Manual compilation

For simple projects or one-off builds, compile directly with g++:

```bash
g++ -std=c++17 -I/usr/local/include main.cpp -o myapp -L/usr/local/lib -lwlite -lsqlite3
```

The exact paths depend on where libwlite and SQLite are installed on your system.

### Requirements

- A C++17 compiler (GCC 8+, Clang 7+, MSVC 19.14+)
- libwlite 0.2.0 or later (ABI v1)
- SQLite 3.25.0 or later (for `RENAME COLUMN` support)

## Quick start

The simplest program that uses wlite loads a model, opens a database, migrates it, and runs a query.

```cpp
#include <wlite/wlite.hpp>
#include <iostream>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        db.migrate(model);

        auto stmt = db.prepare("SELECT * FROM users");
        while (stmt.step()) {
            auto name = stmt.column_text(0);
            std::cout << name << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

This example demonstrates the core pattern: load a schema model, open a database connection, apply migrations, and execute queries. All resources are managed automatically through RAII. If any operation fails, a `wlite::Error` exception is thrown, and all resources are cleaned up when the exception propagates out of the try block.

### What happens step by step

1. `wlite::Model::load("app.wlite")` parses the `.wlite` schema file into an in-memory model. If the file does not exist or contains syntax errors, an exception is thrown.

2. `wlite::Database::open("app.db")` opens (or creates) an SQLite database file. The connection is configured with foreign key enforcement enabled by default.

3. `db.migrate(model)` compares the live database schema against the model. If differences are found, it generates and executes the appropriate SQL to bring the database into alignment. If the database is brand new, it creates all tables, indexes, and constraints.

4. `db.prepare("SELECT * FROM users")` compiles a SQL statement and returns a `wlite::Statement` object. The statement is prepared once and can be executed immediately.

5. `stmt.step()` advances to the next result row. It returns `true` if a row is available and `false` when the result set is exhausted.

6. `stmt.column_text(0)` reads the first column of the current row as a string.

7. When the function returns, all objects are destroyed in reverse order: the statement is finalized, the database is closed, and the model is freed. No manual cleanup is required.

## Types

The C++ binding defines four core types and several supporting types. Each wraps a corresponding C type from libwlite.

| C++ Type | C Equivalent | Description |
|----------|--------------|-------------|
| `wlite::Database` | `wlite_db` | Open database connection |
| `wlite::Model` | `wlite_model` | Loaded `.wlite` schema |
| `wlite::Statement` | `wlite_stmt` | Prepared SQL statement |
| `wlite::Transaction` | `wlite_tx` | Active transaction |
| `wlite::Error` | `wlite_result` | Exception type |
| `wlite::Row` | result row | Single row from a query |

### wlite::Database

The `Database` type manages a connection to an SQLite database. It provides methods for executing SQL, preparing statements, beginning transactions, and running migrations. The database is opened through the static factory method `Database::open` and closed automatically when the object is destroyed.

```cpp
auto db = wlite::Database::open("app.db");
```

The `Database` type is move-only. It cannot be copied. Moving a `Database` transfers ownership of the underlying connection to the new object.

```cpp
auto db1 = wlite::Database::open("app.db");
auto db2 = std::move(db1); // db1 is now empty, db2 owns the connection
```

### wlite::Model

The `Model` type represents a parsed `.wlite` schema. It is immutable after loading and can be shared across threads using `std::shared_ptr`. The model is loaded through the static factory method `Model::load` or constructed from a compiled `.wlitem` binary.

```cpp
auto model = wlite::Model::load("app.wlite");
```

Models can be shared across multiple database connections and across threads. They do not hold any mutable state.

```cpp
auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

// Use in multiple places
{
    auto db1 = wlite::Database::open("db1.db");
    db1.migrate(model);
}
{
    auto db2 = wlite::Database::open("db2.db");
    db2.migrate(model);
}
```

### wlite::Statement

The `Statement` type wraps a prepared SQL statement. It provides methods for binding parameters, stepping through result rows, and accessing column values. Statements are created by calling `Database::prepare`.

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE id = ?");
stmt.bind(1, 42LL);
while (stmt.step()) {
    auto name = stmt.column_text(0);
}
```

Statements are move-only and are finalized automatically when they go out of scope.

### wlite::Transaction

The `Transaction` type manages an active database transaction. It provides `commit` and `rollback` methods. If the transaction object is destroyed without being committed, it is rolled back automatically.

```cpp
auto tx = db.begin();
db.execute("INSERT INTO users (name) VALUES ('Alice')");
tx.commit();
```

Transactions are move-only. Once committed or rolled back, the transaction object is in a terminal state and should not be used further.

### wlite::Error

The `Error` type is the exception thrown by wlite operations. It inherits from `std::exception` and provides access to the error code and a human-readable message.

```cpp
try {
    auto db = wlite::Database::open("missing.db");
} catch (const wlite::Error& e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "Code: " << e.result_code() << std::endl;
}
```

### wlite::Row

The `Row` type represents a single row from a query result. It is created from a stepped statement and owns a snapshot of the row data. The row is independent of the statement, so you can continue stepping or finalize the statement while keeping the row.

```cpp
auto stmt = db.prepare("SELECT id, name FROM users");
while (stmt.step()) {
    auto row = stmt.row();
    auto id = row.int64(0);
    auto name = row.text(1);
}
```

## RAII

All types in the wlite C++ binding use RAII (Resource Acquisition Is Initialization) for resource management. Resources are automatically freed when objects go out of scope.

### Why RAII matters

RAII eliminates an entire class of bugs: resource leaks. In C, every `wlite_open` must be paired with a `wlite_close`, every `wlite_prepare` with a `wlite_stmt_finalize`, and every `wlite_begin` with a `wlite_commit` or `wlite_rollback` followed by `wlite_tx_free`. Missing any of these leads to leaked file descriptors, memory, or database locks.

In C++, the destructor of each type handles the corresponding cleanup. You never need to call `close`, `finalize`, or `free` manually.

```cpp
{
    auto db = wlite::Database::open("app.db");
    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        std::cout << name << std::endl;
    }
} // stmt is finalized, db is closed automatically
```

### Automatic cleanup on exceptions

If an exception is thrown, all objects on the stack are destroyed in reverse order of construction. This means resources are cleaned up even when errors occur.

```cpp
try {
    auto db = wlite::Database::open("app.db");
    auto tx = db.begin();
    db.execute("INSERT INTO users (name) VALUES ('Alice')");
    tx.commit(); // If this throws, tx is rolled back
} catch (const wlite::Error& e) {
    // db and tx are destroyed, their resources are freed
}
```

### Move semantics

All wlite types support move construction and move assignment but not copy. This prevents double-free bugs and makes ownership explicit.

```cpp
auto db1 = wlite::Database::open("app.db");
auto db2 = std::move(db1); // db1 is now empty
// db1 is invalid, db2 owns the connection
```

After a move, the source object is in a valid but unspecified state. You should not use it for any operation except destruction.

### Scope-based resource management

RAII works best when objects have tight lifetimes. Create objects in the narrowest scope that needs them.

```cpp
void process_users() {
    auto db = wlite::Database::open("app.db");

    {
        auto stmt = db.prepare("SELECT * FROM users");
        while (stmt.step()) {
            auto name = stmt.column_text(0);
            std::cout << name << std::endl;
        }
    } // stmt finalized here

    {
        auto tx = db.begin();
        db.execute("INSERT INTO logs (event) VALUES ('processed')");
        tx.commit();
    } // tx committed and freed here
} // db closed here
```

### Shared ownership of models

Models are immutable and can be safely shared. Use `std::shared_ptr` to share a model across threads or across multiple database connections.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>

void multi_threaded_migration() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([model, i]() {
            auto db = wlite::Database::open("app.db");
            db.migrate(*model);
            // each thread gets its own connection
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}
```

The `shared_ptr` ensures the model stays alive as long as any thread holds a reference. When the last reference is destroyed, the model is freed.

### Transaction safety with RAII

The `Transaction` type provides automatic rollback when an exception occurs. If you call `commit`, the transaction is finalized. If you do not call `commit` before the transaction object is destroyed, it is rolled back.

```cpp
void safe_transfer(wlite::Database& db, int64_t from, int64_t to, int64_t amount) {
    auto tx = db.begin();

    auto debit = db.prepare("UPDATE accounts SET balance = balance - ? WHERE id = ?");
    debit.bind(1, amount);
    debit.bind(2, from);
    debit.step();

    auto credit = db.prepare("UPDATE accounts SET balance = balance + ? WHERE id = ?");
    credit.bind(1, amount);
    credit.bind(2, to);
    credit.step();

    auto balance = db.query_scalar("SELECT balance FROM accounts WHERE id = ?");
    if (balance < 0) {
        throw std::runtime_error("Insufficient funds");
        // tx is rolled back automatically when it is destroyed
    }

    tx.commit();
}
```

This pattern eliminates the need for explicit try/catch blocks in many cases. The RAII semantics of `Transaction` handle rollback on any exception path.

### Statement reuse

Statements can be reused by calling `reset` after stepping through all results. This rebinds the statement to a new set of parameters without re-preparing it.

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");

stmt.bind(1, "Alice");
stmt.bind(2, "alice@example.com");
stmt.step();
stmt.reset();

stmt.bind(1, "Bob");
stmt.bind(2, "bob@example.com");
stmt.step();
stmt.reset();

stmt.bind(1, "Charlie");
stmt.bind(2, "charlie@example.com");
stmt.step();
```

### Complete example with RAII patterns

Here is a complete program that demonstrates all major RAII patterns: scoped resources, shared model ownership, transaction safety, and statement reuse.

```cpp
#include <wlite/wlite.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

struct User {
    int64_t id;
    std::string name;
    std::string email;
};

class AppDatabase {
public:
    AppDatabase(const std::string& db_path, const std::string& model_path)
        : model_(std::make_shared<wlite::Model>(wlite::Model::load(model_path)))
        , db_(wlite::Database::open(db_path)) {
        db_.migrate(*model_);
    }

    void insert_users(const std::vector<std::pair<std::string, std::string>>& users) {
        auto stmt = db_.prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        for (const auto& [name, email] : users) {
            stmt.bind(1, name);
            stmt.bind(2, email);
            stmt.step();
            stmt.reset();
        }
    }

    std::vector<User> list_users() {
        auto stmt = db_.prepare(
            "SELECT id, name, email FROM users ORDER BY name"
        );

        std::vector<User> users;
        while (stmt.step()) {
            users.push_back({
                stmt.column_int64(0),
                stmt.column_text(1),
                stmt.column_text(2),
            });
        }
        return users;
    }

    int64_t count_users() {
        return db_.query_scalar("SELECT COUNT(*) FROM users");
    }

    void batch_insert(const std::vector<std::pair<std::string, std::string>>& users) {
        auto tx = db_.begin();
        try {
            auto stmt = db_.prepare(
                "INSERT INTO users (name, email) VALUES (?, ?)"
            );
            for (const auto& [name, email] : users) {
                stmt.bind(1, name);
                stmt.bind(2, email);
                stmt.step();
                stmt.reset();
            }
            tx.commit();
        } catch (...) {
            tx.rollback();
            throw;
        }
    }

private:
    std::shared_ptr<wlite::Model> model_;
    wlite::Database db_;
};

int main() {
    try {
        AppDatabase app("app.db", "app.wlite");

        std::vector<std::pair<std::string, std::string>> users = {
            {"Alice", "alice@example.com"},
            {"Bob", "bob@example.com"},
            {"Charlie", "charlie@example.com"},
        };

        app.insert_users(users);
        std::cout << "Total users: " << app.count_users() << std::endl;

        for (const auto& user : app.list_users()) {
            std::cout << user.id << ": " << user.name
                      << " <" << user.email << ">" << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

This pattern keeps all resource management implicit through RAII. The only explicit error handling is the top-level try/catch block. Every other resource (database connection, statements, transactions, model) is managed automatically by its owning object.

## Thread safety summary

| Object | Thread-safe | Notes |
|--------|-------------|-------|
| `wlite::Model` | Yes | Immutable after loading, share via `shared_ptr` |
| `wlite::Database` | No | One connection per thread |
| `wlite::Statement` | No | Belongs to a connection |
| `wlite::Transaction` | No | Belongs to a connection |
| `wlite::Row` | No | Snapshot of row data, not safe across threads |
| `wlite::Error` | Yes | Immutable after construction |

Models are the only type that can be safely shared across threads. All other types are bound to a single thread through their owning database connection. If you need to use a model from multiple threads, wrap it in a `std::shared_ptr` and pass copies to each thread.
