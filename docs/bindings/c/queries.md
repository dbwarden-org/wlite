---
title: "Queries with the C Binding"
description: "Prepare statements, bind parameters, step through results, access columns, use the record API, manage transactions and savepoints, and handle binary data with the wlite C API."
---

# Queries with the C Binding

This guide covers everything you need to query a SQLite database through the wlite C binding. You will learn how to prepare statements, bind parameters, iterate over results, read column values, use the record API for name-based access, manage transactions with savepoints, and work with binary data.

## Preparing a statement

All SQL queries go through prepared statements. Use `wlite_prepare` to compile a SQL string into a statement handle:

```c
#include <stdio.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_db *db = NULL;
    wlite_open("app.db", &db);

    wlite_stmt *stmt = NULL;
    wlite_result r = wlite_prepare(db, "SELECT id, name FROM users", &stmt);
    if (r != WLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", wlite_strerror(r));
        wlite_close(db);
        return 1;
    }

    /* Use the statement... */

    wlite_stmt_finalize(stmt);
    wlite_close(db);
    return 0;
}
```

You can prepare multiple SQL statements in a single string by separating them with semicolons, but only the first statement is executed. For multiple statements, prepare and execute each one separately.

### When to finalize

Always finalize statements when you are done with them. Leaking statement handles causes memory and resource leaks. The recommended pattern is:

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, sql, &stmt);
/* ... use stmt ... */
wlite_stmt_finalize(stmt);
```

## Binding parameters

Prepared statements can have parameters represented by `?` placeholders. Bind values to these placeholders before stepping through results. Parameters are 1-indexed.

### Binding an integer

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT * FROM users WHERE id = ?", &stmt);
wlite_bind_int64(stmt, 1, 42);

while (wlite_step(stmt) == WLITE_OK) {
    /* Row with id = 42 */
}
wlite_stmt_finalize(stmt);
```

### Binding a double

```c
wlite_prepare(db, "SELECT * FROM products WHERE price < ?", &stmt);
wlite_bind_double(stmt, 1, 29.99);
```

### Binding text

```c
wlite_prepare(db, "SELECT * FROM users WHERE name = ?", &stmt);
wlite_bind_text(stmt, 1, "Alice");
```

Bind text with an explicit length to handle strings containing null bytes or to avoid calling `strlen`:

```c
wlite_bind_text_n(stmt, 1, "hello", 5);
```

### Binding NULL

```c
wlite_prepare(db, "SELECT * FROM users WHERE email IS ?", &stmt);
wlite_bind_null(stmt, 1);
```

### Binding a blob

```c
uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
wlite_prepare(db, "INSERT INTO data (blob_col) VALUES (?)", &stmt);
wlite_bind_blob(stmt, 1, payload, sizeof(payload));
```

### Binding multiple parameters

You can bind multiple parameters in sequence. Each call to a bind function targets a different parameter index:

```c
wlite_prepare(db,
    "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
    &stmt);

wlite_bind_text(stmt, 1, "Alice");
wlite_bind_text(stmt, 2, "alice@example.com");
wlite_bind_int64(stmt, 3, 30);
```

### Reusing a statement

Reset a statement to rebind new parameters and re-execute:

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT * FROM users WHERE id = ?", &stmt);

/* First query */
wlite_bind_int64(stmt, 1, 1);
while (wlite_step(stmt) == WLITE_OK) {
    printf("User 1: %s\n", wlite_column_text(stmt, 1));
}

/* Reset and rebind for a second query */
wlite_stmt_reset(stmt);
wlite_bind_int64(stmt, 1, 2);
while (wlite_step(stmt) == WLITE_OK) {
    printf("User 2: %s\n", wlite_column_text(stmt, 1));
}

wlite_stmt_finalize(stmt);
```

## Stepping through results

`wlite_step` advances to the next row in the result set. It returns `WLITE_OK` for each row and a non-zero value when there are no more rows or on error:

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT id, name FROM users", &stmt);

while (wlite_step(stmt) == WLITE_OK) {
    int64_t id = wlite_column_int64(stmt, 0);
    const char *name = wlite_column_text(stmt, 1);
    printf("%lld: %s\n", (long long)id, name);
}

wlite_stmt_finalize(stmt);
```

### Checking for errors

If `wlite_step` returns something other than `WLITE_OK`, it might mean the result set is exhausted (normal) or an error occurred. For most queries you can rely on the loop pattern above. If you need to distinguish "no more rows" from an error, check the return value after the loop:

```c
wlite_result r;
while ((r = wlite_step(stmt)) == WLITE_OK) {
    /* process row */
}

if (r != WLITE_OK) {
    /* r is either "done" or an actual error */
    fprintf(stderr, "Step error: %s\n", wlite_strerror(r));
}
```

### INSERT, UPDATE, DELETE

