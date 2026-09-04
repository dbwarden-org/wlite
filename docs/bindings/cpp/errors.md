---
title: C++ Error Handling Reference
description: Exception handling, error codes, try/catch patterns, RAII cleanup, thread safety, and memory safety in the wlite C++ binding.
---

# C++ Error Handling Reference

The wlite C++ binding uses exceptions for error handling. All fallible operations throw `wlite::Error` when they fail. The error type inherits from `std::exception` and provides access to the error code and a human-readable message. Resources are managed automatically through RAII, so cleanup happens correctly even when exceptions are thrown.

This document covers the `Error` class, exception handling patterns, try/catch idiom, cleanup with RAII, thread safety, and memory safety.

## The Error class

The `wlite::Error` type wraps the C API error information. It inherits from `std::exception` and can be caught alongside other standard exceptions.

### Error construction

The C++ binding constructs `wlite::Error` objects internally. You do not create them directly. They are thrown by operations like `Database::open`, `Model::load`, `Statement::step`, and `Transaction::commit`.

```cpp
#include <wlite/wlite.hpp>

try {
    auto db = wlite::Database::open("missing.db");
} catch (const wlite::Error& e) {
    // e.what() returns the human-readable error message
    // e.result_code() returns the numeric error code
}
```

### Accessing error information

The `Error` class provides two methods for inspecting the error.

```cpp
try {
    auto model = wlite::Model::load("bad_syntax.wlite");
} catch (const wlite::Error& e) {
    // Human-readable message
    std::cerr << "Message: " << e.what() << std::endl;

    // Numeric error code
    wlite_result code = e.result_code();
    std::cerr << "Code: " << static_cast<int>(code) << std::endl;
}
```

### Error codes

The following error codes can be returned by `result_code()`.

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WLITE_OK` | Success (never thrown) |
| 1 | `WLITE_ERROR` | General or unexpected error |
| 2 | `WLITE_INVALID_ARGUMENT` | Null pointer or invalid parameter |
| 3 | `WLITE_OUT_OF_MEMORY` | Memory allocation failed |
| 4 | `WLITE_IO_ERROR` | I/O error (disk full, permission denied, file not found) |
| 5 | `WLITE_PARSE_ERROR` | Schema parse error (malformed `.wlite` source) |
| 6 | `WLITE_MODEL_ERROR` | Schema model error (invalid table, missing field) |
| 7 | `WLITE_SQLITE_ERROR` | SQLite returned an error |
| 8 | `WLITE_CONSTRAINT_ERROR` | UNIQUE, CHECK, or FOREIGN KEY constraint violation |
| 9 | `WLITE_NOT_FOUND` | Requested table, column, or resource not found |
| 10 | `WLITE_BUSY` | Database is locked by another connection |
| 11 | `WLITE_TRANSACTION_ERROR` | Transaction failed or is in an invalid state |

### Catching specific error codes

You can catch `wlite::Error` and inspect the code to handle specific error types differently.

```cpp
try {
    auto db = wlite::Database::open("app.db");
    auto stmt = db.prepare("INSERT INTO users (email) VALUES (?)");
    stmt.bind(1, "duplicate@example.com");
    stmt.step();
} catch (const wlite::Error& e) {
    switch (e.result_code()) {
        case WLITE_CONSTRAINT_ERROR:
            std::cerr << "Duplicate email" << std::endl;
            break;
        case WLITE_BUSY:
            std::cerr << "Database is locked, try again later" << std::endl;
            break;
        case WLITE_IO_ERROR:
            std::cerr << "Cannot access database file" << std::endl;
            break;
        default:
            std::cerr << "Error: " << e.what() << std::endl;
            break;
    }
}
```

## Exception handling patterns

### Basic try/catch

The fundamental pattern is a try/catch block around the entire operation.

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

### Catching both wlite and standard exceptions

Catch `wlite::Error` first, then `std::exception` as a fallback.

```cpp
try {
    auto db = wlite::Database::open("app.db");
    // ...
} catch (const wlite::Error& e) {
    std::cerr << "wlite error: " << e.what() << std::endl;
    return 1;
} catch (const std::exception& e) {
    std::cerr << "standard error: " << e.what() << std::endl;
    return 1;
}
```

### Catching all exceptions

Use `catch (...)` to catch any exception, including non-standard types.

```cpp
try {
    auto db = wlite::Database::open("app.db");
    // ...
} catch (const wlite::Error& e) {
    std::cerr << "wlite error: " << e.what() << std::endl;
    return 1;
} catch (const std::exception& e) {
    std::cerr << "standard error: " << e.what() << std::endl;
    return 1;
} catch (...) {
    std::cerr << "unknown error" << std::endl;
    return 1;
}
```

### Nested try/catch

Use nested try/catch blocks to handle errors at different levels of granularity.

```cpp
try {
    auto model = wlite::Model::load("app.wlite");
    auto db = wlite::Database::open("app.db");

    try {
        db.migrate(model);
    } catch (const wlite::Error& e) {
        std::cerr << "Migration failed: " << e.what() << std::endl;
        // Migration failed, but database connection is still valid
        // Continue with other operations
    }

    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        std::cout << name << std::endl;
    }

    return 0;
} catch (const wlite::Error& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
}
```

### Safe execution wrapper

Create a helper function that executes a callable and converts exceptions to error values.

```cpp
#include <wlite/wlite.hpp>
#include <functional>
#include <optional>

