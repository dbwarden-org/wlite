---
title: C++ Migration Reference
description: Model loading, database migration, diff, plan, check, snapshot, hash, and compiled models in the wlite C++ binding.
---

# C++ Migration Reference

The wlite C++ binding provides a complete migration system. You load a schema model from a `.wlite` file, open a database, and apply migrations. The binding handles diffing, planning, and executing all necessary SQL statements, including full table rebuilds when SQLite cannot express a change with `ALTER TABLE`.

This document covers model loading, database opening, migration, diff, plan, check, snapshot, hash, and compiled model loading. All examples use the C++ binding with RAII types and exception-based error handling.

## Model loading

The `wlite::Model` type represents a parsed `.wlite` schema. It is immutable after loading and can be shared across threads.

### Loading from a file

The most common way to load a model is from a `.wlite` file on disk.

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");

        // model is now loaded and ready to use
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Failed to load model: " << e.what() << std::endl;
        return 1;
    }
}
```

If the file does not exist, `Model::load` throws a `wlite::Error` with code `WLITE_IO_ERROR`. If the file contains invalid syntax, it throws with code `WLITE_PARSE_ERROR`.

### Loading from a string

You can parse a model directly from a string. This is useful for testing or when the schema is embedded in your application.

```cpp
#include <wlite/wlite.hpp>
#include <string>

int main() {
    try {
        const std::string source = R"(
            model users {
                id: integer pk
                name: text not null
                email: text not null unique
            }
        )";

        auto model = wlite::Model::load(source);
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Loading from a compiled binary

For faster startup, you can compile a `.wlite` file into a `.wlitem` binary and load it at runtime. Compiled models skip parsing entirely.

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load_compiled("app.wlitem");
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Failed to load compiled model: " << e.what() << std::endl;
        return 1;
    }
}
```

Compiled models are created by the `wlite compile` command or programmatically through the C API. They are smaller and faster to load than source `.wlite` files.

### Loading from a memory buffer

You can load a model from a memory buffer. This is useful when the schema is embedded in a binary or loaded from a network resource.

```cpp
#include <wlite/wlite.hpp>
#include <string>

void load_from_buffer(const char* data, size_t size) {
    try {
        auto model = wlite::Model::load(data, size);
        // use model...
    } catch (const wlite::Error& e) {
        std::cerr << "Failed to load model: " << e.what() << std::endl;
    }
}
```

### Sharing models across threads

Models are immutable after loading and can be safely shared across threads. Use `std::shared_ptr` to manage the lifetime.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <thread>
#include <vector>

int main() {
    try {
        auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([model, i]() {
                auto db = wlite::Database::open("app.db");
                db.migrate(*model);
                std::cout << "Thread " << i << " migrated" << std::endl;
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

Each thread gets its own database connection, but they all share the same model. This is safe because the model is immutable.

## Database opening

The `wlite::Database` type manages a connection to an SQLite database. It is opened through the static factory method `Database::open`.

### Opening a database

```cpp
auto db = wlite::Database::open("app.db");
```

If the database file does not exist, it is created. If the file exists, it is opened. Foreign key enforcement is enabled by default.

### Opening with options

You can control readonly mode, file creation, foreign key enforcement, and busy timeout.

```cpp
auto db = wlite::Database::open("app.db", wlite::OpenOptions{
    .readonly = false,
    .create = true,
    .foreign_keys = true,
    .busy_timeout_ms = 5000,
});
```

The `OpenOptions` struct mirrors the `wlite_open_options` struct from the C API. All fields have sensible defaults.

### Opening a readonly database

For applications that only read data, open the database in readonly mode. This prevents accidental modifications and allows opening databases on read-only filesystems.

```cpp
auto db = wlite::Database::open("app.db", wlite::OpenOptions{
    .readonly = true,
    .create = false,
});
```

### Opening with a busy timeout

When multiple processes access the same database, one process may be locked out. The busy timeout determines how long to wait before returning a `WLITE_BUSY` error.

```cpp
auto db = wlite::Database::open("app.db", wlite::OpenOptions{
    .busy_timeout_ms = 10000, // wait up to 10 seconds
});
```

## Migration

Migration is the process of comparing a live database schema against a model and applying the necessary changes to bring them into alignment.

### Basic migration

The simplest migration loads a model, opens a database, and calls `migrate`.

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        db.migrate(model);

        std::cout << "Migration complete" << std::endl;
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Migration failed: " << e.what() << std::endl;
        return 1;
    }
}
```

