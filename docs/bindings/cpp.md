---
title: C++ Binding
description: Header-only C++ wrapper for libwlite.
---

# C++ Binding

The C++ binding is a header-only wrapper around the libwlite C API. It provides RAII types, exception-based error handling, and C++ idioms while delegating to the C ABI.

The binding is designed to feel native to C++ developers. It uses modern C++ features like move semantics, smart pointers, and range-based for loops where appropriate.

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

### Build configuration

Using CMake:

```cmake
find_package(wlite REQUIRED)
add_executable(myapp main.cpp)
target_link_libraries(myapp wlite::wlite)
```

Using pkg-config:

```bash
g++ -std=c++17 main.cpp -o myapp $(pkg-config --cflags --libs wlite)
```

Manual compilation:

```bash
g++ -std=c++17 -I/usr/local/include main.cpp -o myapp -L/usr/local/lib -lwlite -lsqlite3
```

### Complete build example

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

## Types

| C++ Type | C Equivalent | Description |
|----------|--------------|-------------|
| `wlite::Database` | `wlite_db` | Open database connection |
| `wlite::Model` | `wlite_model` | Loaded .wlite schema |
| `wlite::Statement` | `wlite_stmt` | Prepared SQL statement |
| `wlite::Transaction` | `wlite_tx` | Active transaction |
| `wlite::Error` | `wlite_result` | Exception type |
| `wlite::Row` | result row | Single row from a query |

The `Database` type manages the connection lifetime and provides methods for executing SQL, preparing statements, and beginning transactions.

The `Model` type represents a parsed `.wlite` schema. It is immutable after loading and can be shared across threads using `std::shared_ptr`.

The `Statement` type wraps a prepared SQL statement with parameter binding and column access methods.

## RAII

All types use RAII for resource management. Resources are automatically freed when objects go out of scope:

```cpp
{
    auto db = wlite::Database::open("app.db");
    // use db...
} // db is closed automatically
```

This eliminates manual cleanup and prevents resource leaks.

### Move semantics

Types support move construction and assignment but not copy:

```cpp
auto db1 = wlite::Database::open("app.db");
auto db2 = std::move(db1); // db1 is now empty
// db1 is invalid, db2 owns the connection
```

### Exception safety

All operations are exception-safe. If an exception is thrown, resources are properly cleaned up:

```cpp
try {
    auto db = wlite::Database::open("app.db");
    auto tx = db.begin();
    db.execute("INSERT INTO users (name) VALUES ('Alice')");
    tx.commit(); // If this throws, tx is rolled back
} catch (const wlite::Error& e) {
    // Resources are cleaned up automatically
}
```

## Database operations

```cpp
#include <wlite/wlite.hpp>

void database_operations() {
    // Open a database
    auto db = wlite::Database::open("app.db");

    // Execute DDL
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    db.execute("CREATE INDEX idx_test_name ON test(name)");

    // Execute DML
    db.execute("INSERT INTO test (name) VALUES ('hello')");
    db.execute("UPDATE test SET name = 'world' WHERE id = 1");
    db.execute("DELETE FROM test WHERE id = 1");

    // Prepare and query
    auto stmt = db.prepare("SELECT * FROM test WHERE id = ?");
    stmt.bind(1, 1LL);
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        auto id = stmt.column_int64(0);
        std::cout << id << ": " << name << std::endl;
    }

    // Single value query
    auto count = db.query_scalar("SELECT COUNT(*) FROM test");
    std::cout << "Count: " << count << std::endl;
}
```

### Batch operations

```cpp
#include <wlite/wlite.hpp>
#include <vector>

void batch_insert(wlite::Database& db) {
    auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");

    std::vector<std::pair<std::string, std::string>> users = {
        {"Alice", "alice@example.com"},
        {"Bob", "bob@example.com"},
        {"Charlie", "charlie@example.com"},
    };

    for (const auto& [name, email] : users) {
        stmt.bind(1, name);
        stmt.bind(2, email);
        stmt.step();
        stmt.reset();
    }
}
```

### Query with column access

```cpp
#include <wlite/wlite.hpp>

void query_columns(wlite::Database& db) {
    auto stmt = db.prepare("SELECT id, name, email, created_at FROM users");

    std::cout << "Columns: " << stmt.column_count() << std::endl;

    while (stmt.step()) {
        auto id = stmt.column_int64(0);
        auto name = stmt.column_text(1);
        auto email = stmt.column_text(2);
        auto created = stmt.column_text(3);

        std::cout << "User " << id << ": " << name
                  << " <" << email << "> created at " << created << std::endl;
    }
}
```

## Prepared statements

Prepared statements allow you to compile SQL once and execute it multiple times with different parameters. This improves performance and prevents SQL injection.

