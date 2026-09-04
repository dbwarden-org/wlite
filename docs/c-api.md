---
title: C API Reference
description: Complete C API reference for libwlite, a tiny SQLite schema and migration engine.
---

# wlite C API Reference

Version 0.2.0 (ABI v1).

## Version

Query the library and ABI version.

```c
#define WLITE_ABI_VERSION 1
#define WLITE_VERSION_MAJOR 0
#define WLITE_VERSION_MINOR 2
#define WLITE_VERSION_PATCH 0

int wlite_abi_version(void);
const char *wlite_version(void);
```

```c
printf("wlite %s (ABI v%d)\n", wlite_version(), wlite_abi_version());
```

## Error Handling

All fallible functions return `wlite_result`. Use `wlite_strerror` to get a human-readable message.

```c
typedef enum {
    WLITE_OK = 0,
    WLITE_ERROR,
    WLITE_INVALID_ARGUMENT,
    WLITE_OUT_OF_MEMORY,
    WLITE_IO_ERROR,
    WLITE_PARSE_ERROR,
    WLITE_MODEL_ERROR,
    WLITE_SQLITE_ERROR,
    WLITE_CONSTRAINT_ERROR,
    WLITE_NOT_FOUND,
    WLITE_BUSY,
    WLITE_TRANSACTION_ERROR,
} wlite_result;
```

Legacy aliases exist: `WLITE_ERR_SYNTAX` maps to `WLITE_PARSE_ERROR`, `WLITE_ERR_NULL_PTR` maps to `WLITE_INVALID_ARGUMENT`, `WLITE_ERR_IO` maps to `WLITE_IO_ERROR`.

```c
const char *wlite_strerror(wlite_result result);
```

Some functions also return a structured error:

```c
typedef struct wlite_error {
    wlite_result code;
    char *message;
    char *subsystem;
    char *object;
    int sqlite_code;
    int line;
} wlite_error;

void wlite_error_free(wlite_error *err);
```

```c
wlite_result r = wlite_open("missing.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Error %d: %s\n", r, wlite_strerror(r));
}

// Structured error example
wlite_error *err = NULL;
WlSchema *schema = wl_schema_parse(src, len, &err);
if (!schema) {
    fprintf(stderr, "Parse failed in %s: %s\n", err->subsystem, err->message);
    wlite_error_free(err);
}
```

## Database

```c
typedef struct {
    int readonly;
    int create;
    int foreign_keys;
    int busy_timeout_ms;
} wlite_open_options;

wlite_result wlite_open(const char *path, wlite_db **out);
wlite_result wlite_open_ex(const char *path, const wlite_open_options *options, wlite_db **out);
void wlite_close(wlite_db *db);
```

`wlite_open` opens or creates a database with default settings. `wlite_open_ex` lets you control readonly mode, file creation, foreign key enforcement, and busy timeout.

```c
wlite_db *db = NULL;
wlite_result r = wlite_open("app.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", wlite_strerror(r));
    return 1;
}

// Use the database...

wlite_close(db);
```

```c
wlite_open_options opts = {
    .readonly = 0,
    .create = 1,
    .foreign_keys = 1,
    .busy_timeout_ms = 5000,
};
wlite_open_ex("app.db", &opts, &db);
```

## SQL Execution

```c
wlite_result wlite_execute(wlite_db *db, const char *sql, int64_t *rows_affected);
```

Execute one or more SQL statements that return no result set (DDL, DML). Pass `NULL` for `rows_affected` if you do not need the count.

```c
int64_t affected = 0;
wlite_execute(db, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)", &affected);
wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", &affected);
printf("%lld rows affected\n", affected);
```

## Prepared Statements

```c
wlite_result wlite_prepare(wlite_db *db, const char *sql, wlite_stmt **out);
wlite_result wlite_bind_null(wlite_stmt *stmt, int index);
wlite_result wlite_bind_int64(wlite_stmt *stmt, int index, int64_t value);
wlite_result wlite_bind_double(wlite_stmt *stmt, int index, double value);
wlite_result wlite_bind_text(wlite_stmt *stmt, int index, const char *value);
wlite_result wlite_bind_text_n(wlite_stmt *stmt, int index, const char *value, size_t length);
wlite_result wlite_bind_blob(wlite_stmt *stmt, int index, const void *data, size_t size);
wlite_result wlite_step(wlite_stmt *stmt);
void wlite_stmt_reset(wlite_stmt *stmt);
void wlite_stmt_finalize(wlite_stmt *stmt);
```

