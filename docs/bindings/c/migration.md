---
title: "Migration with the C Binding"
description: "Load models, diff schemas, run migrations, inspect plans, work with compiled .wlitem binaries, and verify schema integrity using the wlite C API."
---

# Migration with the C Binding

This guide covers the full migration workflow using the wlite C binding. You will learn how to load model files, compare schemas, generate migration plans, apply changes to a database, and work with compiled model binaries.

## Loading a model

A model is the declarative definition of your desired database schema. libwlite can load models from a file, from a memory buffer, or from a precompiled `.wlitem` binary.

### Loading from a file

The most common case. Pass the path to a `.wlite` file:

```c
#include <stdio.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_result r = wlite_model_load_file("app.wlite", &model);

    if (r != WLITE_OK) {
        fprintf(stderr, "Failed to load model: %s\n", wlite_strerror(r));
        return 1;
    }

    printf("Model loaded successfully.\n");

    /* Inspect the model */
    size_t tables = wlite_model_table_count(model);
    printf("Model defines %zu table(s).\n", tables);

    wlite_model_free(model);
    return 0;
}
```

The file is read entirely into memory and parsed. If the file does not exist, `WLITE_NOT_FOUND` is returned. If the syntax is invalid, `WLITE_PARSE_ERROR` is returned. If the model has semantic errors (duplicate table names, invalid types), `WLITE_MODEL_ERROR` is returned.

### Loading from memory

Useful when the model content is embedded in your binary, read from a network resource, or generated at runtime:

```c
const char *dsl =
    "model_config { name \"embedded\" version 1 }\n"
    "model Item {\n"
    "    table \"items\"\n"
    "    field id integer { primary_key autoincrement }\n"
    "    field name text { not_null }\n"
    "}";

wlite_model *model = NULL;
wlite_result r = wlite_model_load_memory(dsl, strlen(dsl), &model);
if (r != WLITE_OK) {
    fprintf(stderr, "Load from memory failed: %s\n", wlite_strerror(r));
    return 1;
}

/* Model is ready to use */
wlite_model_free(model);
```

### Validating a model

After loading, you should validate the model to catch semantic errors early:

```c
wlite_model *model = NULL;
wlite_model_load_file("app.wlite", &model);

wlite_result r = wlite_model_validate(model);
if (r != WLITE_OK) {
    fprintf(stderr, "Model validation failed: %s\n", wlite_strerror(r));
    wlite_model_free(model);
    return 1;
}

printf("Model is valid.\n");
wlite_model_free(model);
```

Validation checks that all table names are unique, column types are recognized, primary key columns exist, and foreign key references point to valid tables and columns.

## Opening a database

Before migrating, you need a database handle. `wlite_open` creates or opens an existing SQLite database with default settings:

```c
wlite_db *db = NULL;
wlite_result r = wlite_open("app.db", &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", wlite_strerror(r));
    return 1;
}
```

For more control, use `wlite_open_ex` with explicit options:

```c
wlite_open_options opts = {
    .readonly = 0,
    .create = 1,
    .foreign_keys = 1,
    .busy_timeout_ms = 5000,
};

wlite_db *db = NULL;
wlite_result r = wlite_open_ex("app.db", &opts, &db);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", wlite_strerror(r));
    return 1;
}
```

The options:

| Field | Description |
|-------|-------------|
| `readonly` | Open the database in read-only mode. Set to 1 for queries only. |
| `create` | Create the database file if it does not exist. Set to 0 to require an existing file. |
| `foreign_keys` | Enable SQLite foreign key enforcement. Set to 1 to enforce foreign keys. |
| `busy_timeout_ms` | Milliseconds to wait when the database is locked by another connection before returning `WLITE_BUSY`. |

Always close the database when done:

```c
wlite_close(db);
```

## Running a migration

The simplest way to migrate is `wlite_migrate`. It compares the live database against the model and applies all necessary changes in a single call:

```c
wlite_model *model = NULL;
wlite_db *db = NULL;

wlite_model_load_file("app.wlite", &model);
wlite_open("app.db", &db);

wlite_result r = wlite_migrate(db, model);
if (r != WLITE_OK) {
    fprintf(stderr, "Migration failed: %s\n", wlite_strerror(r));
}

wlite_close(db);
wlite_model_free(model);
```

This is safe to call repeatedly. If the database already matches the model, no changes are made. If there are differences, they are applied automatically.