```cpp
#include <wlite/wlite.hpp>

void prepared_statements(wlite::Database& db) {
    // Prepare an INSERT statement
    auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");

    // Insert Alice
    stmt.bind(1, "Alice");
    stmt.bind(2, "alice@example.com");
    stmt.step();
    stmt.reset();

    // Insert Bob
    stmt.bind(1, "Bob");
    stmt.bind(2, "bob@example.com");
    stmt.step();
    stmt.reset();

    // Insert Charlie
    stmt.bind(1, "Charlie");
    stmt.bind(2, "charlie@example.com");
    stmt.step();

    // Prepare a SELECT statement
    auto query = db.prepare("SELECT * FROM users WHERE name = ?");
    query.bind(1, "Alice");

    while (query.step()) {
        auto id = query.column_int64(0);
        auto name = query.column_text(1);
        std::cout << "Found user " << id << ": " << name << std::endl;
    }
}
```

### Column access methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `column_count()` | `int` | Number of columns in result |
| `column_name(i)` | `std::string` | Name of column at index |
| `column_type(i)` | `ColumnType` | Data type of column |
| `column_int64(i)` | `int64_t` | Integer value |
| `column_double(i)` | `double` | Floating point value |
| `column_text(i)` | `std::string` | Text value |

## Transactions

Transactions ensure that a group of operations either all succeed or all fail. This maintains data consistency.

```cpp
#include <wlite/wlite.hpp>

void transfer_funds(wlite::Database& db, int64_t from, int64_t to, int64_t amount) {
    auto tx = db.begin();

    try {
        // Debit sender
        db.execute("UPDATE accounts SET balance = balance - ? WHERE id = ?");

        // Credit receiver
        db.execute("UPDATE accounts SET balance = balance + ? WHERE id = ?");

        // Verify sender has sufficient funds
        auto balance = db.query_scalar("SELECT balance FROM accounts WHERE id = ?");

        if (balance < 0) {
            tx.rollback();
            throw std::runtime_error("Insufficient funds");
        }

        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

### Transaction with automatic rollback

```cpp
#include <wlite/wlite.hpp>
#include <vector>

