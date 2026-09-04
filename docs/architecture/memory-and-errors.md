---
title: Memory and Errors
description: Memory ownership rules, error handling patterns, and thread safety in wlite/libwlite.
---

# Memory and Errors

This page covers memory ownership, error handling, and thread safety. These are critical for using libwlite correctly from any language.

## Memory ownership

### The rule

Objects returned by `_create` or `_load` functions belong to the caller. Borrowed objects (returned by functions that do not allocate) are owned by their parent.

### Owned objects

These are allocated by libwlite and must be freed by the caller:

| Object | Free function | Notes |
|--------|---------------|-------|
| `wlite_db` | `wlite_close(db)` | Closes the SQLite connection |
| `wlite_model` | `wlite_model_free(model)` | Frees all tables, fields, indexes |
| `wlite_stmt` | `wlite_stmt_finalize(stmt)` | Finalizes the prepared statement |
| `wlite_tx` | `wlite_tx_free(tx)` | Frees the transaction handle |
| `wlite_plan` | `wlite_plan_free(plan)` | Frees the migration plan |
| `wlite_error` | `wlite_error_free(err)` | Frees the error object |

### Borrowed objects

These are owned by their parent and must NOT be freed separately:

| Object | Parent | Lifetime |
|--------|--------|----------|
| `wlite_table` | `wlite_model` | Valid until model is freed |
| `wlite_field` | `wlite_model` | Valid until model is freed |
| `wlite_index` | `wlite_model` | Valid until model is freed |
| Column text pointer | `wlite_stmt` | Valid until next step or finalize |

### Example: correct ownership

```c
wlite_model *model = NULL;
wlite_db *db = NULL;
wlite_stmt *stmt = NULL;

// Allocate
wlite_model_load_file("schema.wlite", &model);  // caller owns model
wlite_open("app.db", &db);                        // caller owns db
wlite_prepare(db, "SELECT * FROM users", &stmt);  // caller owns stmt

// Use borrowed objects
const wlite_table *table = wlite_model_table(model, "users");
const wlite_field *field = wlite_table_field(table, "name");
// Do NOT free table or field

// Free in reverse order
wlite_stmt_finalize(stmt);  // free stmt
wlite_close(db);             // free db
wlite_model_free(model);     // free model (also frees all tables and fields)
```

### Example: incorrect ownership (double free)

```c
wlite_model *model = NULL;
wlite_model_load_file("schema.wlite", &model);

const wlite_table *table = wlite_model_table(model, "users");
// WRONG: do not free borrowed objects
// wlite_table_free(table);  // BUG: double free

wlite_model_free(model);  // this already frees the table
```

### Example: use after free

```c
wlite_model *model = NULL;
wlite_model_load_file("schema.wlite", &model);

const wlite_table *table = wlite_model_table(model, "users");
wlite_model_free(model);

// WRONG: table is no longer valid
// const char *name = wlite_table_name(table);  // BUG: use after free
```

## Error handling

### Return codes

All libwlite functions return `wlite_result`. The value `WLITE_OK` (0) indicates success. Any other value indicates an error.

```c
wlite_result r = wlite_open("app.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Error: %s\n", wlite_strerror(r));
    return 1;
}
```

### Error codes

| Code | Value | Meaning |
|------|-------|---------|
| `WLITE_OK` | 0 | Success |
| `WLITE_ERROR` | 1 | General or unexpected error |
| `WLITE_INVALID_ARGUMENT` | 2 | Null pointer or invalid parameter |
| `WLITE_OUT_OF_MEMORY` | 3 | Memory allocation failed |
| `WLITE_IO_ERROR` | 4 | I/O error (disk full, permission denied, file not found) |
| `WLITE_PARSE_ERROR` | 5 | Schema parse error (malformed .wlite source) |
| `WLITE_MODEL_ERROR` | 6 | Schema model error (invalid table, missing field, etc.) |
| `WLITE_SQLITE_ERROR` | 7 | SQLite returned an error |
| `WLITE_CONSTRAINT_ERROR` | 8 | UNIQUE, CHECK, or FOREIGN KEY constraint violation |
| `WLITE_NOT_FOUND` | 9 | Requested table, column, or resource not found |
| `WLITE_BUSY` | 10 | Database is locked by another connection |
| `WLITE_TRANSACTION_ERROR` | 11 | Transaction failed or is in an invalid state |

### Error messages

Use `wlite_strerror` to convert an error code to a human-readable string:

```c
wlite_result r = wlite_open("missing.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Error %d: %s\n", r, wlite_strerror(r));
}
```

### Error objects

For detailed error information, libwlite populates a `wlite_error` struct directly. Access its fields:

```c
wlite_error *err = NULL;
wlite_result r = wlite_migrate(db, model);
if (r != WLITE_OK) {
    fprintf(stderr, "Migration failed: %s\n", wlite_strerror(r));
}
```

Some functions also accept an optional error pointer for more detailed diagnostics:

```c
wlite_error *err = NULL;
WlSchema *schema = wl_schema_load("schema.wlite", &err);
if (!schema) {
    if (err) {
        fprintf(stderr, "Parse error: %s\n", err->message);
        wlite_error_free(err);
    }
    return 1;
}
```

### Checking return values

Always check the return value of allocation functions:

```c
wlite_model *model = NULL;
wlite_result r = wlite_model_load_file("schema.wlite", &model);
if (r != WLITE_OK) {
    // model is NULL, do not use it
    return 1;
}
// model is valid, use it
```

Do not check return values of void functions or functions that cannot fail:

```c
// These cannot fail, no need to check
wlite_close(db);
wlite_model_free(model);
wlite_stmt_finalize(stmt);
```

