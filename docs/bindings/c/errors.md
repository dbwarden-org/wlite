---
title: "Error Handling with the C Binding"
description: "Error codes, structured errors, cleanup patterns, thread safety, and memory ownership in the wlite C binding."
---

# Error Handling with the C Binding

This guide covers error handling in the wlite C binding. You will learn about all error codes, how to use `wlite_strerror` for human-readable messages, how to work with the structured `wlite_error` type, cleanup patterns that prevent resource leaks, thread safety considerations, and memory ownership rules.

## Error codes

All fallible functions in libwlite return `wlite_result`, an enum of error codes. The value `WLITE_OK` (0) means success. Any other value indicates an error.

### Complete list

| Code | Value | Meaning |
|------|-------|---------|
| `WLITE_OK` | 0 | Operation succeeded. |
| `WLITE_ERROR` | 1 | Generic or unspecified error. |
| `WLITE_INVALID_ARGUMENT` | 2 | A null pointer was passed where a valid pointer is required, or an argument is out of the valid range. |
| `WLITE_OUT_OF_MEMORY` | 3 | A memory allocation failed. |
| `WLITE_IO_ERROR` | 4 | A file system operation failed (file not found, permission denied, read/write error). |
| `WLITE_PARSE_ERROR` | 5 | The `.wlite` model file has a syntax error. |
| `WLITE_MODEL_ERROR` | 6 | The model is syntactically correct but has semantic errors (duplicate table names, invalid column types, broken references). |
| `WLITE_SQLITE_ERROR` | 7 | An error from the underlying SQLite engine. |
| `WLITE_CONSTRAINT_ERROR` | 8 | A UNIQUE, CHECK, or FOREIGN KEY constraint was violated. |
| `WLITE_NOT_FOUND` | 9 | A requested file, table, or column does not exist. |
| `WLITE_BUSY` | 10 | The database is locked by another connection and the busy timeout expired. |
| `WLITE_TRANSACTION_ERROR` | 11 | A transaction operation failed (e.g., commit on an already-committed transaction, or begin while a transaction is already active). |

### Legacy aliases

For backward compatibility, three legacy names are defined:

```c
#define WLITE_ERR_SYNTAX   WLITE_PARSE_ERROR
#define WLITE_ERR_NULL_PTR WLITE_INVALID_ARGUMENT
#define WLITE_ERR_IO       WLITE_IO_ERROR
```

These aliases are deprecated and will be removed in a future release. Use the canonical names from the table above.

## wlite_strerror

Convert an error code to a human-readable string:

```c
const char *wlite_strerror(wlite_result result);
```

The returned string is a static constant. You do not need to free it. It is valid for the lifetime of the program.

### Basic usage

```c
wlite_result r = wlite_open("missing.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Error %d: %s\n", r, wlite_strerror(r));
    return 1;
}
```

### Using with printf

```c
const char *path = "app.wlite";
WlSchema *schema = wl_schema_load(path, NULL);
if (!schema) {
    fprintf(stderr, "Failed to load schema from %s\n", path);
}
```

### Checking specific error codes

You can compare the return value directly against specific codes:

```c
wlite_result r = wlite_open("app.db", &db);

if (r == WLITE_NOT_FOUND) {
    fprintf(stderr, "Database file not found.\n");
} else if (r == WLITE_IO_ERROR) {
    fprintf(stderr, "Cannot read database file.\n");
} else if (r != WLITE_OK) {
    fprintf(stderr, "Unexpected error: %s\n", wlite_strerror(r));
}
```

### Handling different operations

Different functions return different error codes for the same underlying problem. Here is a summary of common scenarios:

| Operation | Likely errors |
|-----------|---------------|
| `wlite_open` | `WLITE_IO_ERROR`, `WLITE_NOT_FOUND`, `WLITE_SQLITE_ERROR` |
| `wlite_model_load_file` | `WLITE_IO_ERROR`, `WLITE_NOT_FOUND`, `WLITE_PARSE_ERROR` |
| `wlite_model_validate` | `WLITE_MODEL_ERROR` |
| `wlite_prepare` | `WLITE_SQLITE_ERROR`, `WLITE_INVALID_ARGUMENT` |
| `wlite_step` | `WLITE_SQLITE_ERROR`, `WLITE_BUSY`, `WLITE_CONSTRAINT_ERROR` |
| `wlite_migrate` | `WLITE_SQLITE_ERROR`, `WLITE_MODEL_ERROR`, `WLITE_IO_ERROR` |
| `wlite_begin` | `WLITE_TRANSACTION_ERROR`, `WLITE_SQLITE_ERROR` |
| `wlite_commit` | `WLITE_TRANSACTION_ERROR`, `WLITE_SQLITE_ERROR` |

## Structured errors

Some functions accept an optional `wlite_error **` parameter that returns detailed error information. This is more informative than a simple error code.