Prepare a SQL statement, bind parameters (1-indexed), step through result rows, and finalize. `wlite_step` returns `WLITE_OK` for each row and a non-zero value when done or on error. `wlite_stmt_reset` resets a statement so it can be re-bound and re-executed.

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT * FROM users WHERE id = ? AND name = ?", &stmt);

wlite_bind_int64(stmt, 1, 42);
wlite_bind_text(stmt, 2, "Alice");

while (wlite_step(stmt) == WLITE_OK) {
    const char *name = wlite_column_text(stmt, 1);
    printf("User: %s\n", name);
}

wlite_stmt_finalize(stmt);
```

Bind a text value with an explicit length:

```c
wlite_bind_text_n(stmt, 1, "hello", 5);
```

Bind binary data:

```c
uint8_t bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
wlite_bind_blob(stmt, 1, bytes, sizeof(bytes));
```

Reuse a statement by resetting it:

```c
wlite_stmt_reset(stmt);
wlite_bind_int64(stmt, 1, 99);
while (wlite_step(stmt) == WLITE_OK) { /* ... */ }
```

## Column Access

```c
typedef enum {
    WLITE_TYPE_NULL, WLITE_TYPE_INTEGER, WLITE_TYPE_REAL, WLITE_TYPE_TEXT, WLITE_TYPE_BLOB
} wlite_value_type;

int wlite_column_count(wlite_stmt *stmt);
const char *wlite_column_name(wlite_stmt *stmt, int column);
wlite_value_type wlite_column_type(wlite_stmt *stmt, int column);
int64_t wlite_column_int64(wlite_stmt *stmt, int column);
double wlite_column_double(wlite_stmt *stmt, int column);
const char *wlite_column_text(wlite_stmt *stmt, int column);
const void *wlite_column_blob(wlite_stmt *stmt, int column);
size_t wlite_column_bytes(wlite_stmt *stmt, int column);
```

Access column metadata and values after a successful `wlite_step`. Columns are 0-indexed. `wlite_column_bytes` returns the size of the value in bytes (for both text and blob).

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT id, name, email FROM users", &stmt);

int cols = wlite_column_count(stmt);
printf("Query returns %d columns\n", cols);

while (wlite_step(stmt) == WLITE_OK) {
    int64_t id = wlite_column_int64(stmt, 0);
    const char *name = wlite_column_text(stmt, 1);
    const char *email = wlite_column_text(stmt, 2);

    wlite_value_type type = wlite_column_type(stmt, 2);
    if (type == WLITE_TYPE_NULL) {
        printf("%lld: %s (no email)\n", id, name);
    } else {
        printf("%lld: %s <%s>\n", id, name, email);
    }
}

wlite_stmt_finalize(stmt);
```

Reading blob data:

```c
while (wlite_step(stmt) == WLITE_OK) {
    const void *blob = wlite_column_blob(stmt, 0);
    size_t len = wlite_column_bytes(stmt, 0);
    // Process blob bytes...
}
```

## Record API

The record API provides a higher-level, name-based interface over a result row. A record is created from a stepped statement and owns a snapshot of the row data.

```c
wlite_record *wlite_record_from_stmt(wlite_stmt *stmt);
void wlite_record_free(wlite_record *record);
int wlite_record_column_count(const wlite_record *record);
const char *wlite_record_column_name(const wlite_record *record, int index);
wlite_value_type wlite_record_column_type(const wlite_record *record, int index);
int wlite_record_find(const wlite_record *record, const char *name);
int64_t wlite_record_int64(const wlite_record *record, int index);
double wlite_record_double(const wlite_record *record, int index);
const char *wlite_record_text(const wlite_record *record, int index);
const void *wlite_record_blob(const wlite_record *record, int index);
size_t wlite_record_blob_bytes(const wlite_record *record, int index);
```

`wlite_record_from_stmt` captures the current row. The record is independent of the statement, so you can continue stepping or finalize the statement while keeping the record. `wlite_record_find` returns the column index for a name, or -1 if not found.

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT id, name, email FROM users WHERE id = 1", &stmt);