void batch_with_transaction(wlite::Database& db, const std::vector<Order>& orders) {
    auto tx = db.begin();

    try {
        auto stmt = db.prepare("INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)");

        for (const auto& order : orders) {
            stmt.bind(1, order.user_id);
            stmt.bind(2, order.product_id);
            stmt.bind(3, order.quantity);
            stmt.step();
            stmt.reset();
        }

        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

## Error handling

Errors throw `wlite::Error`:

```cpp
#include <wlite/wlite.hpp>

try {
    auto db = wlite::Database::open("app.db");
} catch (const wlite::Error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << "Code: " << e.result_code() << std::endl;
}
```

### Error codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WLITE_OK` | Success |
| 1 | `WLITE_ERROR` | General error |
| 2 | `WLITE_INVALID_ARGUMENT` | Null pointer or invalid parameter |
| 3 | `WLITE_OUT_OF_MEMORY` | Allocation failed |
| 4 | `WLITE_IO_ERROR` | I/O error |
| 5 | `WLITE_PARSE_ERROR` | Schema parse error |
| 6 | `WLITE_MODEL_ERROR` | Schema model error |
| 7 | `WLITE_SQLITE_ERROR` | SQLite error |
| 8 | `WLITE_CONSTRAINT_ERROR` | Constraint violation |
| 9 | `WLITE_NOT_FOUND` | Resource not found |
| 10 | `WLITE_BUSY` | Database locked |
| 11 | `WLITE_TRANSACTION_ERROR` | Transaction error |

### Custom error handling

```cpp
#include <wlite/wlite.hpp>
#include <functional>

template<typename Func>
auto safe_execute(Func&& func) -> std::optional<decltype(func())> {
    try {
        return func();
    } catch (const wlite::Error& e) {
        std::cerr << "wlite error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

void error_handling_example() {
    auto db = safe_execute([]() {
        return wlite::Database::open("app.db");
    });

    if (!db) {
        std::cerr << "Failed to open database" << std::endl;
        return;
    }
}
```

## Memory management

The C++ binding uses RAII and smart pointers for memory management. All types are automatically freed when they go out of scope.

```cpp
{
    auto db = wlite::Database::open("app.db");
    // use db...
} // db is closed automatically
```

### Using shared_ptr for model sharing

```cpp
#include <wlite/wlite.hpp>
#include <memory>

void shared_model_example() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    // Use in multiple places
    {
        auto db = wlite::Database::open("app.db");
        db.migrate(model);
    }

    {
        auto db = wlite::Database::open("other.db");
        db.migrate(model);
    }
}
```

## Thread safety

Models are immutable after loading and can be shared via `std::shared_ptr`. Database connections are not thread-safe; use one per thread.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>

void thread_safety_example() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    // Share across threads
    std::thread t([model]() {
        auto db = wlite::Database::open("app.db");
        db.migrate(*model);

        auto stmt = db.prepare("INSERT INTO logs (message) VALUES (?)");
        stmt.bind(1, "Hello from thread");
        stmt.step();
    });

    t.join();
}
```

### Multi-threaded worker pool

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>
#include <vector>

void multi_threaded_example() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([model, i]() {
            auto db = wlite::Database::open("app.db");
            db.migrate(*model);

            for (int j = 0; j < 100; ++j) {
                auto stmt = db.prepare(
                    "INSERT INTO work_items (thread_id, data) VALUES (?, ?)"
                );
                stmt.bind(1, static_cast<int64_t>(i));
                stmt.bind(2, "item_" + std::to_string(j));
                stmt.step();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}
```

## Complete example

Here is a complete, working program that demonstrates all major features:

```cpp
#include <wlite/wlite.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

struct User {
    int64_t id;
    std::string name;
    std::string email;
    bool active;
};

class UserDatabase {
public:
    UserDatabase(const std::string& db_path, const std::string& model_path)
        : model_(std::make_shared<wlite::Model>(wlite::Model::load(model_path)))
        , db_(wlite::Database::open(db_path)) {
        db_.migrate(*model_);
    }

    void create_tables() {
        db_.execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                active INTEGER DEFAULT 1,
                created_at TEXT DEFAULT (datetime('now'))
            )
        )");
    }

    void insert_user(const std::string& name, const std::string& email) {
        auto stmt = db_.prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.bind(1, name);
        stmt.bind(2, email);
        stmt.step();
    }

    void insert_users(const std::vector<std::pair<std::string, std::string>>& users) {
        auto stmt = db_.prepare(
            "INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)"
        );
        for (const auto& [name, email] : users) {
            stmt.bind(1, name);
            stmt.bind(2, email);
            stmt.step();
            stmt.reset();
        }
    }

    std::optional<User> get_user(int64_t user_id) {
        auto stmt = db_.prepare(
            "SELECT id, name, email, active FROM users WHERE id = ?"
        );
        stmt.bind(1, user_id);

        if (stmt.step()) {
            return User{
                stmt.column_int64(0),
                stmt.column_text(1),
                stmt.column_text(2),
                stmt.column_int64(3) != 0
            };
        }
        return std::nullopt;
    }

    std::vector<User> search_users(const std::string& pattern) {
        auto stmt = db_.prepare(
            "SELECT id, name, email, active FROM users WHERE name LIKE ?"
        );
        stmt.bind(1, "%" + pattern + "%");

        std::vector<User> results;
        while (stmt.step()) {
            results.push_back({
                stmt.column_int64(0),
                stmt.column_text(1),
                stmt.column_text(2),
                stmt.column_int64(3) != 0
            });
        }
        return results;
    }

    std::vector<User> list_users() {
        auto stmt = db_.prepare(
            "SELECT id, name, email, active FROM users ORDER BY name"
        );

        std::vector<User> users;
        while (stmt.step()) {
            users.push_back({
                stmt.column_int64(0),
                stmt.column_text(1),
                stmt.column_text(2),
                stmt.column_int64(3) != 0
            });
        }
        return users;
    }

    int64_t count_users() {
        return db_.query_scalar("SELECT COUNT(*) FROM users");
    }

    void batch_transfer(
        const std::vector<std::tuple<int64_t, int64_t, int64_t>>& transfers
    ) {
        auto tx = db_.begin();

        try {
            for (const auto& [from, to, amount] : transfers) {
                auto debit = db_.prepare(
                    "UPDATE accounts SET balance = balance - ? WHERE id = ?"
                );
                debit.bind(1, amount);
                debit.bind(2, from);
                debit.step();

                auto credit = db_.prepare(
                    "UPDATE accounts SET balance = balance + ? WHERE id = ?"
                );
                credit.bind(1, amount);
                credit.bind(2, to);
                credit.step();

                auto balance = db_.query_scalar(
                    "SELECT balance FROM accounts WHERE id = ?"
                );
                if (balance < 0) {
                    throw std::runtime_error("Insufficient funds");
                }
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
        UserDatabase db("app.db", "app.wlite");

        db.create_tables();

        std::vector<std::pair<std::string, std::string>> users = {
            {"Alice", "alice@example.com"},
            {"Bob", "bob@example.com"},
            {"Charlie", "charlie@example.com"},
        };
        db.insert_users(users);

        std::cout << "Total users: " << db.count_users() << std::endl;

        std::cout << "\nAll users:" << std::endl;
        for (const auto& user : db.list_users()) {
            std::cout << "  " << user.id << ": " << user.name
                      << " <" << user.email << ">"
                      << (user.active ? "" : " [inactive]")
                      << std::endl;
        }

        std::cout << "\nSearch results for 'Ali':" << std::endl;
        for (const auto& user : db.search_users("Ali")) {
            std::cout << "  " << user.name << std::endl;
        }

        auto alice = db.get_user(1);
        if (alice) {
            std::cout << "\nGot user: " << alice->name << std::endl;
        }

    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```