### What happens during a migration

Under the hood, `wlite_migrate` does the following:

1. Introspects the live database into a `WlSchema`
2. Loads the model into a `WlSchema`
3. Computes a diff between the two
4. Generates a migration plan
5. Executes each step of the plan
6. Records rollback SQL for each step

If any step fails, the migration stops and returns an error. Rollback SQL is stored so you can undo changes if needed.

## Diffing schemas

If you want to inspect changes before applying them, use the diff API. This lets you see exactly what would happen without executing anything.

### Single-call diff

`wlite_diff` compares a live database against a model and returns a migration plan:

```c
wlite_db *db = NULL;
wlite_model *model = NULL;

wlite_open("app.db", &db);
wlite_model_load_file("app.wlite", &model);

WlPlan *plan = NULL;
wlite_result r = wlite_diff(db, model, &plan);
if (r == WLITE_OK && plan) {
    size_t count = wlite_plan_count(plan);
    printf("Migration requires %zu step(s).\n", count);

    for (size_t i = 0; i < count; i++) {
        printf("  Step %zu: %s\n", i + 1, plan->steps[i].detail);
        if (plan->steps[i].sql) {
            printf("    SQL: %s\n", plan->steps[i].sql);
        }
    }

    wl_plan_free(plan);
} else if (r == WLITE_OK) {
    printf("Database is up to date.\n");
} else {
    fprintf(stderr, "Diff failed: %s\n", wlite_strerror(r));
}

wlite_model_free(model);
wlite_close(db);
```

### Manual diff with schema introspection

For finer control, you can introspect the database and load the model separately, then diff them:

```c
/* Introspect the live database */
WlSchema *current = wl_schema_inspect(db, NULL);
if (!current) {
    fprintf(stderr, "Failed to introspect database.\n");
    return 1;
}

/* Load the desired schema from the model file */
WlSchema *desired = wl_schema_load("app.wlite", NULL);
if (!desired) {
    fprintf(stderr, "Failed to load desired schema.\n");
    wl_schema_free(current);
    return 1;
}

/* Compute the diff */
WlDiff *diff = wl_schema_diff(current, desired, NULL);
if (diff) {
    printf("%zu difference(s) found.\n", diff->entry_count);

    for (size_t i = 0; i < diff->entry_count; i++) {
        const WlDiffEntry *entry = &diff->entries[i];
        printf("  Operation: %d\n", entry->op);
        printf("  Safety:    %d\n", entry->safety);
        printf("  Table:     %s\n", entry->table);
        printf("  Detail:    %s\n", entry->detail);
        printf("\n");
    }

    wl_diff_free(diff);
} else {
    printf("Schemas are identical.\n");
}

wl_schema_free(desired);
wl_schema_free(current);
```

## Migration plans

A migration plan is an ordered list of SQL statements, each with a corresponding rollback statement. You can inspect, modify, or apply plans programmatically.

### Generating a plan

```c
WlSchema *current = wl_schema_inspect(db, NULL);
WlSchema *desired = wl_schema_load("app.wlite", NULL);

WlPlan *plan = wl_plan_migration(current, desired, NULL);
if (plan) {
    printf("Plan has %zu step(s).\n", plan->step_count);
    printf("Schema hash before: %s\n", plan->schema_hash_before);
    printf("Schema hash after:  %s\n", plan->schema_hash_after);

    for (size_t i = 0; i < plan->step_count; i++) {
        const WlPlanStep *step = &plan->steps[i];
        printf("\nStep %zu:\n", i + 1);
        printf("  Operation:  %d\n", step->op);
        printf("  Safety:     %d\n", step->safety);
        printf("  Table:      %s\n", step->table);
        printf("  Detail:     %s\n", step->detail);
        printf("  SQL:        %s\n", step->sql);
        if (step->rollback_sql) {
            printf("  Rollback:   %s\n", step->rollback_sql);
        }
        if (step->is_non_atomic) {
            printf("  Warning:    non-atomic step\n");
        }
    }

    wl_plan_free(plan);
}

wl_schema_free(desired);
wl_schema_free(current);
```

### Applying a plan

Once you have a plan, apply it with `wl_apply_plan`:

```c
wlite_result r = wl_apply_plan(db, plan, NULL);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to apply plan: %s\n", wlite_strerror(r));
}
```

### Rolling back the last migration