while (wlite_step(stmt) == WLITE_OK) {
    wlite_record *rec = wlite_record_from_stmt(stmt);

    int idx = wlite_record_find(rec, "name");
    if (idx >= 0) {
        printf("Name: %s\n", wlite_record_text(rec, idx));
    }

    int64_t id = wlite_record_int64(rec, 0);
    printf("ID: %lld\n", id);

    wlite_record_free(rec);
}

wlite_stmt_finalize(stmt);
```

## Transaction API

```c
wlite_result wlite_begin(wlite_db *db, wlite_tx **out);
wlite_result wlite_commit(wlite_tx *tx);
wlite_result wlite_rollback(wlite_tx *tx);
void wlite_tx_free(wlite_tx *tx);
```

Begin, commit, or rollback a transaction. Always free the transaction handle when done.

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);
wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);

if (some_error) {
    wlite_rollback(tx);
} else {
    wlite_commit(tx);
}

wlite_tx_free(tx);
```

## Savepoints

```c
wlite_result wlite_savepoint(wlite_tx *tx, const char *name);
wlite_result wlite_release(wlite_tx *tx, const char *name);
wlite_result wlite_rollback_to(wlite_tx *tx, const char *name);
```

Nested transaction control via savepoints. You must release a savepoint after rolling back to it.

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);

wlite_savepoint(tx, "sp1");
wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);

// Undo just the second insert
wlite_rollback_to(tx, "sp1");
wlite_release(tx, "sp1");

// Alice is still inserted, Bob is not
wlite_commit(tx);
wlite_tx_free(tx);
```

## Schema Parsing

Parse a wlite DSL model into a schema structure.

```c
WlSchema *wl_schema_parse(const char *source, size_t length, wlite_error **error);
WlSchema *wl_schema_load(const char *path, wlite_error **error);
void wl_schema_free(WlSchema *schema);
```

```c
const char *dsl = "model users { id: integer pk, name: text not null }";
WlSchema *schema = wl_schema_parse(dsl, strlen(dsl), NULL);
if (schema) {
    printf("Parsed %zu tables\n", schema->table_count);
    wl_schema_free(schema);
}
```

```c
// Load from file
WlSchema *schema = wl_schema_load("schema.wlite", NULL);
wl_schema_free(schema);
```

## Schema Introspection

Introspect a live SQLite database into a schema structure, or inspect one held by a `wlite_db`.

```c
WlSchema *wl_schema_introspect(struct sqlite3 *db, wlite_error **error);
WlSchema *wl_schema_inspect(wlite_db *db, wlite_error **error);
```

`wl_schema_introspect` works with a raw `sqlite3*` pointer. `wl_schema_inspect` works with a `wlite_db` handle.

```c
WlSchema *live = wl_schema_inspect(db, NULL);
if (live) {
    printf("Live database has %zu tables\n", live->table_count);
    // Compare with desired schema...
    wl_schema_free(live);
}
```

## Model Identity

```c
const char *wl_schema_model_name(const WlSchema *schema);
int wl_schema_model_version(const WlSchema *schema);
```

Retrieve the model name and version declared in a parsed schema.

```c
WlSchema *schema = wl_schema_load("schema.wlite", NULL);
printf("Model: %s v%d\n", wl_schema_model_name(schema), wl_schema_model_version(schema));
wl_schema_free(schema);
```

## Schema Diff

Compare two schemas and produce a list of differences.

```c
typedef enum {
    WL_DIFF_ADD_TABLE, WL_DIFF_DROP_TABLE, WL_DIFF_RENAME_TABLE,
    WL_DIFF_ADD_COLUMN, WL_DIFF_DROP_COLUMN, WL_DIFF_RENAME_COLUMN, WL_DIFF_ALTER_COLUMN,
    WL_DIFF_ADD_INDEX, WL_DIFF_DROP_INDEX, WL_DIFF_ALTER_INDEX,
    WL_DIFF_ADD_CHECK, WL_DIFF_DROP_CHECK, WL_DIFF_ADD_UNIQUE, WL_DIFF_DROP_UNIQUE,
    WL_DIFF_ADD_FKEY, WL_DIFF_DROP_FKEY,
    WL_DIFF_ALTER_TABLE_OPTIONS, WL_DIFF_ALTER_VIEW, WL_DIFF_ALTER_TRIGGER,
    WL_DIFF_REBUILD_TABLE,
} WlDiffOp;