### The wlite_error struct

```c
typedef struct wlite_error {
    wlite_result code;      /* Error code */
    char *message;          /* Human-readable error message */
    char *subsystem;        /* Subsystem that generated the error */
    char *object;           /* Object involved (table name, file path, etc.) */
    int sqlite_code;        /* SQLite error code (if applicable) */
    int line;               /* Line number in the source file (if applicable) */
} wlite_error;
```

### Getting a structured error

Pass a non-NULL `wlite_error **` to functions that accept it:

```c
wlite_error *err = NULL;
WlSchema *schema = wl_schema_parse(src, len, &err);

if (!schema) {
    if (err) {
        fprintf(stderr, "Parse error:\n");
        fprintf(stderr, "  Code:    %d\n", err->code);
        fprintf(stderr, "  Message: %s\n", err->message);
        if (err->subsystem) {
            fprintf(stderr, "  System:  %s\n", err->subsystem);
        }
        if (err->object) {
            fprintf(stderr, "  Object:  %s\n", err->object);
        }
        if (err->sqlite_code != 0) {
            fprintf(stderr, "  SQLite:  %d\n", err->sqlite_code);
        }
        if (err->line != 0) {
            fprintf(stderr, "  Line:    %d\n", err->line);
        }
        wlite_error_free(err);
    } else {
        fprintf(stderr, "Parse error (no details available).\n");
    }
}
```

### Freeing structured errors

Always free structured errors with `wlite_error_free`:

```c
wlite_error *err = NULL;
WlPlan *plan = wl_plan_migration(current, desired, &err);

if (!plan) {
    if (err) {
        fprintf(stderr, "Plan failed: %s\n", err->message);
        wlite_error_free(err);
    }
}
```

### Passing NULL for error output

If you do not need detailed error information, pass `NULL`:

```c
WlSchema *schema = wl_schema_load("app.wlite", NULL);
```

This is fine for most use cases. The `wlite_result` return value still tells you whether the operation succeeded.

### Functions that support structured errors

The following functions accept a `wlite_error **` parameter:

- `wl_schema_parse`
- `wl_schema_load`
- `wl_schema_introspect`
- `wl_schema_diff`
- `wl_plan_migration`
- `wl_apply_plan`
- `wl_rollback_last`
- `wl_schema_verify`

All other functions return only `wlite_result`. Use `wlite_strerror` for those.

## Cleanup patterns

Proper cleanup is critical in C. Every resource allocated by libwlite must be freed. These patterns show the correct order of cleanup for various scenarios.

### Database cleanup

```c
wlite_db *db = NULL;
wlite_result r = wlite_open("app.db", &db);
if (r != WLITE_OK) {
    /* db is NULL, nothing to free */
    fprintf(stderr, "Open failed: %s\n", wlite_strerror(r));
    return 1;
}

/* ... use db ... */

wlite_close(db);
```

### Model cleanup

```c
wlite_model *model = NULL;
wlite_result r = wlite_model_load_file("app.wlite", &model);
if (r != WLITE_OK) {
    /* model is NULL, nothing to free */
    fprintf(stderr, "Load failed: %s\n", wlite_strerror(r));
    return 1;
}

/* ... use model ... */

wlite_model_free(model);
```

### Statement cleanup

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT * FROM users", &stmt);

/* ... use stmt ... */

wlite_stmt_finalize(stmt);
```

### Record cleanup

```c
while (wlite_step(stmt) == WLITE_OK) {
    wlite_record *rec = wlite_record_from_stmt(stmt);
    /* ... use rec ... */
    wlite_record_free(rec);
}
```

### Transaction cleanup

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

/* ... do work ... */

/* Always end with either wlite_commit or wlite_rollback */
if (failed) {
    wlite_rollback(tx);
} else {
    wlite_commit(tx);
}

/* Always free the transaction handle */
wlite_tx_free(tx);
```

### Schema cleanup

```c
WlSchema *schema = wl_schema_load("app.wlite", NULL);
/* ... use schema ... */
wl_schema_free(schema);
```

### Diff cleanup

```c
WlDiff *diff = wl_schema_diff(current, desired, NULL);
if (diff) {
    /* ... use diff ... */
    wl_diff_free(diff);
}
```

### Plan cleanup

```c
WlPlan *plan = wl_plan_migration(current, desired, NULL);
if (plan) {
    /* ... use plan ... */
    wl_plan_free(plan);
}
```

### Structured error cleanup

```c
wlite_error *err = NULL;
/* ... operation that may set err ... */
if (err) {
    /* ... use err ... */
    wlite_error_free(err);
}
```

### Hash cleanup

```c
char *hash = wl_schema_hash(schema);
/* ... use hash ... */
wlite_free(hash);
```

### Complete cleanup example

This example shows the full cleanup chain for a program that opens a database, loads a model, runs a query, and handles errors:

```c
#include <stdio.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_stmt *stmt = NULL;
    wlite_result r;
    int exit_code = 0;

    /* Load model */
    r = wlite_model_load_file("app.wlite", &model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Load model: %s\n", wlite_strerror(r));
        return 1;
    }

    /* Open database */
    r = wlite_open("app.db", &db);
    if (r != WLITE_OK) {
        fprintf(stderr, "Open database: %s\n", wlite_strerror(r));
        wlite_model_free(model);
        return 1;
    }

    /* Migrate */
    r = wlite_migrate(db, model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Migrate: %s\n", wlite_strerror(r));
        exit_code = 1;
        goto cleanup;
    }

    /* Query */
    r = wlite_prepare(db, "SELECT name FROM users", &stmt);
    if (r != WLITE_OK) {
        fprintf(stderr, "Prepare: %s\n", wlite_strerror(r));
        exit_code = 1;
        goto cleanup;
    }

    while (wlite_step(stmt) == WLITE_OK) {
        printf("%s\n", wlite_column_text(stmt, 0));
    }

cleanup:
    if (stmt) wlite_stmt_finalize(stmt);
    if (db) wlite_close(db);
    if (model) wlite_model_free(model);
    return exit_code;
}
```

The `goto cleanup` pattern is idiomatic C for centralized resource cleanup. It ensures every allocated resource is freed regardless of where an error occurs.

## Thread safety

libwlite handles are not thread-safe by default. Each thread must use its own set of handles.

### One handle per thread

```c
/* Thread 1 */
wlite_db *db1 = NULL;
wlite_open("app.db", &db1);
/* ... use db1 ... */
wlite_close(db1);

/* Thread 2 (independent) */
wlite_db *db2 = NULL;
wlite_open("app.db", &db2);
/* ... use db2 ... */
wlite_close(db2);
```

Both threads can open the same database file concurrently. SQLite handles the locking internally.

### Shared database with busy timeout

If multiple threads need to access the same database, open it once and set a busy timeout:

```c
wlite_open_options opts = {
    .readonly = 0,
    .create = 1,
    .foreign_keys = 1,
    .busy_timeout_ms = 10000,
};

wlite_db *db = NULL;
wlite_open_ex("app.db", &opts, &db);
```

The busy timeout causes SQLite to wait up to 10 seconds before returning `WLITE_BUSY`. Without a timeout, concurrent writes fail immediately.

### Models and schemas

Model and schema objects (`wlite_model`, `WlSchema`) are immutable after creation. They can be shared between threads without synchronization, as long as no thread frees them while another thread is using them.

### Statements

Statement handles (`wlite_stmt`) are not thread-safe. Each thread should prepare its own statements, even if they share a database handle.

### Error messages

`wlite_strerror` returns a pointer to a static string. It is safe to call from any thread.

Structured errors (`wlite_error`) are heap-allocated and thread-safe as long as only one thread accesses a given error struct at a time.

## Memory ownership

libwlite follows a consistent memory ownership model. Understanding which functions allocate memory and which free it prevents leaks and use-after-free bugs.

### Who allocates

| Function | Allocates | Free with |
|----------|-----------|-----------|
| `wlite_open` / `wlite_open_ex` | `wlite_db` | `wlite_close` |
| `wlite_model_load_*` | `wlite_model` | `wlite_model_free` |
| `wlite_prepare` | `wlite_stmt` | `wlite_stmt_finalize` |
| `wlite_record_from_stmt` | `wlite_record` | `wlite_record_free` |
| `wlite_begin` | `wlite_tx` | `wlite_tx_free` |
| `wl_schema_parse` / `wl_schema_load` | `WlSchema` | `wl_schema_free` |
| `wl_schema_introspect` / `wl_schema_inspect` | `WlSchema` | `wl_schema_free` |
| `wl_schema_diff` | `WlDiff` | `wl_diff_free` |
| `wl_plan_migration` | `WlPlan` | `wl_plan_free` |
| `wl_schema_hash` | `char*` | `wlite_free` |
| `wlite_strdup` | `char*` | `wlite_free` |
| `wlite_model_load_compiled` (via model) | `wlite_model` | `wlite_model_free` |
| `wl_model_load_compiled_raw` | `WlSchema` | `wl_schema_free` |

### Borrowed pointers

Some functions return pointers to memory owned by another object. These must not be freed independently:

- `wlite_column_text` returns a pointer valid until the next `wlite_step` or `wlite_stmt_finalize`
- `wlite_column_blob` returns a pointer valid until the next `wlite_step` or `wlite_stmt_finalize`
- `wlite_record_text` returns a pointer valid until the record is freed
- `wlite_record_blob` returns a pointer valid until the record is freed
- `wlite_table_name` returns a pointer valid until the schema is freed
- `wlite_field_name` returns a pointer valid until the schema is freed
- `wlite_model_table_at` returns a pointer valid until the model is freed
- `wlite_model_table` returns a pointer valid until the model is freed
- `wlite_table_field_at` returns a pointer valid until the model is freed
- `wlite_table_field` returns a pointer valid until the model is freed
- `wl_schema_model_name` returns a pointer valid until the schema is freed
- `wlite_strerror` returns a static string, never free it

### Dangling pointers

A common mistake is using a pointer after the object that owns it has been freed:

```c
/* WRONG: dangling pointer */
WlSchema *schema = wl_schema_load("app.wlite", NULL);
char *hash = wl_schema_hash(schema);
wl_schema_free(schema);
printf("Hash: %s\n", hash); /* UNDEFINED: hash points to freed memory */
wlite_free(hash);
```

The correct order is to use the hash before freeing the schema:

```c
/* CORRECT */
WlSchema *schema = wl_schema_load("app.wlite", NULL);
char *hash = wl_schema_hash(schema);
printf("Hash: %s\n", hash);
wlite_free(hash);
wl_schema_free(schema);
```

### Double free

Never free the same pointer twice:

```c
/* WRONG: double free */
wlite_db *db = NULL;
wlite_open("app.db", &db);
wlite_close(db);
wlite_close(db); /* UNDEFINED: double free */
```

### Freeing NULL

Passing NULL to any free function is safe and has no effect. This means you can write:

```c
wlite_stmt *stmt = NULL;
/* If prepare fails, stmt is still NULL */
wlite_prepare(db, sql, &stmt);

/* ... use stmt ... */

wlite_stmt_finalize(stmt); /* Safe even if stmt is NULL */
```

### Structured error ownership

When you receive a `wlite_error` through an output parameter, you own it and must free it with `wlite_error_free`. If the function succeeds, the error output parameter is not touched:

```c
wlite_error *err = NULL;
WlSchema *schema = wl_schema_parse(src, len, &err);

if (schema) {
    /* Success: err was not set */
    wl_schema_free(schema);
} else {
    /* Failure: err was set, we own it */
    if (err) {
        fprintf(stderr, "Error: %s\n", err->message);
        wlite_error_free(err);
    }
}
```

## Common error handling patterns

### Early return

The most straightforward pattern. Check each operation and return immediately on failure:

```c
int do_work(wlite_db *db) {
    wlite_stmt *stmt = NULL;
    wlite_result r;

    r = wlite_prepare(db, "SELECT 1", &stmt);
    if (r != WLITE_OK) return -1;

    while (wlite_step(stmt) == WLITE_OK) {
        /* ... */
    }

    wlite_stmt_finalize(stmt);
    return 0;
}
```

### Goto cleanup

Useful when multiple resources need cleanup on any failure:

```c
int do_work(wlite_db *db) {
    wlite_model *model = NULL;
    wlite_stmt *stmt = NULL;
    int result = -1;

    if (wlite_model_load_file("app.wlite", &model) != WLITE_OK)
        goto done;

    if (wlite_prepare(db, "SELECT 1", &stmt) != WLITE_OK)
        goto done;

    while (wlite_step(stmt) == WLITE_OK) {
        /* ... */
    }

    result = 0;

done:
    if (stmt) wlite_stmt_finalize(stmt);
    if (model) wlite_model_free(model);
    return result;
}
```

### Retry on WLITE_BUSY

For concurrent access, you may want to retry when the database is busy:

```c
int execute_with_retry(wlite_db *db, const char *sql) {
    for (int attempt = 0; attempt < 3; attempt++) {
        int64_t affected = 0;
        wlite_result r = wlite_execute(db, sql, &affected);
        if (r == WLITE_OK) return 0;
        if (r != WLITE_BUSY) return -1;

        /* Wait a bit before retrying */
        struct timespec ts = {0, 100000000}; /* 100ms */
        nanosleep(&ts, NULL);
    }
    return -1;
}
```

### Conditional error detail

Use structured errors for operations where you need detailed diagnostics, and plain error codes for everything else:

```c
/* Simple case: just check the result */
wlite_result r = wlite_open("app.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Open: %s\n", wlite_strerror(r));
    return 1;
}

/* Complex case: get detailed error information */
wlite_error *err = NULL;
WlSchema *schema = wl_schema_parse(dsl, len, &err);
if (!schema) {
    if (err) {
        fprintf(stderr, "Parse error at line %d in %s: %s\n",
                err->line,
                err->subsystem ? err->subsystem : "unknown",
                err->message);
        wlite_error_free(err);
    }
    return 1;
}
```

## Next steps

- [Migration](migration.md): Load models, diff schemas, and run migrations
- [Queries](queries.md): Prepare statements, bind parameters, and step through results
- [C API Reference](../../c-api.md): Complete function-by-function reference
