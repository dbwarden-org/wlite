---
title: Migration Guide
description: Schema management and migration with the wlite Go binding, including LoadModel, Migrate, Diff, Plan, Check, Snapshot, Hash, and compiled models.
---

# Migration Guide

The migration system in wlite lets you define your schema in a `.wlite` file, load it into Go as a `Model`, and apply it to any SQLite database. The Go binding exposes the full migration pipeline: loading, validation, diffing, planning, applying, checking, and snapshotting.

## Loading models

### LoadModel

Load a `.wlite` schema file from disk:

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()
```

`LoadModel` parses the schema file and returns an immutable `Model`. The model is safe to share across goroutines and can be used to migrate multiple databases. Always call `model.Close()` when you are done with the model.

### ModelFromBytes

Load a model from raw bytes. This is useful when the schema is embedded in the binary, fetched from a remote server, or stored in a database:

```go
schemaData, err := os.ReadFile("app.wlite")
if err != nil {
    log.Fatal(err)
}

model, err := wlite.ModelFromBytes(schemaData)
if err != nil {
    log.Fatal(err)
}
defer model.Close()
```

### ModelFromBytes with embedded data

You can embed the schema file directly in your Go binary using `embed`:

```go
//go:embed app.wlite
var schemaData []byte

func loadEmbeddedModel() (*wlite.Model, error) {
    model, err := wlite.ModelFromBytes(schemaData)
    if err != nil {
        return nil, fmt.Errorf("loading embedded model: %w", err)
    }
    return model, nil
}
```

This approach eliminates the need to ship the `.wlite` file alongside the binary.

## Opening a database

Before migrating you need an open database connection:

```go
db, err := wlite.Open("app.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

For testing, use an in-memory database:

```go
db, err := wlite.OpenMemory()
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

## Migrate

The `Migrate` method compares the current database schema against the loaded model and applies the necessary changes:

```go
if err := db.Migrate(model); err != nil {
    log.Fatal(err)
}
fmt.Println("Migration complete.")
```

`Migrate` is idempotent. Running it multiple times with the same model produces no changes after the first migration. This makes it safe to call at application startup.

### Full migration workflow

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    model, err := wlite.LoadModel("app.wlite")
    if err != nil {
        log.Fatal(err)
    }
    defer model.Close()

    if err := model.Validate(); err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Model has %d table(s)\n", model.TableCount())

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }
    fmt.Println("Database is up to date.")
}
```

### Migrating multiple databases

Since the model is immutable and reusable, you can migrate many databases with the same loaded model:

```go
func migrateAll(model *wlite.Model, paths []string) error {
    for _, path := range paths {
        db, err := wlite.Open(path)
        if err != nil {
            return fmt.Errorf("opening %s: %w", path, err)
        }

        if err := db.Migrate(model); err != nil {
            db.Close()
            return fmt.Errorf("migrating %s: %w", path, err)
        }

        db.Close()
        fmt.Printf("Migrated: %s\n", path)
    }
    return nil
}
```

## Diff

The `Diff` method computes a migration plan without applying it. This lets you inspect what changes would be made before executing them:

```go
err := db.Diff(model)
if err != nil {
    log.Fatal(err)
}
```

If the database already matches the model, `Diff` returns nil with no error. If there are differences, it computes a plan and then frees it. Use `Diff` when you want a dry-run of the migration.

### Diff with output

```go
func previewMigration(db *wlite.DB, model *wlite.Model) error {
    err := db.Diff(model)
    if err != nil {
        return fmt.Errorf("computing diff: %w", err)
    }
    fmt.Println("Diff computed. No errors means the plan is valid.")
    return nil
}
```

## Plan

The `Plan` method produces a detailed migration plan that includes individual steps, their safety levels, and rollback SQL. Use it when you need fine-grained control over which changes are applied:

```go
plan, err := db.Plan(model)
if err != nil {
    log.Fatal(err)
}
```

Each step in the plan has an operation type, a safety classification, the SQL to apply, and optional rollback SQL.

### Safety levels

| Level | Meaning |
|-------|---------|
| Safe | The operation can be applied without risk of data loss. |
| Requires Rebuild | The operation requires recreating the table. |
| Destructive | The operation will remove data. |
| Conditional | The operation depends on runtime conditions. |
| Irreversible | The operation cannot be rolled back. |

### Inspecting a plan before applying

```go
func inspectPlan(db *wlite.DB, model *wlite.Model) error {
    plan, err := db.Plan(model)
    if err != nil {
        return fmt.Errorf("creating plan: %w", err)
    }
    defer plan.Free()

    fmt.Printf("Plan has %d step(s)\n", plan.StepCount())
    return nil
}
```

## Check

The `Check` method verifies that the current database schema matches the expected model. It returns an error if there are discrepancies:

```go
err := db.Check(model)
if err != nil {
    log.Printf("Schema mismatch: %v", err)
}
```

Use `Check` in health checks or startup routines to confirm that the running database matches the application's expected schema.

### Check in a health endpoint

```go
func healthHandler(db *wlite.DB, model *wlite.Model) http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        if err := db.Check(model); err != nil {
            http.Error(w, fmt.Sprintf("schema mismatch: %v", err), http.StatusServiceUnavailable)
            return
        }
        w.WriteHeader(http.StatusOK)
        fmt.Fprintln(w, "ok")
    }
}
```

## Snapshot

The `Snapshot` method captures the current database schema as a `Model`. This lets you save the state of a database after migration:

```go
snapshot, err := db.Snapshot()
if err != nil {
    log.Fatal(err)
}
defer snapshot.Close()
```

Use snapshots to record the schema at a point in time, compare it against another model, or persist it for later use.

### Saving a snapshot to a file

```go
func saveSchemaSnapshot(db *wlite.DB, path string) error {
    snapshot, err := db.Snapshot()
    if err != nil {
        return fmt.Errorf("snapshotting: %w", err)
    }
    defer snapshot.Close()

    fmt.Printf("Snapshot captured: %d table(s)\n", snapshot.TableCount())
    return nil
}
```

## Hash

The `Hash` method computes a deterministic hash of the database schema. Use it to quickly check whether two schemas are identical without comparing every detail:

```go
hash, err := db.Hash()
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Schema hash: %s\n", hash)
```

### Comparing schemas with hashes

```go
func schemasMatch(db *wlite.DB, model *wlite.Model) (bool, error) {
    currentHash, err := db.Hash()
    if err != nil {
        return false, err
    }

    if err := db.Migrate(model); err != nil {
        return false, err
    }

    afterHash, err := db.Hash()
    if err != nil {
        return false, err
    }

    return currentHash == afterHash, nil
}
```

If the hashes match, the schema was already up to date. If they differ, the migration made changes.

## Compiled models

Compiled models are a binary representation of the schema that can be loaded faster than parsing a text `.wlite` file. This is useful for applications that need fast startup times:

### Loading a compiled model

```go
compiledData, err := os.ReadFile("app.wlite.bin")
if err != nil {
    log.Fatal(err)
}

model, err := wlite.LoadCompiled(compiledData)
if err != nil {
    log.Fatal(err)
}
defer model.Close()
```

### Compiling a model

You can compile a model from a schema using the wlite CLI:

```bash
wlite compile app.wlite app.wlite.bin
```

Then load the compiled binary in your Go application. Compiled models load faster because they skip the parsing step.

### Embedding a compiled model

```go
//go:embed app.wlite.bin
var compiledSchema []byte

func loadCompiledModel() (*wlite.Model, error) {
    model, err := wlite.LoadCompiled(compiledSchema)
    if err != nil {
        return nil, fmt.Errorf("loading compiled model: %w", err)
    }
    return model, nil
}
```

## Validation

Always validate a model after loading it to catch schema errors early:

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()

if err := model.Validate(); err != nil {
    log.Fatalf("Invalid schema: %v", err)
}
```

Validation checks for structural problems like missing table names, duplicate columns, invalid types, and malformed constraints.

## Complete migration workflow

Here is a complete program that demonstrates the full migration lifecycle:

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    model, err := wlite.LoadModel("app.wlite")
    if err != nil {
        log.Fatal(err)
    }
    defer model.Close()

    if err := model.Validate(); err != nil {
        log.Fatalf("Schema error: %v", err)
    }
    fmt.Printf("Schema: %d table(s)\n", model.TableCount())

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    beforeHash, _ := db.Hash()
    fmt.Printf("Schema hash before: %s\n", beforeHash)

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }

    afterHash, _ := db.Hash()
    fmt.Printf("Schema hash after:  %s\n", afterHash)

    if beforeHash == afterHash {
        fmt.Println("Database was already up to date.")
    } else {
        fmt.Println("Database schema was updated.")
    }

    fmt.Println("Migration workflow complete.")
}
```

## Error handling for migrations

Migration errors fall into specific categories. Handle them to provide useful feedback:

```go
func safeMigrate(db *wlite.DB, model *wlite.Model) error {
    if err := model.Validate(); err != nil {
        return fmt.Errorf("invalid model: %w", err)
    }

    if err := db.Migrate(model); err != nil {
        var wliteErr *wlite.Error
        if errors.As(err, &wliteErr) {
            switch wliteErr.Code {
            case wlite.SQLITE_ERROR:
                return fmt.Errorf("database error during migration: %s", wliteErr.Message)
            case wlite.CONSTRAINT_ERROR:
                return fmt.Errorf("constraint violation during migration: %s", wliteErr.Message)
            case wlite.BUSY:
                return fmt.Errorf("database locked during migration")
            case wlite.IO_ERROR:
                return fmt.Errorf("I/O error during migration: %s", wliteErr.Message)
            default:
                return fmt.Errorf("migration failed: %w", err)
            }
        }
        return fmt.Errorf("migration failed: %w", err)
    }

    return nil
}
```

## Best practices

Call `model.Close()` with `defer` immediately after loading. Call `db.Close()` with `defer` immediately after opening. Validate models before migrating. Use `Check` in health endpoints. Use `Hash` to detect schema drift. Use compiled models for fast startup. Use snapshots to record schema state before and after migrations.

## Further reading

- [Index](index.md) for types, installation, and CGO configuration.
- [Query guide](queries.md) for prepared statements, binding, and transactions.
- [Error handling guide](errors.md) for the `Error` type and error constants.