typedef enum {
    WL_SAFETY_SAFE = 0, WL_SAFETY_REQUIRES_REBUILD, WL_SAFETY_DESTRUCTIVE,
    WL_SAFETY_CONDITIONAL, WL_SAFETY_IRREVERSIBLE,
} WlSafety;

typedef struct {
    WlDiffOp op;
    WlSafety safety;
    char *table;
    char *object;
    char *detail;
} WlDiffEntry;

typedef struct {
    WlDiffEntry *entries;
    size_t entry_count;
} WlDiff;

WlDiff *wl_schema_diff(const WlSchema *current, const WlSchema *desired, wlite_error **error);
void wl_diff_free(WlDiff *diff);
```

```c
WlSchema *current = wl_schema_inspect(db, NULL);
WlSchema *desired = wl_schema_load("schema.wlite", NULL);

WlDiff *diff = wl_schema_diff(current, desired, NULL);
if (diff) {
    printf("%zu changes detected\n", diff->entry_count);
    for (size_t i = 0; i < diff->entry_count; i++) {
        printf("  op=%d safety=%d table=%s detail=%s\n",
               diff->entries[i].op, diff->entries[i].safety,
               diff->entries[i].table, diff->entries[i].detail);
    }
    wl_diff_free(diff);
}

wl_schema_free(current);
wl_schema_free(desired);
```

## Migration Plan

Convert a diff into a concrete migration plan with SQL statements.

```c
typedef enum {
    WL_PLAN_CREATE_TABLE, WL_PLAN_DROP_TABLE, WL_PLAN_RENAME_TABLE,
    WL_PLAN_ADD_COLUMN, WL_PLAN_DROP_COLUMN, WL_PLAN_RENAME_COLUMN, WL_PLAN_ALTER_COLUMN,
    WL_PLAN_REBUILD_TABLE, WL_PLAN_CREATE_INDEX, WL_PLAN_DROP_INDEX,
    WL_PLAN_ADD_CHECK, WL_PLAN_DROP_CHECK, WL_PLAN_ADD_UNIQUE, WL_PLAN_DROP_UNIQUE,
    WL_PLAN_ADD_FKEY, WL_PLAN_DROP_FKEY, WL_PLAN_CUSTOM_SQL,
} WlPlanOp;

typedef struct {
    WlPlanOp op;
    WlSafety safety;
    char *sql;
    char *rollback_sql;
    char *table;
    char *detail;
    int is_non_atomic;
} WlPlanStep;

typedef struct {
    WlPlanStep *steps;
    size_t step_count;
    char *schema_hash_before;
    char *schema_hash_after;
} WlPlan;

WlPlan *wl_plan_migration(const WlSchema *current, const WlSchema *desired, wlite_error **error);
void wl_plan_free(WlPlan *plan);
size_t wlite_plan_count(const WlPlan *plan);
```

Each plan step has `sql` to apply the change and `rollback_sql` to undo it. `schema_hash_before` and `schema_hash_after` record the schema fingerprint before and after the migration.

```c
WlPlan *plan = wl_plan_migration(current, desired, NULL);
if (plan) {
    printf("Migration has %zu steps\n", plan->step_count);
    for (size_t i = 0; i < plan->step_count; i++) {
        printf("Step %zu: %s\n", i, plan->steps[i].sql);
        if (plan->steps[i].rollback_sql) {
            printf("  rollback: %s\n", plan->steps[i].rollback_sql);
        }
    }
    wl_plan_free(plan);
}
```

## Single-Call Diff

Compare a live database against a model and produce a migration plan in one call.

```c
wlite_result wlite_diff(wlite_db *db, const wlite_model *model, WlPlan **out_plan);
```

```c
wlite_plan *plan = NULL;
wlite_result r = wlite_diff(db, model, &plan);
if (r == WLITE_OK && plan) {
    printf("Migration has %zu steps\n", wlite_plan_count(plan));
    // Apply or inspect...
    wl_plan_free(plan);
}
```

## Migration

```c
wlite_result wlite_migrate(wlite_db *db, const wlite_model *model);
```

Compare the live database against the model and apply all necessary changes.

```c
wlite_result r = wlite_migrate(db, model);
if (r != WLITE_OK) {
    fprintf(stderr, "Migration failed: %s\n", wlite_strerror(r));
}
```

## Advanced Migration

```c
wlite_result wl_apply_plan(wlite_db *db, const WlPlan *plan, wlite_error **error);
wlite_result wl_rollback_last(wlite_db *db, wlite_error **error);
wlite_result wl_schema_verify(wlite_db *db, const WlSchema *expected, WlDiff **difference, wlite_error **error);
```

`wl_apply_plan` executes a previously generated migration plan. `wl_rollback_last` undoes the most recent migration using stored rollback SQL. `wl_schema_verify` checks whether the live database matches an expected schema and returns the differences.

```c
// Apply a plan
wlite_result r = wl_apply_plan(db, plan, NULL);