If you need to undo the most recent migration, use `wl_rollback_last`. It uses the rollback SQL stored during the previous `wlite_migrate` or `wl_apply_plan` call:

```c
wlite_result r = wl_rollback_last(db, NULL);
if (r != WLITE_OK) {
    fprintf(stderr, "Rollback failed: %s\n", wlite_strerror(r));
}
```

## Checking schema integrity

`wl_schema_verify` compares a live database against an expected schema and returns any differences:

```c
WlSchema *expected = wl_schema_load("app.wlite", NULL);

WlDiff *diff = NULL;
wlite_result r = wl_schema_verify(db, expected, &diff, NULL);

if (r == WLITE_OK && diff && diff->entry_count > 0) {
    printf("Schema mismatch: %zu difference(s).\n", diff->entry_count);
    for (size_t i = 0; i < diff->entry_count; i++) {
        printf("  %s on table %s: %s\n",
               diff->entries[i].table,
               diff->entries[i].object,
               diff->entries[i].detail);
    }
    wl_diff_free(diff);
} else if (r == WLITE_OK) {
    printf("Schema matches the expected model.\n");
} else {
    fprintf(stderr, "Verification failed: %s\n", wlite_strerror(r));
}

wl_schema_free(expected);
```

This is useful in CI pipelines or at application startup to confirm the database has been migrated correctly.

## Schema hashing

Compute a fingerprint of a schema for integrity checks and change detection:

```c
WlSchema *schema = wl_schema_load("app.wlite", NULL);
char *hash = wl_schema_hash(schema);
printf("Schema hash: %s\n", hash);
wlite_free(hash);
wl_schema_free(schema);
```

The returned string is heap-allocated and must be freed with `wlite_free`. Two identical schemas produce the same hash. You can store the hash in a configuration file or environment variable and compare it at runtime to detect schema drift.

### Comparing hashes

```c
WlSchema *live = wl_schema_inspect(db, NULL);
char *live_hash = wl_schema_hash(live);

const char *expected_hash = "abc123..."; /* from your config */

if (strcmp(live_hash, expected_hash) != 0) {
    fprintf(stderr, "Schema drift detected. Expected %s, got %s\n",
            expected_hash, live_hash);
    wlite_free(live_hash);
    wl_schema_free(live);
    return 1;
}

printf("Schema hash matches.\n");
wlite_free(live_hash);
wl_schema_free(live);
```

## Compiled models (.wlitem)

For faster loading, you can compile a `.wlite` model into a binary `.wlitem` file. The compiled format skips parsing and is loaded directly into memory.

### Compiling a model

```c
/* Load and parse the source model */
WlSchema *schema = wl_schema_load("app.wlite", NULL);
if (!schema) {
    fprintf(stderr, "Failed to load schema.\n");
    return 1;
}

/* Compile to binary */
int r = wl_model_compile(schema, "app.wlitem");
if (r != 0) {
    fprintf(stderr, "Failed to compile model.\n");
    wl_schema_free(schema);
    return 1;
}

printf("Compiled model written to app.wlitem\n");
wl_schema_free(schema);
```

### Loading a compiled model

Use `wlite_model_load_compiled` to load the binary directly:

```c
/* Read the compiled file into memory */
FILE *fp = fopen("app.wlitem", "rb");
if (!fp) {
    fprintf(stderr, "Cannot open compiled model.\n");
    return 1;
}

fseek(fp, 0, SEEK_END);
size_t size = (size_t)ftell(fp);
fseek(fp, 0, SEEK_SET);

void *data = malloc(size);
fread(data, 1, size, fp);
fclose(fp);

/* Load the compiled model */
wlite_model *model = NULL;
wlite_result r = wlite_model_load_compiled(data, size, &model);
if (r != WLITE_OK) {
    fprintf(stderr, "Failed to load compiled model: %s\n", wlite_strerror(r));
    free(data);
    return 1;
}

/* Use the model for migration */
wlite_db *db = NULL;
wlite_open("app.db", &db);
wlite_migrate(db, model);

wlite_close(db);
wlite_model_free(model);
free(data);
```

### Loading the raw schema from compiled data

If you need a `WlSchema` directly from compiled data (bypassing the `wlite_model` wrapper):

```c
WlSchema *schema = wl_model_load_compiled_raw(data, size);
if (schema) {
    printf("Loaded %zu tables from compiled model.\n", schema->table_count);
    wl_schema_free(schema);
}
```