If the database is brand new (empty), `migrate` creates all tables, indexes, and constraints defined in the model. If the database already exists, `migrate` compares the live schema against the model and applies only the differences.

### What migration does

The migration process follows these steps:

1. The live database schema is introspected into an in-memory schema object.
2. The desired schema (from the model) is compared against the live schema.
3. Differences are classified as additive, subtractive, alternative, or rebuild-required.
4. A migration plan is generated with the correct SQL statements.
5. The plan is executed within a transaction with foreign keys disabled.
6. A checksum is stored to verify the migration was applied correctly.

Additive changes (new tables, new columns) are applied with direct DDL. Subtractive and alternative changes (dropped columns, type changes, constraint modifications) require a full table rebuild because SQLite does not support most `ALTER TABLE` operations.

### Migration with shared_ptr model

When using `std::shared_ptr<Model>`, pass a reference to the model to `migrate`.

```cpp
#include <wlite/wlite.hpp>
#include <memory>

int main() {
    try {
        auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));
        auto db = wlite::Database::open("app.db");

        db.migrate(model);

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Migration in a class

A common pattern is to wrap the model and database in a class that performs migration in the constructor.

```cpp
#include <wlite/wlite.hpp>
#include <memory>
#include <string>

class AppDatabase {
public:
    AppDatabase(const std::string& db_path, const std::string& model_path)
        : model_(std::make_shared<wlite::Model>(wlite::Model::load(model_path)))
        , db_(wlite::Database::open(db_path)) {
        db_.migrate(*model_);
    }

    wlite::Database& db() { return db_; }

private:
    std::shared_ptr<wlite::Model> model_;
    wlite::Database db_;
};