For statements that do not return rows, call `wlite_step` once:

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "INSERT INTO users (name) VALUES (?)", &stmt);
wlite_bind_text(stmt, 1, "Bob");

wlite_result r = wlite_step(stmt);
if (r != WLITE_OK) {
    fprintf(stderr, "Insert failed: %s\n", wlite_strerror(r));
}

wlite_stmt_finalize(stmt);
```

Alternatively, use `wlite_execute` for one-shot statements that do not need parameter binding:

```c
int64_t affected = 0;
wlite_execute(db, "DELETE FROM users WHERE id < 10", &affected);
printf("%lld rows deleted.\n", (long long)affected);
```

## Column access

After a successful `wlite_step`, read column values using the column accessors. Columns are 0-indexed.

### Column metadata

```c
int cols = wlite_column_count(stmt);
printf("Query returns %d column(s).\n", cols);

for (int i = 0; i < cols; i++) {
    const char *name = wlite_column_name(stmt, i);
    wlite_value_type type = wlite_column_type(stmt, i);
    printf("  Column %d: %s (type %d)\n", i, name, type);
}
```

### Reading integer values

```c
int64_t id = wlite_column_int64(stmt, 0);
printf("ID: %lld\n", (long long)id);
```

### Reading real values

```c
double price = wlite_column_double(stmt, 2);
printf("Price: %.2f\n", price);
```

### Reading text values

```c
const char *name = wlite_column_text(stmt, 1);
size_t len = wlite_column_bytes(stmt, 1);
printf("Name: %s (%zu bytes)\n", name, len);
```

The pointer returned by `wlite_column_text` is valid until the next call to `wlite_step` or `wlite_stmt_finalize`. Copy the string if you need it to persist.

### Reading blob values

```c
const void *data = wlite_column_blob(stmt, 0);
size_t len = wlite_column_bytes(stmt, 0);
printf("Blob is %zu bytes.\n", len);

/* Copy the data if you need it to persist */
uint8_t *copy = malloc(len);
memcpy(copy, data, len);
```

Like text, the blob pointer is valid only until the next step or finalize.

### Checking column types

```c
wlite_value_type type = wlite_column_type(stmt, 0);
switch (type) {
    case WLITE_TYPE_NULL:
        printf("NULL\n");
        break;
    case WLITE_TYPE_INTEGER:
        printf("Integer: %lld\n", (long long)wlite_column_int64(stmt, 0));
        break;
    case WLITE_TYPE_REAL:
        printf("Real: %f\n", wlite_column_double(stmt, 0));
        break;
    case WLITE_TYPE_TEXT:
        printf("Text: %s\n", wlite_column_text(stmt, 0));
        break;
    case WLITE_TYPE_BLOB:
        printf("Blob: %zu bytes\n", wlite_column_bytes(stmt, 0));
        break;
}
```

## The record API

The record API provides name-based access to result rows. A record is a snapshot of the current row and is independent of the statement, so you can continue stepping or finalize the statement while keeping the record.

### Creating a record

```c
while (wlite_step(stmt) == WLITE_OK) {
    wlite_record *rec = wlite_record_from_stmt(stmt);

    int idx = wlite_record_find(rec, "name");
    if (idx >= 0) {
        printf("Name: %s\n", wlite_record_text(rec, idx));
    }

    wlite_record_free(rec);
}
```

### Record column metadata

```c
wlite_record *rec = wlite_record_from_stmt(stmt);

int cols = wlite_record_column_count(rec);
for (int i = 0; i < cols; i++) {
    const char *name = wlite_record_column_name(rec, i);
    wlite_value_type type = wlite_record_column_type(rec, i);
    printf("  %s (type %d)\n", name, type);
}

wlite_record_free(rec);
```

### Finding a column by name

```c
int idx = wlite_record_find(rec, "email");
if (idx >= 0) {
    printf("Email: %s\n", wlite_record_text(rec, idx));
} else {
    printf("Column 'email' not found.\n");
}
```

### Reading values from a record

```c
int64_t id = wlite_record_int64(rec, 0);
double score = wlite_record_double(rec, 2);
const char *name = wlite_record_text(rec, 1);
const void *blob = wlite_record_blob(rec, 3);
size_t blob_len = wlite_record_blob_bytes(rec, 3);
```

### Records and statement lifetime

Records own a copy of the row data. You can finalize the statement immediately after creating the record:

```c
wlite_record *rec = NULL;
while (wlite_step(stmt) == WLITE_OK) {
    rec = wlite_record_from_stmt(stmt);
    break; /* We only need the first row */
}
wlite_stmt_finalize(stmt);

