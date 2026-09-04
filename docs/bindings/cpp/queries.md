---
title: C++ Query Reference
description: Prepared statements, parameter binding, column access, records, transactions, and savepoints in the wlite C++ binding.
---

# C++ Query Reference

The wlite C++ binding provides a complete query interface through prepared statements. You prepare a SQL statement once, bind parameters, step through result rows, and access column values. The binding uses RAII for resource management and exceptions for error handling.

This document covers preparing statements, binding parameters, stepping through results, accessing columns, using records, managing transactions with RAII, and working with savepoints.

## Preparing statements

Use `Database::prepare` to compile a SQL statement. The compiled statement can be executed immediately or reused with different parameters.

### Basic prepare

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto db = wlite::Database::open("app.db");

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

### Prepare with parameters

Use `?` as a placeholder for parameters. Parameters are 1-indexed.

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE id = ? AND name = ?");
stmt.bind(1, 42LL);
stmt.bind(2, "Alice");
while (stmt.step()) {
    auto email = stmt.column_text(2);
    std::cout << email << std::endl;
}
```

### Prepare for DDL and DML

Prepared statements work for INSERT, UPDATE, DELETE, and DDL statements as well.

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
stmt.bind(1, "Alice");
stmt.bind(2, "alice@example.com");
stmt.step();
```

### Statement lifecycle

A statement goes through these states:

1. **Prepared**: Created by `Database::prepare`. Ready for binding.
2. **Bound**: Parameters are bound. Ready for stepping.
3. **Stepped**: `step()` returns `true`. Column values are available.
4. **Exhausted**: `step()` returns `false`. No more rows.
5. **Reset**: `reset()` returns the statement to the bound state for reuse.
6. **Finalized**: Destroyed by RAII. Resources are freed.

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE name = ?");

// Step 1: bind
stmt.bind(1, "Alice");

// Step 2: step through results
while (stmt.step()) {
    auto id = stmt.column_int64(0);
    auto name = stmt.column_text(1);
}

// Step 3: reset for reuse
stmt.reset();
stmt.bind(1, "Bob");
while (stmt.step()) {
    auto id = stmt.column_int64(0);
    auto name = stmt.column_text(1);
}

// Step 4: finalized automatically when stmt goes out of scope
```

## Binding parameters

Parameters are bound by position using 1-based indexing. The binding methods accept the appropriate C++ type and convert it to the SQLite type.

### Bind text

```cpp
auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
stmt.bind(1, "Alice");
stmt.step();
```

### Bind integer

```cpp
auto stmt = db.prepare("INSERT INTO counts (value) VALUES (?)");
stmt.bind(1, 42LL);
stmt.step();
```

### Bind double

```cpp
auto stmt = db.prepare("INSERT INTO measurements (value) VALUES (?)");
stmt.bind(1, 3.14);
stmt.step();
```

### Bind null

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
stmt.bind(1, "Bob");
stmt.bind_null(2);
stmt.step();
```

### Bind blob

```cpp
#include <vector>

std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
auto stmt = db.prepare("INSERT INTO blobs (data) VALUES (?)");
stmt.bind_blob(1, data.data(), data.size());
stmt.step();
```

### Bind multiple parameters

```cpp
auto stmt = db.prepare(
    "INSERT INTO orders (user_id, product_id, quantity, price) VALUES (?, ?, ?, ?)"
);
stmt.bind(1, user_id);
stmt.bind(2, product_id);
stmt.bind(3, quantity);
stmt.bind(4, price);
stmt.step();
```

### Rebinding after reset

After stepping through all results, call `reset` to reuse the statement with different parameters.

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE name = ?");

stmt.bind(1, "Alice");
while (stmt.step()) { /* ... */ }
stmt.reset();

stmt.bind(1, "Bob");
while (stmt.step()) { /* ... */ }
stmt.reset();