### When to use compiled models

- Embedded applications where you want to avoid shipping source text
- Applications that load the same model repeatedly and want faster startup
- Scenarios where the model file should not be exposed as plaintext

## Model introspection

You can navigate the structure of a loaded model to inspect tables and fields programmatically:

```c
wlite_model *model = NULL;
wlite_model_load_file("app.wlite", &model);

size_t table_count = wlite_model_table_count(model);
printf("Model has %zu table(s).\n", table_count);

for (size_t i = 0; i < table_count; i++) {
    const wlite_table *table = wlite_model_table_at(model, i);
    printf("\nTable: %s\n", wlite_table_name(table));

    size_t field_count = wlite_table_field_count(table);
    for (size_t j = 0; j < field_count; j++) {
        const wlite_field *field = wlite_table_field_at(table, j);
        printf("  %s (type %d)", wlite_field_name(field), wlite_field_type(field));

        if (wlite_field_is_primary_key(field)) printf(" PK");
        if (wlite_field_is_unique(field)) printf(" UNIQUE");
        if (wlite_field_is_autoincrement(field)) printf(" AUTOINCREMENT");
        if (!wlite_field_is_nullable(field)) printf(" NOT NULL");
        printf("\n");
    }
}

wlite_model_free(model);
```

You can also look up a specific table or field by name:

```c
const wlite_table *users = wlite_model_table(model, "users");
if (users) {
    const wlite_field *email = wlite_table_field(users, "email");
    if (email) {
        printf("email is type %d\n", wlite_field_type(email));
    }
}
```

## Complete migration workflow

Here is a complete program that loads a model, checks the current state, shows the diff, and applies the migration:

```c
#include <stdio.h>
#include <wlite/wlite.h>

int main(void) {
    wlite_model *model = NULL;
    wlite_db *db = NULL;
    wlite_result r;

    /* Load and validate the model */
    r = wlite_model_load_file("app.wlite", &model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Load model: %s\n", wlite_strerror(r));
        return 1;
    }

    r = wlite_model_validate(model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Validate model: %s\n", wlite_strerror(r));
        wlite_model_free(model);
        return 1;
    }

    /* Open the database */
    wlite_open_options opts = {
        .readonly = 0,
        .create = 1,
        .foreign_keys = 1,
        .busy_timeout_ms = 5000,
    };

    r = wlite_open_ex("app.db", &opts, &db);
    if (r != WLITE_OK) {
        fprintf(stderr, "Open database: %s\n", wlite_strerror(r));
        wlite_model_free(model);
        return 1;
    }

    /* Check what the migration would do */
    WlPlan *plan = NULL;
    r = wlite_diff(db, model, &plan);
    if (r != WLITE_OK) {
        fprintf(stderr, "Diff: %s\n", wlite_strerror(r));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    if (plan && plan->step_count > 0) {
        printf("Migration plan (%zu steps):\n", plan->step_count);
        for (size_t i = 0; i < plan->step_count; i++) {
            printf("  %zu. %s\n", i + 1, plan->steps[i].detail);
        }
        wl_plan_free(plan);
    } else {
        printf("Database is already up to date.\n");
        if (plan) wl_plan_free(plan);
        wlite_close(db);
        wlite_model_free(model);
        return 0;
    }

    /* Apply the migration */
    r = wlite_migrate(db, model);
    if (r != WLITE_OK) {
        fprintf(stderr, "Migrate: %s\n", wlite_strerror(r));
        wlite_close(db);
        wlite_model_free(model);
        return 1;
    }

    printf("Migration completed successfully.\n");

    /* Verify the schema matches */
    WlSchema *expected = wl_schema_load("app.wlite", NULL);
    WlDiff *diff = NULL;
    r = wl_schema_verify(db, expected, &diff, NULL);
    if (r == WLITE_OK && diff && diff->entry_count == 0) {
        printf("Schema verified: database matches the model.\n");
    } else if (diff) {
        printf("Warning: %zu difference(s) after migration.\n", diff->entry_count);
        wl_diff_free(diff);
    }
    wl_schema_free(expected);

    wlite_close(db);
    wlite_model_free(model);
    return 0;
}
```

## Next steps

- [Queries](queries.md): Prepare statements, bind parameters, step through results, and manage transactions
- [Errors](errors.md): Error codes, structured errors, and cleanup patterns
- [C API Reference](../../c-api.md): Complete function-by-function reference