// Undo it
r = wl_rollback_last(db, NULL);

// Verify the schema is correct
WlDiff *diff = NULL;
r = wl_schema_verify(db, expected, &diff, NULL);
if (r == WLITE_OK && diff && diff->entry_count > 0) {
    printf("Schema mismatch: %zu differences\n", diff->entry_count);
    wl_diff_free(diff);
}
```

## Model API

Load a `.wlite` model from file, memory, or a compiled `.wlitem` binary.

```c
wlite_result wlite_model_load_file(const char *path, wlite_model **out);
wlite_result wlite_model_load_memory(const void *data, size_t size, wlite_model **out);
wlite_result wlite_model_load_compiled(const void *data, size_t size, wlite_model **out);
wlite_result wlite_model_validate(const wlite_model *model);
void wlite_model_free(wlite_model *model);
```

```c
wlite_model *model = NULL;
wlite_result r = wlite_model_load_file("schema.wlite", &model);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to load model: %s\n", wlite_strerror(r));
    return 1;
}

r = wlite_model_validate(model);
if (r != WLITE_OK) {
    fprintf(stderr, "Invalid model: %s\n", wlite_strerror(r));
}

// Use the model for migration or introspection...

wlite_model_free(model);
```

Load from a memory buffer:

```c
const char *buf = "..."; // model content
wlite_model *model = NULL;
wlite_model_load_memory(buf, strlen(buf), &model);
```

Load from a precompiled binary:

```c
const void *compiled = load_file("schema.wlitem", &size);
wlite_model *model = NULL;
wlite_model_load_compiled(compiled, size, &model);
```

## Model Compilation

Compile a parsed schema into a binary `.wlitem` file for fast loading.

```c
int wl_model_compile(const WlSchema *schema, const char *path);
WlSchema *wl_model_load_compiled_raw(const void *data, size_t size);
```

```c
WlSchema *schema = wl_schema_load("schema.wlite", NULL);
int r = wl_model_compile(schema, "schema.wlitem");
wl_schema_free(schema);

// Later, load the compiled binary directly
WlSchema *loaded = wl_model_load_compiled_raw(compiled_data, compiled_size);
```

## Model Introspection

Navigate the tables and fields in a loaded model. Tables and fields are borrowed; do not free them separately.

```c
size_t wlite_model_table_count(const wlite_model *model);
const wlite_table *wlite_model_table_at(const wlite_model *model, size_t index);
const wlite_table *wlite_model_table(const wlite_model *model, const char *name);
const char *wlite_table_name(const wlite_table *table);
size_t wlite_table_field_count(const wlite_table *table);
const wlite_field *wlite_table_field_at(const wlite_table *table, size_t index);
const wlite_field *wlite_table_field(const wlite_table *table, const char *name);
const char *wlite_table_sql_name(const wlite_table *table);
const char *wlite_field_name(const wlite_field *field);
wlite_col_type wlite_field_type(const wlite_field *field);
unsigned wlite_field_flags(const wlite_field *field);
int wlite_field_is_nullable(const wlite_field *field);
int wlite_field_is_primary_key(const wlite_field *field);
int wlite_field_is_unique(const wlite_field *field);
int wlite_field_is_autoincrement(const wlite_field *field);
```

```c
wlite_model *model = NULL;
wlite_model_load_file("schema.wlite", &model);

size_t table_count = wlite_model_table_count(model);
printf("Model has %zu tables\n", table_count);

for (size_t i = 0; i < table_count; i++) {
    const wlite_table *table = wlite_model_table_at(model, i);
    printf("Table: %s\n", wlite_table_name(table));

    size_t field_count = wlite_table_field_count(table);
    for (size_t j = 0; j < field_count; j++) {
        const wlite_field *field = wlite_table_field_at(table, j);
        printf("  %s (%d)", wlite_field_name(field), wlite_field_type(field));
        if (wlite_field_is_primary_key(field)) printf(" PK");
        if (wlite_field_is_unique(field)) printf(" UNIQUE");
        if (!wlite_field_is_nullable(field)) printf(" NOT NULL");
        printf("\n");
    }
}