int main() {
    try {
        AppDatabase app("app.db", "app.wlite");
        // database is migrated and ready to use
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

## Diff

The diff operation compares two schemas and produces a list of differences. This is useful for understanding what changes a migration will make before applying them.

### Computing a diff

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        auto diff = db.diff(model);

        std::cout << "Found " << diff.size() << " differences" << std::endl;

        for (const auto& entry : diff) {
            std::cout << "  " << entry.detail << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Diff entries

Each diff entry contains information about the type of change, the safety level, the affected table, and a human-readable description.

| Field | Type | Description |
|-------|------|-------------|
| `op` | `DiffOp` | The type of operation (add table, drop column, etc.) |
| `safety` | `Safety` | Safety level (safe, requires rebuild, destructive) |
| `table` | `std::string` | The affected table name |
| `object` | `std::string` | The affected object name (column, index, etc.) |
| `detail` | `std::string` | Human-readable description of the change |

### Safety levels

| Level | Meaning |
|-------|---------|
| `Safe` | Direct DDL, no data loss risk |
| `RequiresRebuild` | Table rebuild needed, data is preserved |
| `Destructive` | Data may be lost (dropped table, dropped column) |
| `Conditional` | Change may fail depending on existing data |
| `Irreversible` | Cannot be undone with rollback SQL |

### Inspecting diff before migration

Always inspect the diff before running a migration on production data. This lets you verify that the changes are what you expect.

```cpp
#include <wlite/wlite.hpp>

void preview_migration(const std::string& db_path, const std::string& model_path) {
    try {
        auto model = wlite::Model::load(model_path);
        auto db = wlite::Database::open(db_path);

        auto diff = db.diff(model);

        if (diff.empty()) {
            std::cout << "Database is up to date" << std::endl;
            return;
        }

        std::cout << "Migration will apply " << diff.size() << " changes:" << std::endl;
        for (const auto& entry : diff) {
            const char* safety_str = "safe";
            if (entry.safety == wlite::Safety::RequiresRebuild) safety_str = "requires rebuild";
            if (entry.safety == wlite::Safety::Destructive) safety_str = "destructive";
            if (entry.safety == wlite::Safety::Irreversible) safety_str = "irreversible";

            std::cout << "  [" << safety_str << "] " << entry.detail << std::endl;
        }
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

## Plan

The plan operation generates a concrete migration plan with SQL statements. It is similar to diff but produces executable SQL instead of a list of abstract differences.

### Generating a plan

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        auto plan = db.plan(model);

        std::cout << "Migration has " << plan.size() << " steps" << std::endl;

        for (size_t i = 0; i < plan.size(); ++i) {
            std::cout << "Step " << i << ": " << plan[i].sql << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Plan steps

Each plan step contains the SQL to apply the change, optional rollback SQL, and metadata about the operation.

| Field | Type | Description |
|-------|------|-------------|
| `op` | `PlanOp` | The type of operation |
| `safety` | `Safety` | Safety level |
| `sql` | `std::string` | SQL to apply the change |
| `rollback_sql` | `std::string` | SQL to undo the change (empty if unavailable) |
| `table` | `std::string` | The affected table name |
| `detail` | `std::string` | Human-readable description |

### Applying a plan

You can apply a plan manually, step by step, instead of using `migrate`. This gives you control over which steps are executed and allows you to log or inspect each statement.

```cpp
#include <wlite/wlite.hpp>

void apply_plan_manually(wlite::Database& db, const wlite::Model& model) {
    auto plan = db.plan(model);

    if (plan.empty()) {
        std::cout << "No changes needed" << std::endl;
        return;
    }

    auto tx = db.begin();
    try {
        for (const auto& step : plan) {
            std::cout << "Executing: " << step.detail << std::endl;
            db.execute(step.sql);
        }
        tx.commit();
        std::cout << "Migration applied successfully" << std::endl;
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

### Plan with rollback SQL

Each plan step includes rollback SQL when available. This allows you to undo a migration.

```cpp
#include <wlite/wlite.hpp>

void apply_and_log_plan(wlite::Database& db, const wlite::Model& model) {
    auto plan = db.plan(model);

    auto tx = db.begin();
    try {
        for (size_t i = 0; i < plan.size(); ++i) {
            std::cout << "Step " << i << ": " << plan[i].sql << std::endl;
            db.execute(plan[i].sql);

            if (!plan[i].rollback_sql.empty()) {
                std::cout << "  Rollback: " << plan[i].rollback_sql << std::endl;
            }
        }
        tx.commit();
    } catch (...) {
        tx.rollback();
        throw;
    }
}
```

## Check

The check operation verifies that the live database schema matches the model. It returns a diff if there are any differences.

### Checking schema alignment

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        auto diff = db.check(model);

        if (diff.empty()) {
            std::cout << "Schema is up to date" << std::endl;
        } else {
            std::cout << "Schema is out of date by " << diff.size() << " changes" << std::endl;
            for (const auto& entry : diff) {
                std::cout << "  " << entry.detail << std::endl;
            }
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Using check in a build script

Check is useful in CI/CD pipelines to verify that migrations have been applied.

```cpp
#include <wlite/wlite.hpp>
#include <cstdlib>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        auto diff = db.check(model);

        if (!diff.empty()) {
            std::cerr << "Database schema is out of date. Run migrations first." << std::endl;
            return 1;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

## Snapshot

The snapshot operation captures the current state of the database schema as an in-memory schema object. This is useful for creating backups or for comparing against a future model.

### Capturing a snapshot

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto db = wlite::Database::open("app.db");

        auto snapshot = db.snapshot();

        // snapshot contains the current schema
        // use it for comparison or serialization
        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Snapshot for comparison

You can capture a snapshot and later compare it against a model to see what has changed.

```cpp
#include <wlite/wlite.hpp>

void compare_with_snapshot(wlite::Database& db, const wlite::Model& model) {
    auto before = db.snapshot();

    db.migrate(model);

    auto after = db.snapshot();

    auto diff = before.diff(after);
    std::cout << "Migration applied " << diff.size() << " changes" << std::endl;
}
```

### Saving a snapshot to a file

You can serialize a snapshot to JSON or DSL format for archival purposes.

```cpp
#include <wlite/wlite.hpp>
#include <fstream>

void save_snapshot(wlite::Database& db, const std::string& path) {
    auto snapshot = db.snapshot();

    std::ofstream out(path);
    out << snapshot.to_json();
    out.close();

    std::cout << "Snapshot saved to " << path << std::endl;
}
```

## Hash

The hash operation computes a fingerprint of a schema. This is useful for integrity checks and migration tracking.

### Computing a schema hash

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        auto db = wlite::Database::open("app.db");

        auto model_hash = model.hash();
        auto db_hash = db.schema_hash();

        if (model_hash == db_hash) {
            std::cout << "Schema is up to date" << std::endl;
        } else {
            std::cout << "Schema differs" << std::endl;
            std::cout << "Model hash: " << model_hash << std::endl;
            std::cout << "Database hash: " << db_hash << std::endl;
        }

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Using hashes for migration tracking

Hashes can be stored alongside migrations to verify that the correct schema state was achieved.

```cpp
#include <wlite/wlite.hpp>
#include <map>
#include <string>

class MigrationTracker {
public:
    MigrationTracker(wlite::Database& db) : db_(db) {}

    void record(const std::string& version, const wlite::Model& model) {
        auto hash = model.hash();
        auto stmt = db_.prepare(
            "INSERT INTO schema_versions (version, hash) VALUES (?, ?)"
        );
        stmt.bind(1, version);
        stmt.bind(2, hash);
        stmt.step();
    }

    bool verify(const wlite::Model& model) {
        auto stmt = db_.prepare(
            "SELECT hash FROM schema_versions ORDER BY id DESC LIMIT 1"
        );
        if (stmt.step()) {
            auto stored_hash = stmt.column_text(0);
            auto current_hash = model.hash();
            return stored_hash == current_hash;
        }
        return false;
    }

private:
    wlite::Database& db_;
};
```

## Compiled models

Compiled models are binary representations of a `.wlite` schema. They skip the parsing step, which makes them faster to load and smaller in size.

### Compiling a model

Use the `wlite compile` command to compile a `.wlite` file into a `.wlitem` binary.

```bash
wlite compile app.wlite -o app.wlitem
```

### Loading a compiled model

```cpp
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load_compiled("app.wlitem");
        auto db = wlite::Database::open("app.db");

        db.migrate(model);

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### Loading from a compiled buffer

You can load a compiled model from a memory buffer. This is useful when the compiled model is embedded in your application binary.

```cpp
#include <wlite/wlite.hpp>
#include <fstream>
#include <vector>

std::vector<char> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<char>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int main() {
    try {
        auto data = read_file("app.wlitem");
        auto model = wlite::Model::load_compiled(data.data(), data.size());

        auto db = wlite::Database::open("app.db");
        db.migrate(model);

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

### When to use compiled models

Compiled models are beneficial in these scenarios:

- **Startup time**: Compiled models load faster because they skip parsing.
- **Distribution**: Compiled models are smaller and do not require the parser at runtime.
- **Security**: Compiled models do not expose the schema source code.
- **Embedded systems**: Compiled models reduce memory usage and code size.

### Embedded compiled models

You can embed a compiled model directly in your C++ source using a raw string literal or a byte array.

```cpp
#include <wlite/wlite.hpp>

// Generated by: wlite compile app.wlite -o app.wlitem --hex
static const unsigned char compiled_model[] = {
    0x01, 0x02, 0x03, /* ... binary data ... */
};

int main() {
    try {
        auto model = wlite::Model::load_compiled(
            compiled_model, sizeof(compiled_model)
        );

        auto db = wlite::Database::open("app.db");
        db.migrate(model);

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

This pattern eliminates the need to ship `.wlite` or `.wlitem` files alongside your application.

## Complete migration example

Here is a complete program that demonstrates the full migration workflow: loading a model, checking the schema, previewing the diff, applying the migration, and verifying the result.

```cpp
#include <wlite/wlite.hpp>
#include <iostream>
#include <memory>

int main() {
    try {
        auto model = std::make_shared<wlite::Model>(wlite::Model::load("app.wlite"));
        auto db = wlite::Database::open("app.db");

        // Step 1: Check current state
        auto diff = db.diff(*model);

        if (diff.empty()) {
            std::cout << "Database is already up to date" << std::endl;
            return 0;
        }

        // Step 2: Preview changes
        std::cout << "Migration will apply " << diff.size() << " changes:" << std::endl;
        for (const auto& entry : diff) {
            std::cout << "  " << entry.detail << std::endl;
        }

        // Step 3: Generate and inspect plan
        auto plan = db.plan(*model);
        std::cout << "\nPlan has " << plan.size() << " steps:" << std::endl;
        for (size_t i = 0; i < plan.size(); ++i) {
            std::cout << "  Step " << i << ": " << plan[i].sql << std::endl;
        }

        // Step 4: Apply migration
        std::cout << "\nApplying migration..." << std::endl;
        db.migrate(*model);
        std::cout << "Migration complete" << std::endl;

        // Step 5: Verify
        auto verify_diff = db.diff(*model);
        if (verify_diff.empty()) {
            std::cout << "Verification passed" << std::endl;
        } else {
            std::cerr << "Verification failed: "
                      << verify_diff.size() << " remaining differences" << std::endl;
            return 1;
        }

        // Step 6: Record hash
        auto hash = model->hash();
        std::cout << "Schema hash: " << hash << std::endl;

        return 0;
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```