/* rec is still valid */
if (rec) {
    printf("Name: %s\n", wlite_record_text(rec, 1));
    wlite_record_free(rec);
}
```

## Transactions

Transactions group multiple operations into an atomic unit. Either all operations succeed, or none of them are applied.

### Basic transaction

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);
wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);

/* Check for errors and decide */
if (some_error) {
    wlite_rollback(tx);
    printf("Transaction rolled back.\n");
} else {
    wlite_commit(tx);
    printf("Transaction committed.\n");
}

wlite_tx_free(tx);
```

### Transaction with error handling

```c
wlite_tx *tx = NULL;
wlite_result r = wlite_begin(db, &tx);
if (r != WLITE_OK) {
    fprintf(stderr, "Begin transaction: %s\n", wlite_strerror(r));
    return 1;
}

r = wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);
if (r != WLITE_OK) {
    wlite_rollback(tx);
    wlite_tx_free(tx);
    fprintf(stderr, "Insert failed: %s\n", wlite_strerror(r));
    return 1;
}

r = wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);
if (r != WLITE_OK) {
    wlite_rollback(tx);
    wlite_tx_free(tx);
    fprintf(stderr, "Insert failed: %s\n", wlite_strerror(r));
    return 1;
}

r = wlite_commit(tx);
wlite_tx_free(tx);

if (r != WLITE_OK) {
    fprintf(stderr, "Commit failed: %s\n", wlite_strerror(r));
    return 1;
}
```

### Always free the transaction handle

The transaction handle must be freed even after a rollback or commit:

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

/* ... do work ... */

wlite_rollback(tx);  /* or wlite_commit(tx) */
wlite_tx_free(tx);   /* always free */
```

## Savepoints

Savepoints allow partial rollback within a transaction. You can undo changes made after a savepoint without aborting the entire transaction.

### Creating a savepoint

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

/* First insert (outside any savepoint) */
wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);

/* Create a savepoint */
wlite_savepoint(tx, "sp1");

/* Second insert (inside savepoint) */
wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);

/* Something went wrong, undo the second insert */
wlite_rollback_to(tx, "sp1");
wlite_release(tx, "sp1");

/* Alice is still inserted, Bob is not */
wlite_commit(tx);
wlite_tx_free(tx);
```

### Nested savepoints

You can nest savepoints for finer-grained rollback:

```c
wlite_tx *tx = NULL;
wlite_begin(db, &tx);

wlite_execute(db, "INSERT INTO users (name) VALUES ('Alice')", NULL);

wlite_savepoint(tx, "outer");

wlite_execute(db, "INSERT INTO users (name) VALUES ('Bob')", NULL);

wlite_savepoint(tx, "inner");

wlite_execute(db, "INSERT INTO users (name) VALUES ('Charlie')", NULL);

/* Undo Charlie only */
wlite_rollback_to(tx, "inner");
wlite_release(tx, "inner");

/* Bob is still there */
wlite_execute(db, "INSERT INTO users (name) VALUES ('Diana')", NULL);

/* Undo Bob and Diana, but keep Alice */
wlite_rollback_to(tx, "outer");
wlite_release(tx, "outer");

wlite_commit(tx);
wlite_tx_free(tx);
```

### Releasing a savepoint

You must release a savepoint after you are done with it, whether you rolled back to it or not:

```c
wlite_savepoint(tx, "sp1");
/* ... do work ... */
wlite_release(tx, "sp1");
```

If you rolled back to the savepoint, release it before creating another savepoint or before committing.

## Blob support

Binary data (blobs) are fully supported through the bind and column access APIs.

### Inserting a blob

```c
uint8_t image_data[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

wlite_stmt *stmt = NULL;
wlite_prepare(db, "INSERT INTO images (name, data) VALUES (?, ?)", &stmt);
wlite_bind_text(stmt, 1, "logo.png");
wlite_bind_blob(stmt, 2, image_data, sizeof(image_data));

wlite_step(stmt);
wlite_stmt_finalize(stmt);
```

### Reading a blob

```c
wlite_stmt *stmt = NULL;
wlite_prepare(db, "SELECT data FROM images WHERE name = ?", &stmt);
wlite_bind_text(stmt, 1, "logo.png");

while (wlite_step(stmt) == WLITE_OK) {
    const void *data = wlite_column_blob(stmt, 0);
    size_t len = wlite_column_bytes(stmt, 0);

    printf("Blob is %zu bytes.\n", len);

    /* Copy the data if needed */
    uint8_t *copy = malloc(len);
    memcpy(copy, data, len);

    /* Process the copy... */

    free(copy);
}

wlite_stmt_finalize(stmt);
```

### Blob with the record API

```c
while (wlite_step(stmt) == WLITE_OK) {
    wlite_record *rec = wlite_record_from_stmt(stmt);
    int idx = wlite_record_find(rec, "data");

    if (idx >= 0) {
        const void *data = wlite_record_blob(rec, idx);
        size_t len = wlite_record_blob_bytes(rec, idx);
        printf("Blob: %zu bytes\n", len);
    }

    wlite_record_free(rec);
}
```