template<typename Func>
auto safe_execute(Func&& func) -> std::optional<decltype(func())> {
    try {
        return func();
    } catch (const wlite::Error& e) {
        std::cerr << "wlite error: " << e.what() << std::endl;
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

void example() {
    auto db = safe_execute([]() {
        return wlite::Database::open("app.db");
    });

    if (!db) {
        std::cerr << "Failed to open database" << std::endl;
        return;
    }

    // Use *db...
}
```

### Error logging with context

Wrap operations to add context to error messages.

```cpp
#include <wlite/wlite.hpp>
#include <string>
#include <iostream>

class DatabaseLogger {
public:
    DatabaseLogger(const std::string& path) : path_(path) {}

    wlite::Database open() {
        try {
            return wlite::Database::open(path_);
        } catch (const wlite::Error& e) {
            std::cerr << "Failed to open database '" << path_ << "': "
                      << e.what() << " (code " << e.result_code() << ")" << std::endl;
            throw;
        }
    }

    void migrate(wlite::Database& db, const wlite::Model& model) {
        try {
            db.migrate(model);
        } catch (const wlite::Error& e) {
            std::cerr << "Migration failed for '" << path_ << "': "
                      << e.what() << " (code " << e.result_code() << ")" << std::endl;
            throw;
        }
    }

private:
    std::string path_;
};
```

## Cleanup with RAII

RAII ensures that resources are cleaned up correctly even when exceptions are thrown. This is the primary defense against resource leaks in C++.

### Automatic statement cleanup

When a statement throws an exception, the statement is finalized automatically.

```cpp
void query_users(wlite::Database& db) {
    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        // If an exception is thrown here, stmt is finalized automatically
        std::cout << name << std::endl;
    }
} // stmt is finalized when it goes out of scope
```

### Automatic database cleanup

When a database connection throws an exception, the connection is closed automatically.

```cpp
void process_database(const std::string& path) {
    auto db = wlite::Database::open(path);
    // If an exception is thrown here, db is closed automatically
    // ...
} // db is closed when it goes out of scope
```

### Automatic transaction rollback

When a transaction is not committed, it is rolled back automatically.

```cpp
void safe_insert(wlite::Database& db, const std::string& name) {
    auto tx = db.begin();
    try {
        auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
        stmt.bind(1, name);
        stmt.step();
        tx.commit();
    } catch (...) {
        // tx is not committed, so it is rolled back automatically
        // when the exception propagates out of this scope
        throw;
    }
}
```

### Automatic model cleanup

When a model goes out of scope, it is freed automatically.

```cpp
void use_model() {
    auto model = wlite::Model::load("app.wlite");
    // model is used here...
} // model is freed when it goes out of scope
```

### Exception-safe batch operations

Use RAII to make batch operations exception-safe.

```cpp
void batch_insert(wlite::Database& db, const std::vector<std::string>& names) {
    auto tx = db.begin();
    try {
        auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
        for (const auto& name : names) {
            stmt.bind(1, name);
            stmt.step();
            stmt.reset();
        }
        tx.commit();
    } catch (...) {
        // tx is rolled back, stmt and db are cleaned up automatically
        throw;
    }
}
```

### Resource cleanup ordering

Resources are cleaned up in reverse order of construction. This is important when one resource depends on another.

```cpp
void process() {
    auto model = wlite::Model::load("app.wlite");   // constructed first
    auto db = wlite::Database::open("app.db");       // constructed second
    auto stmt = db.prepare("SELECT * FROM users");   // constructed third

    // When an exception is thrown or the function returns:
    // stmt is finalized first (third constructed, first destroyed)
    // db is closed second
    // model is freed last (first constructed, last destroyed)
}
```

This ordering is correct because the statement depends on the database, and the database may depend on the model.

## Thread safety

Thread safety in the wlite C++ binding follows a simple rule: only `Model` objects are thread-safe. All other types are bound to a single thread.

### Models are thread-safe

Models are immutable after loading. They can be shared across threads safely using `std::shared_ptr`.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>

void thread_safe_example() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    std::thread t1([model]() {
        auto db = wlite::Database::open("db1.db");
        db.migrate(*model);
        // model is read-only, safe across threads
    });

    std::thread t2([model]() {
        auto db = wlite::Database::open("db2.db");
        db.migrate(*model);
        // model is read-only, safe across threads
    });

    t1.join();
    t2.join();
}
```

### Database connections are not thread-safe

Each thread must have its own database connection. Do not share a `wlite::Database` across threads.

```cpp
// WRONG: sharing a connection across threads
void bad_example() {
    auto db = wlite::Database::open("app.db");

    std::thread t1([&db]() {
        auto stmt = db.prepare("SELECT * FROM users");  // DATA RACE
    });

    std::thread t2([&db]() {
        auto stmt = db.prepare("INSERT INTO logs (msg) VALUES (?)");  // DATA RACE
    });

    t1.join();
    t2.join();
}

// CORRECT: each thread gets its own connection
void good_example() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    std::thread t1([model]() {
        auto db = wlite::Database::open("app.db");
        auto stmt = db.prepare("SELECT * FROM users");
        while (stmt.step()) { /* ... */ }
    });

    std::thread t2([model]() {
        auto db = wlite::Database::open("app.db");
        auto stmt = db.prepare("INSERT INTO logs (msg) VALUES (?)");
        stmt.bind(1, "hello");
        stmt.step();
    });

    t1.join();
    t2.join();
}
```

### Statements and transactions are not thread-safe

Statements and transactions belong to a database connection. They cannot be shared across threads.

### Thread-safe model sharing patterns

Use `std::shared_ptr` to share models across threads. The shared pointer ensures the model stays alive as long as any thread holds a reference.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>
#include <vector>

class WorkerPool {
public:
    WorkerPool(const std::string& model_path, const std::string& db_path, int num_threads)
        : model_(std::make_shared<wlite::Model>(wlite::Model::load(model_path)))
        , db_path_(db_path) {
        for (int i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this, i]() {
                auto db = wlite::Database::open(db_path_);
                db.migrate(*model_);

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
    }

    void wait() {
        for (auto& t : threads_) {
            t.join();
        }
    }

private:
    std::shared_ptr<wlite::Model> model_;
    std::string db_path_;
    std::vector<std::thread> threads_;
};
```

## Memory safety

The wlite C++ binding uses RAII for memory management. All types are automatically freed when they go out of scope. This eliminates most memory safety issues.

### No manual memory management

You never need to call `free`, `delete`, `wlite_close`, `wlite_stmt_finalize`, or `wlite_model_free` manually. The C++ types handle this through their destructors.

```cpp
void example() {
    auto model = wlite::Model::load("app.wlite");  // allocated
    auto db = wlite::Database::open("app.db");      // allocated
    auto stmt = db.prepare("SELECT * FROM users");  // allocated
    // ...
} // stmt freed, db closed, model freed - all automatic
```

### No double-free

Move semantics prevent double-free bugs. After a move, the source object is in a valid but unspecified state.

```cpp
auto db1 = wlite::Database::open("app.db");
auto db2 = std::move(db1); // db1 is now empty
// db1 is valid but empty, db2 owns the connection
// No double-free occurs
```

### No use-after-free

RAII ensures that objects are not used after they are freed. The destructor runs when the object goes out of scope, and the object is no longer accessible.

```cpp
{
    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        auto name = stmt.column_text(0);
        std::cout << name << std::endl;
    }
} // stmt is freed here
// stmt is no longer accessible, so use-after-free cannot occur
```

### No dangling pointers

The `Row` type owns a snapshot of the row data. You can continue stepping or finalize the statement while keeping the row.

```cpp
std::vector<wlite::Row> rows;

{
    auto stmt = db.prepare("SELECT * FROM users");
    while (stmt.step()) {
        rows.push_back(stmt.row());
    }
} // stmt is freed here

// rows are still valid because they own their data
for (const auto& row : rows) {
    auto name = row.text(1);
    std::cout << name << std::endl;
}
```

### Shared model lifetime

When sharing a model across threads, use `std::shared_ptr` to manage the lifetime. The model stays alive as long as any thread holds a reference.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>

void shared_model_lifetime() {
    auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

    std::thread t([model]() {
        auto db = wlite::Database::open("app.db");
        db.migrate(*model);
        // model is valid because this thread holds a shared_ptr copy
    });

    // model is still valid here because the thread holds a copy
    t.join();

    // model is freed when it goes out of scope (if no other shared_ptr holds it)
}
```

### Exception safety and memory

When an exception is thrown, all objects on the stack are destroyed in reverse order. This ensures that all allocated memory is freed.

```cpp
void exception_safe() {
    auto model = wlite::Model::load("app.wlite");   // allocated
    auto db = wlite::Database::open("app.db");       // allocated
    auto stmt = db.prepare("SELECT * FROM users");   // allocated

    while (stmt.step()) {
        auto name = stmt.column_text(0);
        if (name == "error") {
            throw std::runtime_error("found error");  // exception thrown
        }
    }
} // stmt freed, db closed, model freed - all automatic, even on exception
```

### Memory leak prevention

RAII prevents memory leaks by design. Every allocation has a corresponding deallocation in the destructor.

```cpp
// This code cannot leak memory, even with exceptions
void no_leaks() {
    for (int i = 0; i < 1000; ++i) {
        auto stmt = db.prepare("INSERT INTO test (value) VALUES (?)");
        stmt.bind(1, static_cast<int64_t>(i));
        stmt.step();
    } // stmt is freed each iteration
}
```

### Cleanup on early return

RAII handles cleanup correctly on early returns as well.

```cpp
bool find_user(wlite::Database& db, const std::string& name, User& out) {
    auto stmt = db.prepare("SELECT id, name, email FROM users WHERE name = ?");
    stmt.bind(1, name);

    if (!stmt.step()) {
        return false;  // stmt is freed automatically on return
    }

    out.id = stmt.column_int64(0);
    out.name = stmt.column_text(1);
    out.email = stmt.column_text(2);
    return true;
} // stmt is freed here
```

## Complete error handling example

Here is a complete program that demonstrates all error handling patterns: try/catch, specific error code handling, RAII cleanup, thread safety, and memory safety.

```cpp
#include <wlite/wlite.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>

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

    void insert_user(const std::string& name, const std::string& email) {
        auto stmt = db_.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
        stmt.bind(1, name);
        stmt.bind(2, email);
        stmt.step();
    }

    std::vector<User> list_users() {
        auto stmt = db_.prepare("SELECT id, name, email FROM users ORDER BY name");
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

    void batch_insert(const std::vector<std::pair<std::string, std::string>>& users) {
        auto tx = db_.begin();
        try {
            auto stmt = db_.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
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

    std::shared_ptr<wlite::Model> model() const { return model_; }
    const std::string& db_path() const { return db_path_; }

private:
    std::shared_ptr<wlite::Model> model_;
    wlite::Database db_;
    std::string db_path_;
};

void worker(std::shared_ptr<wlite::Model> model, const std::string& db_path, int id) {
    try {
        auto db = wlite::Database::open(db_path);
        db.migrate(*model);

        auto stmt = db.prepare("INSERT INTO logs (thread_id, message) VALUES (?, ?)");
        stmt.bind(1, static_cast<int64_t>(id));
        stmt.bind(2, "Hello from thread " + std::to_string(id));
        stmt.step();
    } catch (const wlite::Error& e) {
        std::cerr << "Thread " << id << " error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        // Load model once, share across threads
        auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

        // Open database
        auto db = wlite::Database::open("app.db");
        db.migrate(*model);

        // Create tables
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE
            )
        )");

        db.execute(R"(
            CREATE TABLE IF NOT EXISTS logs (
                id INTEGER PRIMARY KEY,
                thread_id INTEGER,
                message TEXT
            )
        )");

        // Insert users with error handling
        try {
            db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");
            db.execute("INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')");
        } catch (const wlite::Error& e) {
            if (e.result_code() == WLITE_CONSTRAINT_ERROR) {
                std::cerr << "Duplicate user, skipping" << std::endl;
            } else {
                throw;
            }
        }

        // Batch insert with transaction
        std::vector<std::pair<std::string, std::string>> batch = {
            {"Charlie", "charlie@example.com"},
            {"Diana", "diana@example.com"},
            {"Eve", "eve@example.com"},
        };

        try {
            auto tx = db.begin();
            try {
                auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
                for (const auto& [name, email] : batch) {
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
        } catch (const wlite::Error& e) {
            std::cerr << "Batch insert failed: " << e.what() << std::endl;
        }

        // List users
        std::cout << "Users:" << std::endl;
        auto stmt = db.prepare("SELECT id, name, email FROM users ORDER BY name");
        while (stmt.step()) {
            std::cout << "  " << stmt.column_int64(0) << ": "
                      << stmt.column_text(1) << " <"
                      << stmt.column_text(2) << ">" << std::endl;
        }

        // Multi-threaded operations
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(worker, model, "app.db", i);
        }
        for (auto& t : threads) {
            t.join();
        }

        // List logs
        std::cout << "\nLogs:" << std::endl;
        auto log_stmt = db.prepare("SELECT thread_id, message FROM logs ORDER BY id");
        while (log_stmt.step()) {
            std::cout << "  Thread " << log_stmt.column_int64(0) << ": "
                      << log_stmt.column_text(1) << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Fatal wlite error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error" << std::endl;
        return 1;
    }
}
```

This program demonstrates the recommended error handling patterns for the wlite C++ binding. All resources are managed through RAII, exceptions are caught and handled at appropriate levels, and thread safety is maintained by sharing only the immutable model across threads.