stmt.bind(1, "Charlie");
while (stmt.step()) { /* ... */ }
```

## Stepping through results

The `step` method advances the statement to the next result row. It returns `true` if a row is available and `false` when the result set is exhausted.

### Basic stepping

```cpp
auto stmt = db.prepare("SELECT id, name, email FROM users");
while (stmt.step()) {
    auto id = stmt.column_int64(0);
    auto name = stmt.column_text(1);
    auto email = stmt.column_text(2);
    std::cout << id << ": " << name << " <" << email << ">" << std::endl;
}
```

### Single-row result

For queries that return a single row, step once.

```cpp
auto stmt = db.prepare("SELECT COUNT(*) FROM users WHERE active = ?");
stmt.bind(1, 1LL);
if (stmt.step()) {
    auto count = stmt.column_int64(0);
    std::cout << "Active users: " << count << std::endl;
}
```

### No result

For statements that do not return rows (INSERT, UPDATE, DELETE, DDL), step once.

```cpp
auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
stmt.bind(1, "Alice");
stmt.step();
```

### Stepping with reset

For batch operations, step and reset in a loop.

```cpp
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
```

## Column access

After a successful `step`, you can access column values by index. Columns are 0-indexed.

### Column count

```cpp
auto stmt = db.prepare("SELECT id, name, email FROM users");
while (stmt.step()) {
    int count = stmt.column_count();
    std::cout << "Row has " << count << " columns" << std::endl;
}
```

### Column names

```cpp
auto stmt = db.prepare("SELECT id, name, email FROM users");
for (int i = 0; i < stmt.column_count(); ++i) {
    std::cout << "Column " << i << ": " << stmt.column_name(i) << std::endl;
}
```

### Column types

```cpp
auto stmt = db.prepare("SELECT * FROM users");
while (stmt.step()) {
    for (int i = 0; i < stmt.column_count(); ++i) {
        auto type = stmt.column_type(i);
        auto name = stmt.column_name(i);
        std::cout << name << ": type=" << static_cast<int>(type) << std::endl;
    }
}
```

### Integer access

```cpp
auto stmt = db.prepare("SELECT id, age FROM users");
while (stmt.step()) {
    auto id = stmt.column_int64(0);
    auto age = stmt.column_int64(1);
    std::cout << "User " << id << " is " << age << " years old" << std::endl;
}
```

### Double access

```cpp
auto stmt = db.prepare("SELECT id, price FROM products");
while (stmt.step()) {
    auto id = stmt.column_int64(0);
    auto price = stmt.column_double(1);
    std::cout << "Product " << id << " costs " << price << std::endl;
}
```

### Text access

```cpp
auto stmt = db.prepare("SELECT name, email FROM users");
while (stmt.step()) {
    auto name = stmt.column_text(0);
    auto email = stmt.column_text(1);
    std::cout << name << " <" << email << ">" << std::endl;
}
```

### Blob access

```cpp
auto stmt = db.prepare("SELECT data FROM blobs");
while (stmt.step()) {
    const void* blob = stmt.column_blob(0);
    size_t len = stmt.column_bytes(0);
    // Process len bytes of data...
}
```

### NULL checking

Check the column type before reading a value that might be NULL.

```cpp
auto stmt = db.prepare("SELECT name, email FROM users");
while (stmt.step()) {
    auto name = stmt.column_text(0);
    if (stmt.column_type(1) == wlite::ColumnType::Null) {
        std::cout << name << " has no email" << std::endl;
    } else {
        auto email = stmt.column_text(1);
        std::cout << name << " <" << email << ">" << std::endl;
    }
}
```

### Reading all columns dynamically

For queries where you do not know the column layout at compile time, iterate over all columns.

```cpp
void print_row(wlite::Statement& stmt) {
    for (int i = 0; i < stmt.column_count(); ++i) {
        auto name = stmt.column_name(i);
        auto type = stmt.column_type(i);

        std::cout << name << "=";

        switch (type) {
            case wlite::ColumnType::Null:
                std::cout << "NULL";
                break;
            case wlite::ColumnType::Integer:
                std::cout << stmt.column_int64(i);
                break;
            case wlite::ColumnType::Real:
                std::cout << stmt.column_double(i);
                break;
            case wlite::ColumnType::Text:
                std::cout << "\"" << stmt.column_text(i) << "\"";
                break;
            case wlite::ColumnType::Blob:
                std::cout << "[blob " << stmt.column_bytes(i) << " bytes]";
                break;
        }

        if (i < stmt.column_count() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
}

void dump_table(wlite::Database& db, const std::string& table_name) {
    auto stmt = db.prepare("SELECT * FROM " + table_name);
    while (stmt.step()) {
        print_row(stmt);
    }
}
```

## Records

The `Row` type provides a higher-level, name-based interface over a result row. A row is created from a stepped statement and owns a snapshot of the row data.

### Creating a row

```cpp
auto stmt = db.prepare("SELECT id, name, email FROM users WHERE id = ?");
stmt.bind(1, 42LL);

if (stmt.step()) {
    auto row = stmt.row();
    auto id = row.int64(0);
    auto name = row.text(1);
    auto email = row.text(2);
    std::cout << id << ": " << name << " <" << email << ">" << std::endl;
}
```

### Row independence

A row is independent of the statement. You can continue stepping or finalize the statement while keeping the row.

```cpp
std::vector<wlite::Row> rows;

auto stmt = db.prepare("SELECT id, name FROM users");
while (stmt.step()) {
    rows.push_back(stmt.row());
}

// stmt is no longer needed, but rows are valid
for (const auto& row : rows) {
    auto id = row.int64(0);
    auto name = row.text(1);
    std::cout << id << ": " << name << std::endl;
}
```

### Name-based column lookup

You can look up columns by name instead of index.

```cpp
auto stmt = db.prepare("SELECT id, name, email FROM users WHERE id = ?");
stmt.bind(1, 42LL);

if (stmt.step()) {
    auto row = stmt.row();
    int name_idx = row.find("name");
    if (name_idx >= 0) {
        std::cout << "Name: " << row.text(name_idx) << std::endl;
    }
}
```

### Collecting rows into a vector

A common pattern is to collect all rows into a vector for later processing.

```cpp
struct User {
    int64_t id;
    std::string name;
    std::string email;
};

std::vector<User> get_all_users(wlite::Database& db) {
    auto stmt = db.prepare("SELECT id, name, email FROM users ORDER BY name");
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
```

## Transactions

Transactions ensure that a group of operations either all succeed or all fail. The `wlite::Transaction` type manages the transaction lifetime with RAII.

### Basic transaction

```cpp
auto tx = db.begin();

auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
stmt.bind(1, "Alice");
stmt.step();

stmt.reset();
stmt.bind(1, "Bob");
stmt.step();

tx.commit();
```

### Automatic rollback on exception

If the transaction object is destroyed without being committed, it is rolled back automatically.

```cpp
try {
    auto tx = db.begin();

    auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
    stmt.bind(1, "Alice");
    stmt.step();

    stmt.reset();
    stmt.bind(1, "Bob");
    stmt.step();

    tx.commit();
} catch (const wlite::Error& e) {
    // tx is destroyed, rollback happens automatically
    std::cerr << "Error: " << e.what() << std::endl;
}
```

### Transaction with explicit rollback

You can explicitly roll back a transaction before it is committed.

```cpp
auto tx = db.begin();

auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
stmt.bind(1, "Alice");
stmt.step();

tx.rollback();

// Alice is not inserted
```

### Transaction for batch operations

Use a transaction to wrap a batch of inserts for better performance and atomicity.

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
        tx.rollback();
        throw;
    }
}
```

### Transaction for transfers

A classic use case is transferring funds between accounts. Both operations must succeed or both must fail.

```cpp
void transfer(wlite::Database& db, int64_t from, int64_t to, int64_t amount) {
    auto tx = db.begin();

    try {
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
        }

        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

### Nested operations within a transaction

You can execute multiple statements within a single transaction.

```cpp
void create_user_with_profile(
    wlite::Database& db,
    const std::string& name,
    const std::string& email,
    const std::string& bio
) {
    auto tx = db.begin();

    try {
        auto user_stmt = db.prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        user_stmt.bind(1, name);
        user_stmt.bind(2, email);
        user_stmt.step();

        auto profile_stmt = db.prepare(
            "INSERT INTO profiles (user_id, bio) VALUES (last_insert_rowid(), ?)"
        );
        profile_stmt.bind(1, bio);
        profile_stmt.step();

        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

## Savepoints

Savepoints provide nested transaction control within a transaction. You can roll back to a savepoint without rolling back the entire transaction.

### Basic savepoint

```cpp
auto tx = db.begin();

auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
stmt.bind(1, "Alice");
stmt.step();

// Create a savepoint
tx.savepoint("sp1");

stmt.reset();
stmt.bind(1, "Bob");
stmt.step();

// Roll back to the savepoint (undo Bob)
tx.rollback_to("sp1");
tx.release("sp1");

// Alice is still inserted, Bob is not

tx.commit();
```

### Multiple savepoints

You can create multiple savepoints and roll back to any of them.

```cpp
auto tx = db.begin();

auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");

// Insert Alice
stmt.bind(1, "Alice");
stmt.step();

tx.savepoint("sp1");

// Insert Bob
stmt.reset();
stmt.bind(1, "Bob");
stmt.step();

tx.savepoint("sp2");

// Insert Charlie
stmt.reset();
stmt.bind(1, "Charlie");
stmt.step();

// Roll back to sp2 (undo Charlie)
tx.rollback_to("sp2");
tx.release("sp2");

// Roll back to sp1 (undo Bob)
tx.rollback_to("sp1");
tx.release("sp1");

// Only Alice is inserted

tx.commit();
```

### Savepoint for partial rollback

Use savepoints to undo a subset of operations within a transaction.

```cpp
void process_orders(wlite::Database& db, const std::vector<Order>& orders) {
    auto tx = db.begin();

    try {
        auto stmt = db.prepare(
            "INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)"
        );

        for (size_t i = 0; i < orders.size(); ++i) {
            auto savepoint_name = "sp_" + std::to_string(i);
            tx.savepoint(savepoint_name);

            try {
                stmt.bind(1, orders[i].user_id);
                stmt.bind(2, orders[i].product_id);
                stmt.bind(3, orders[i].quantity);
                stmt.step();
                stmt.reset();

                tx.release(savepoint_name);
            } catch (...) {
                tx.rollback_to(savepoint_name);
                tx.release(savepoint_name);
                std::cerr << "Failed to insert order " << i << ", skipping" << std::endl;
            }
        }

        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

### Savepoint naming conventions

Savepoint names must be unique within a transaction. A common convention is to use a prefix with an incrementing counter.

```cpp
int savepoint_counter = 0;

std::string next_savepoint() {
    return "sp_" + std::to_string(savepoint_counter++);
}

// Usage
auto tx = db.begin();
auto sp = next_savepoint();
tx.savepoint(sp);
// ... do work ...
tx.release(sp);
```

## Executing raw SQL

For statements that do not return rows, you can use `Database::execute` directly.

### Execute DDL

```cpp
db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
db.execute("CREATE INDEX idx_users_name ON users(name)");
```

### Execute DML

```cpp
db.execute("INSERT INTO users (name) VALUES ('Alice')");
db.execute("UPDATE users SET name = 'Bob' WHERE id = 1");
db.execute("DELETE FROM users WHERE id = 1");
```

### Execute with rows affected

```cpp
int64_t affected = 0;
db.execute("UPDATE users SET active = 0 WHERE last_login < '2024-01-01'", &affected);
std::cout << affected << " users deactivated" << std::endl;
```

### Query scalar

For queries that return a single value, use `query_scalar`.

```cpp
auto count = db.query_scalar("SELECT COUNT(*) FROM users");
std::cout << "Total users: " << count << std::endl;

auto max_id = db.query_scalar("SELECT MAX(id) FROM users");
std::cout << "Highest ID: " << max_id << std::endl;
```

## Complete query example

Here is a complete program that demonstrates all query patterns: preparing statements, binding parameters, stepping through results, accessing columns, using transactions, and working with savepoints.

```cpp
#include <wlite/wlite.hpp>
#include <iostream>
#include <string>
#include <vector>

struct User {
    int64_t id;
    std::string name;
    std::string email;
    bool active;
};

class UserService {
public:
    UserService(wlite::Database& db) : db_(db) {}

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
        auto stmt = db_.prepare("INSERT INTO users (name, email) VALUES (?, ?)");
        stmt.bind(1, name);
        stmt.bind(2, email);
        stmt.step();
    }

    void insert_users(const std::vector<std::pair<std::string, std::string>>& users) {
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
                stmt.column_int64(3) != 0,
            });
        }
        return users;
    }

    void deactivate_user(int64_t user_id) {
        auto stmt = db_.prepare("UPDATE users SET active = 0 WHERE id = ?");
        stmt.bind(1, user_id);
        stmt.step();
    }

    void delete_inactive() {
        auto stmt = db_.prepare("DELETE FROM users WHERE active = 0");
        stmt.step();
    }

    int64_t count_active() {
        return db_.query_scalar("SELECT COUNT(*) FROM users WHERE active = 1");
    }

    void transfer_ownership(int64_t from_id, int64_t to_id) {
        auto tx = db_.begin();
        try {
            auto stmt = db_.prepare(
                "UPDATE users SET email = (SELECT email FROM users WHERE id = ?) WHERE id = ?"
            );
            stmt.bind(1, from_id);
            stmt.bind(2, to_id);
            stmt.step();

            auto deactivate = db_.prepare("UPDATE users SET active = 0 WHERE id = ?");
            deactivate.bind(1, from_id);
            deactivate.step();

            tx.commit();
        } catch (...) {
            tx.rollback();
            throw;
        }
    }

private:
    wlite::Database& db_;
};

int main() {
    try {
        auto db = wlite::Database::open("app.db");
        UserService service(db);

        service.create_tables();

        // Batch insert
        std::vector<std::pair<std::string, std::string>> users = {
            {"Alice", "alice@example.com"},
            {"Bob", "bob@example.com"},
            {"Charlie", "charlie@example.com"},
        };
        service.insert_users(users);

        // List all users
        std::cout << "All users:" << std::endl;
        for (const auto& user : service.list_users()) {
            std::cout << "  " << user.id << ": " << user.name
                      << " <" << user.email << ">"
                      << (user.active ? "" : " [inactive]")
                      << std::endl;
        }

        // Count active users
        std::cout << "Active users: " << service.count_active() << std::endl;

        // Deactivate a user
        service.deactivate_user(2);

        // Count again
        std::cout << "Active users after deactivation: " << service.count_active() << std::endl;

        // Delete inactive users
        service.delete_inactive();
        std::cout << "Active users after cleanup: " << service.count_active() << std::endl;

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```
