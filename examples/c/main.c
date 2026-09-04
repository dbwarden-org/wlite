/*
 * Example: C application using libwlite
 *
 * Build:
 *   gcc -I../../include -o todo main.c -L../../. -lwlite -lsqlite3
 *   LD_LIBRARY_PATH=../../. ./todo
 */

#include <stdio.h>
#include <string.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_result rc;

    /* Load model */
    rc = wlite_model_load_file("app.wlite", &model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Failed to load model: %s\n", wlite_strerror(rc));
        return 1;
    }

    /* Validate model */
    rc = wlite_model_validate(model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Model validation failed: %s\n", wlite_strerror(rc));
        wlite_model_free(model);
        return 1;
    }

    printf("Model: %s v%d\n", wl_schema_model_name(model->schema),
           wl_schema_model_version(model->schema));
    printf("Tables: %zu\n", wlite_model_table_count(model));

    /* Open database */
    rc = wlite_open("todo.db", &db);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Failed to open database: %s\n", wlite_strerror(rc));
        wlite_model_free(model);
        return 1;
    }

    /* Migrate */
    rc = wlite_migrate(db, model);
    if (rc != WLITE_OK) {
        fprintf(stderr, "Migration failed: %s\n", wlite_strerror(rc));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    printf("Migration complete.\n");

    /* Insert a todo */
    wlite_stmt *stmt = NULL;
    rc = wlite_prepare(db, "INSERT INTO todos (title, completed, created_at) VALUES (?, 0, strftime('%s','now'))", &stmt);
    if (rc == WLITE_OK) {
        wlite_bind_text(stmt, 1, "Buy groceries");
        wlite_step(stmt);
        wlite_stmt_finalize(stmt);
    }

    /* Query todos */
    rc = wlite_prepare(db, "SELECT id, title, completed FROM todos", &stmt);
    if (rc == WLITE_OK) {
        printf("\nTodos:\n");
        while (wlite_step(stmt) == WLITE_OK) {
            printf("  [%s] %s (id=%lld)\n",
                wlite_column_int64(stmt, 2) ? "x" : " ",
                wlite_column_text(stmt, 1),
                (long long)wlite_column_int64(stmt, 0));
        }
        wlite_stmt_finalize(stmt);
    }

    /* Transaction example */
    wlite_tx *tx = NULL;
    wlite_begin(db, &tx);
    wlite_prepare(db, "INSERT INTO todos (title, completed, created_at) VALUES (?, 0, strftime('%s','now'))", &stmt);
    wlite_bind_text(stmt, 1, "Read documentation");
    wlite_step(stmt);
    wlite_stmt_finalize(stmt);
    wlite_commit(tx);

    /* Savepoint example */
    wlite_begin(db, &tx);
    wlite_savepoint(tx, "before_batch");
    /* ... batch operations ... */
    wlite_release(tx, "before_batch");
    wlite_commit(tx);

    /* Cleanup */
    wlite_close(db);
    wlite_model_free(model);

    printf("\nDone!\n");
    return 0;
}