### Panic on error

For simple programs where any error is fatal:

```c
#define CHECK(r) do { \
    if ((r) != WLITE_OK) { \
        fprintf(stderr, "Fatal: %s at %s:%d\n", wlite_strerror(r), __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

CHECK(wlite_open("app.db", &db));
CHECK(wlite_migrate(db, model));
```

## Thread safety

### Models

Models are immutable after loading. They can be shared across threads safely. Use `wlite_model_load_file` once, then pass the model pointer to any thread.

```c
// Thread-safe: sharing a model
wlite_model *model = NULL;
wlite_model_load_file("schema.wlite", &model);

// Thread 1
wlite_db *db1 = NULL;
wlite_open("db1.db", &db1);
wlite_migrate(db1, model);  // OK: model is immutable

// Thread 2
wlite_db *db2 = NULL;
wlite_open("db2.db", &db2);
wlite_migrate(db2, model);  // OK: model is immutable
```

### Database connections

Database connections are NOT thread-safe. Each thread must have its own connection. Do not share a `wlite_db` across threads.

```c
// WRONG: sharing a connection across threads
// Thread 1: wlite_prepare(db, "SELECT ...", &stmt1);
// Thread 2: wlite_prepare(db, "INSERT ...", &stmt2);  // DATA RACE

// CORRECT: each thread gets its own connection
// Thread 1: wlite_open("app.db", &db1); wlite_prepare(db1, ...);
// Thread 2: wlite_open("app.db", &db2); wlite_prepare(db2, ...);
```

### Statements

Statements belong to a database connection. Do not share a `wlite_stmt` across threads.

### Transactions

Transactions belong to a database connection. Do not share a `wlite_tx` across threads.

### Summary

| Object | Thread-safe? | Notes |
|--------|-------------|-------|
| `wlite_model` | Yes | Immutable after loading |
| `wlite_db` | No | One connection per thread |
| `wlite_stmt` | No | Belongs to a connection |
| `wlite_tx` | No | Belongs to a connection |
| `wlite_table` | Yes | Borrowed from model, immutable |
| `wlite_field` | Yes | Borrowed from model, immutable |

## Memory allocation

### Default allocator

libwlite uses the C standard library allocator (`malloc`, `realloc`, `free`) by default.

### Custom allocator

You can provide a custom allocator by calling `wlite_set_allocator` before any other function:

```c
void *my_alloc(size_t size) {
    return my_custom_malloc(size);
}

void *my_realloc(void *ptr, size_t size) {
    return my_custom_realloc(ptr, size);
}

void my_free(void *ptr) {
    my_custom_free(ptr);
}

int main(void) {
    wlite_set_allocator(my_alloc, my_realloc, my_free);
    // ... use libwlite
}
```

This is useful for embedded systems, games, or applications with custom memory pools.

### Memory reporting

Use `wlite_memory_usage` to check how much memory libwlite is currently using:

```c
size_t current, peak;
wlite_memory_usage(&current, &peak);
printf("Current: %zu bytes, Peak: %zu bytes\n", current, peak);
```

## Cleanup patterns

### C: goto cleanup

The idiomatic C pattern for cleanup on error is `goto`:

```c
int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_stmt *stmt = NULL;
    int exit_code = 0;

    if (wlite_model_load_file("schema.wlite", &model) != WLITE_OK) {
        exit_code = 1;
        goto done;
    }

    if (wlite_open("app.db", &db) != WLITE_OK) {
        exit_code = 1;
        goto done;
    }

    if (wlite_prepare(db, "SELECT * FROM users", &stmt) != WLITE_OK) {
        exit_code = 1;
        goto done;
    }

    while (wlite_step(stmt) == WLITE_OK) {
        printf("%s\n", wlite_column_text(stmt, 0));
    }

done:
    if (stmt) wlite_stmt_finalize(stmt);
    if (db) wlite_close(db);
    if (model) wlite_model_free(model);
    return exit_code;
}
```

### C: defer-like macros

For less boilerplate, use a scope-guard macro:

```c
#define DEFER(fn, ptr) __attribute__((cleanup(cleanup_##fn))) typeof(ptr) cleanup_##ptr = (ptr)
static void cleanup_close(wlite_db **p) { if (*p) wlite_close(*p); }
static void cleanup_model(wlite_model **p) { if (*p) wlite_model_free(*p); }
static void cleanup_stmt(wlite_stmt **p) { if (*p) wlite_stmt_finalize(*p); }
```

### Python: context managers

Python uses context managers for automatic cleanup:

```python
with wlite.Database.open("app.db") as db:
    with db.prepare("SELECT * FROM users") as stmt:
        while stmt.step():
            print(stmt.column_text(0))
    # stmt finalized, db closed automatically
```

### Rust: Drop trait

Rust uses `Drop` for automatic cleanup:

```rust
{
    let db = Database::open("app.db")?;
    let stmt = db.prepare("SELECT * FROM users")?;
    // ...
} // stmt dropped, then db dropped, both finalized automatically
```

### Go: defer

Go uses `defer` for cleanup:

```go
db, _ := wlite.Open("app.db")
defer db.Close()

stmt, _ := db.Prepare("SELECT * FROM users")
defer stmt.Finalize()
```

### C#: using statements

C# uses `using` for automatic disposal:

```csharp
using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT * FROM users");
// both disposed at end of scope
```

### Zig: defer

Zig uses `defer` for cleanup:

```zig
defer if (db) |d| wlite.wlite_close(d);
defer if (stmt) |s| wlite.wlite_stmt_finalize(s);
```