wlite_model_free(model);
```

Look up a table or field by name:

```c
const wlite_table *users = wlite_model_table(model, "users");
if (users) {
    const wlite_field *email = wlite_table_field(users, "email");
    if (email) {
        printf("email type: %d\n", wlite_field_type(email));
    }
}
```

## Blob Support

Bind and read binary data through prepared statements and column accessors.

```c
// Bind blob data
uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
wlite_bind_blob(stmt, 1, payload, sizeof(payload));

// Read blob data
while (wlite_step(stmt) == WLITE_OK) {
    const void *data = wlite_column_blob(stmt, 0);
    size_t len = wlite_column_bytes(stmt, 0);
    // Process len bytes of data...
}
```

The record API also supports blobs:

```c
wlite_record *rec = wlite_record_from_stmt(stmt);
const void *data = wlite_record_blob(rec, 0);
size_t len = wlite_record_blob_bytes(rec, 0);
wlite_record_free(rec);
```

## Serialization (wlite_writer)

Serialize a schema to JSON or DSL format using a writer interface.

```c
typedef struct wlite_writer {
    void *ctx;
    int (*write)(struct wlite_writer *w, const char *data, size_t len);
} wlite_writer;

int wl_schema_write_json(const WlSchema *schema, wlite_writer *w, wlite_error **error);
int wl_schema_write_dsl(const WlSchema *schema, wlite_writer *w, wlite_error **error);
```

Implement a writer callback to send output wherever you need it.

```c
// Example: write to a FILE*
int file_write(wlite_writer *w, const char *data, size_t len) {
    return (int)fwrite(data, 1, len, (FILE *)w->ctx) == (int)len ? 0 : -1;
}

FILE *fp = fopen("output.json", "w");
wlite_writer writer = { .ctx = fp, .write = file_write };
wl_schema_write_json(schema, &writer, NULL);
fclose(fp);
```

```c
// Example: write to a string buffer
typedef struct { char *buf; size_t len; size_t cap; } BufWriter;

int buf_write(wlite_writer *w, const char *data, size_t len) {
    BufWriter *bw = (BufWriter *)w->ctx;
    if (bw->len + len >= bw->cap) {
        bw->cap = (bw->cap + len) * 2;
        bw->buf = realloc(bw->buf, bw->cap);
    }
    memcpy(bw->buf + bw->len, data, len);
    bw->len += len;
    return 0;
}

BufWriter bw = {0};
wlite_writer writer = { .ctx = &bw, .write = buf_write };
wl_schema_write_dsl(schema, &writer, NULL);
printf("%.*s", (int)bw.len, bw.buf);
free(bw.buf);
```

## Schema Hashing

Compute a fingerprint of a schema for integrity checks and migration tracking.

```c
char *wl_schema_hash(const WlSchema *schema);
```

Returns a heap-allocated hash string. Free with `wlite_free`.

```c
WlSchema *schema = wl_schema_inspect(db, NULL);
char *hash = wl_schema_hash(schema);
printf("Schema fingerprint: %s\n", hash);
wlite_free(hash);
wl_schema_free(schema);
```

## SQLite Capabilities

Detect which SQLite features are available at runtime.

```c
typedef struct {
    int has_rename_column;
    int has_drop_column;
    int has_strict;
    int has_generated;
    int has_without_rowid;
} wlite_sqlite_caps;

void wl_sqlite_capabilities(struct sqlite3 *db, wlite_sqlite_caps *caps);
```

```c
wlite_sqlite_caps caps;
wl_sqlite_capabilities(db->sqlite, &caps);  // if you have the sqlite3* pointer

if (caps.has_drop_column) {
    printf("SQLite supports ALTER TABLE DROP COLUMN\n");
}
if (caps.has_strict) {
    printf("SQLite supports STRICT tables\n");
}
```

This is useful when planning migrations that depend on specific SQLite version features.

## Memory

```c
char *wlite_strdup(const char *s);
void wlite_free(void *p);
```

Use `wlite_strdup` to duplicate a string and `wlite_free` to release any memory allocated by libwlite (schema hashes, error messages, etc.).

```c
char *copy = wlite_strdup("hello");
// Use the copy...
wlite_free(copy);
```