### Large blobs

For large blobs, make sure you allocate enough memory and handle errors:

```c
/* Create a 1MB buffer */
size_t buf_size = 1024 * 1024;
uint8_t *buf = malloc(buf_size);
if (!buf) {
    fprintf(stderr, "Out of memory.\n");
    return 1;
}

/* Fill the buffer with data... */

wlite_prepare(db, "INSERT INTO buffers (data) VALUES (?)", &stmt);
wlite_bind_blob(stmt, 1, buf, buf_size);

wlite_step(stmt);
wlite_stmt_finalize(stmt);
free(buf);
```

## Complete query example

Here is a complete program that demonstrates a typical query workflow:

```c
#include <stdio.h>
#include <stdlib.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_db *db = NULL;
    wlite_result r;

    /* Open the database */
    r = wlite_open("app.db", &db);
    if (r != WLITE_OK) {
        fprintf(stderr, "Open: %s\n", wlite_strerror(r));
        return 1;
    }

    /* Create a table */
    r = wlite_execute(db,
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  email TEXT,"
        "  score REAL"
        ")",
        NULL);
    if (r != WLITE_OK) {
        fprintf(stderr, "Create table: %s\n", wlite_strerror(r));
        wlite_close(db);
        return 1;
    }

    /* Insert rows in a transaction */
    wlite_tx *tx = NULL;
    wlite_begin(db, &tx);

    const char *names[] = {"Alice", "Bob", "Charlie", "Diana"};
    const char *emails[] = {"a@x.com", "b@x.com", NULL, "d@x.com"};
    double scores[] = {95.5, 87.0, 92.3, 88.7};

    for (int i = 0; i < 4; i++) {
        wlite_stmt *stmt = NULL;
        wlite_prepare(db,
            "INSERT INTO users (name, email, score) VALUES (?, ?, ?)",
            &stmt);
        wlite_bind_text(stmt, 1, names[i]);
        if (emails[i]) {
            wlite_bind_text(stmt, 2, emails[i]);
        } else {
            wlite_bind_null(stmt, 2);
        }
        wlite_bind_double(stmt, 3, scores[i]);
        wlite_step(stmt);
        wlite_stmt_finalize(stmt);
    }

    wlite_commit(tx);
    wlite_tx_free(tx);

    /* Query with a parameter */
    wlite_stmt *stmt = NULL;
    wlite_prepare(db,
        "SELECT id, name, email, score FROM users WHERE score > ? ORDER BY score DESC",
        &stmt);
    wlite_bind_double(stmt, 1, 90.0);

    printf("Users with score > 90:\n");
    printf("  %-5s %-10s %-15s %s\n", "ID", "Name", "Email", "Score");
    printf("  %-5s %-10s %-15s %s\n", "---", "----", "-----", "-----");

    while (wlite_step(stmt) == WLITE_OK) {
        int64_t id = wlite_column_int64(stmt, 0);
        const char *name = wlite_column_text(stmt, 1);
        const char *email = wlite_column_text(stmt, 2);
        double score = wlite_column_double(stmt, 3);

        wlite_value_type email_type = wlite_column_type(stmt, 2);

        printf("  %-5lld %-10s %-15s %.1f\n",
               (long long)id,
               name,
               email_type == WLITE_TYPE_NULL ? "(none)" : email,
               score);
    }

    wlite_stmt_finalize(stmt);

    /* Count rows */
    wlite_prepare(db, "SELECT COUNT(*) FROM users", &stmt);
    if (wlite_step(stmt) == WLITE_OK) {
        int64_t count = wlite_column_int64(stmt, 0);
        printf("\nTotal users: %lld\n", (long long)count);
    }
    wlite_stmt_finalize(stmt);

    /* Close the database */
    wlite_close(db);

    return 0;
}
```

## Best practices

Always finalize statements when done with them. A leaked statement handle holds memory and a lock on the SQLite statement cache.

Use `wlite_bind_text_n` instead of `wlite_bind_text` when you have a string with a known length and want to avoid calling `strlen`.

Copy data from column accessors if you need it to persist beyond the current row or statement lifetime. The pointers are invalidated on the next step or finalize.

Use transactions when inserting or updating multiple rows. This avoids repeated disk syncs and ensures atomicity.

Use `wlite_execute` for one-shot DDL and DML statements that do not need parameter binding. It is simpler and avoids the overhead of prepare/step/finalize.

Check return values from every fallible function. Ignoring errors leads to silent data corruption or resource leaks.

## Next steps

- [Errors](errors.md): Error codes, structured errors, cleanup patterns, and thread safety
- [Migration](migration.md): Load models, diff schemas, and run migrations
- [C API Reference](../../c-api.md): Complete function-by-function reference
