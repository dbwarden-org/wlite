---
title: Migration Guide
description: Model loading, schema migration, diffing, planning, and snapshot operations in the wlite Rust binding.
---

# Migration Guide

This guide covers loading models, opening databases, running migrations, inspecting diffs, building plans, computing snapshots, and working with compiled models. All examples use `wlite::Result` for error propagation.

## Loading a model

Use `Model::load` to parse a `.wlite` schema file into an in-memory representation. The model is immutable after loading and can be shared across threads.

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    println!("Model loaded successfully");
    Ok(())
}
```

If the file does not exist or contains invalid syntax, `Model::load` returns an error:

```rust
use wlite::{Error, Model};

fn main() {
    match Model::load("missing.wlite") {
        Ok(model) => {
            println!("Model loaded");
        }
        Err(Error::Io(msg)) => {
            eprintln!("Could not read file: {msg}");
        }
        Err(Error::Parse(msg)) => {
            eprintln!("Schema syntax error: {msg}");
        }
        Err(e) => {
            eprintln!("Unexpected error: {e}");
        }
    }
}
```

### Loading from a string

If the schema is stored in a string rather than a file, load it from memory. This is useful for embedded schemas or testing.

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let source = r#"
        model app {
            users {
                id: integer pk autoincrement
                name: text not null
                email: text not null unique
            }
        }
    "#;

    let model = Model::load_memory(source.as_bytes())?;
    println!("Model loaded from memory");
    Ok(())
}
```

### Loading a compiled model

For faster startup, compile the model to a binary `.wlitem` file and load that instead. Compiled models skip parsing entirely.

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Model::load_compiled("schema.wlitem")?;
    println!("Compiled model loaded");
    Ok(())
}
```

## Opening a database

Use `Database::open` to open or create a SQLite database. If the file does not exist, it is created.

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;
    println!("Database opened");
    Ok(())
}
```

### Read-only mode

Open a database in read-only mode to prevent accidental modifications:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open_readonly("app.db")?;
    println!("Database opened in read-only mode");
    Ok(())
}
```

### Opening with options

Use `Database::open_ex` to control opening behavior:

```rust
use wlite::{Database, OpenOptions};

fn main() -> wlite::Result<()> {
    let opts = OpenOptions {
        readonly: false,
        create: true,
        foreign_keys: true,
        busy_timeout_ms: Some(5000),
    };

    let db = Database::open_ex("app.db", &opts)?;
    println!("Database opened with custom options");
    Ok(())
}
```

## Running migrations

The `migrate` method compares the live database schema against the model and applies all necessary changes. This is the primary way to keep a database schema in sync with the application.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    println!("Migration complete");
    Ok(())
}
```

If the database is already up to date, `migrate` does nothing. If changes are needed, it applies them in a single transaction.

### Migration with error handling

Handle specific migration errors:

```rust
use wlite::{Database, Error, Model};

fn migrate_database(model_path: &str, db_path: &str) -> wlite::Result<()> {
    let model = Model::load(model_path)?;
    let db = Database::open(db_path)?;

    match db.migrate(&model) {
        Ok(()) => {
            println!("Migration successful");
            Ok(())
        }
        Err(Error::Constraint(msg)) => {
            eprintln!("Migration failed due to constraint: {msg}");
            Err(Error::Constraint(msg))
        }
        Err(Error::Busy) => {
            eprintln!("Database is locked by another process");
            Err(Error::Busy)
        }
        Err(e) => {
            eprintln!("Migration failed: {e}");
            Err(e)
        }
    }
}
```

## Schema diffing

The `diff` method compares the live database against a model and returns a list of differences. This is useful for previewing changes before applying them.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    let diffs = db.diff(&model)?;

    println!("{} differences found", diffs.len());

    for diff in &diffs {
        println!("  {}", diff.detail);
    }

    Ok(())
}
```

### Filtering diffs by safety level

```rust
use wlite::{Database, Model, Safety};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    let diffs = db.diff(&model)?;

    let safe_diffs: Vec<_> = diffs.iter()
        .filter(|d| d.safety == Safety::Safe)
        .collect();

    let destructive_diffs: Vec<_> = diffs.iter()
        .filter(|d| d.safety == Safety::Destructive)
        .collect();

    println!("{} safe changes", safe_diffs.len());
    println!("{} destructive changes", destructive_diffs.len());

    Ok(())
}
```

## Migration planning

The `plan` method generates a concrete migration plan with SQL statements. Each step includes the SQL to apply the change and optional rollback SQL.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    let plan = db.plan(&model)?;

    println!("Migration plan has {} steps", plan.steps.len());

    for (i, step) in plan.steps.iter().enumerate() {
        println!("Step {}: {}", i + 1, step.sql);
        if let Some(ref rollback) = step.rollback_sql {
            println!("  Rollback: {rollback}");
        }
    }

    Ok(())
}
```

### Inspecting schema hashes

The plan includes schema hashes before and after the migration. These can be used for integrity checks:

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    let plan = db.plan(&model)?;

    println!("Hash before: {}", plan.schema_hash_before);
    println!("Hash after: {}", plan.schema_hash_after);

    Ok(())
}
```

### Applying a plan manually

You can inspect a plan before applying it:

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    let plan = db.plan(&model)?;

    if plan.steps.is_empty() {
        println!("No changes needed");
        return Ok(());
    }

    println!("Applying {} migration steps", plan.steps.len());
    db.apply_plan(&plan)?;

    println!("Migration applied successfully");
    Ok(())
}
```

### Rolling back the last migration

If a migration causes problems, you can undo it:

```rust
use wlite::Database;

fn main() -> wlite::Result<()> {
    let db = Database::open("app.db")?;

    db.rollback_last()?;

    println!("Last migration rolled back");
    Ok(())
}
```

## Schema checking

The `check` method verifies that the live database matches an expected model without making changes. It returns `Ok(())` if the schema is in sync, or an error describing the mismatch.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    match db.check(&model) {
        Ok(()) => {
            println!("Schema is in sync");
        }
        Err(e) => {
            eprintln!("Schema mismatch: {e}");
        }
    }

    Ok(())
}
```

### Using check in CI

Use `check` in continuous integration to verify that the database schema matches the model:

```rust
use wlite::{Database, Model};

fn verify_schema() -> wlite::Result<()> {
    let model = Model::load("schema.wlite")?;
    let db = Database::open("test.db")?;

    db.migrate(&model)?;

    // Verify the migration produced the expected schema
    db.check(&model)?;

    println!("Schema verification passed");
    Ok(())
}
```

## Snapshots

A snapshot captures the current state of the schema as a hash string. This is useful for tracking schema versions and detecting drift.

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    let snapshot = db.snapshot()?;
    println!("Schema snapshot: {snapshot}");

    Ok(())
}
```

### Comparing snapshots

Compare two snapshots to detect schema drift between environments:

```rust
use wlite::{Database, Model};

fn compare_envs(prod_path: &str, staging_path: &str) -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;

    let prod_db = Database::open(prod_path)?;
    let staging_db = Database::open(staging_path)?;

    let prod_hash = prod_db.snapshot()?;
    let staging_hash = staging_db.snapshot()?;

    if prod_hash == staging_hash {
        println!("Schemas are identical");
    } else {
        println!("Schema drift detected");
        println!("  Production:  {prod_hash}");
        println!("  Staging:     {staging_hash}");
    }

    Ok(())
}
```

## Schema hashing

The `hash` method computes a fingerprint of the model's schema. This is a deterministic hash that changes whenever the schema changes.

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Model::load("app.wlite")?;

    let hash = model.hash()?;
    println!("Model schema hash: {hash}");

    Ok(())
}
```

### Using hashes for change detection

```rust
use wlite::Model;

fn check_for_changes(model_path: &str, known_hash: &str) -> wlite::Result<bool> {
    let model = Model::load(model_path)?;
    let current_hash = model.hash()?;

    Ok(current_hash != known_hash)
}
```

## Compiled models

Compiled models are binary representations of the schema that load faster than text files. Use the CLI to compile a model:

```bash
wlite compile schema.wlite schema.wlitem
```

Then load the compiled model in Rust:

```rust
use wlite::Model;

fn main() -> wlite::Result<()> {
    let model = Model::load_compiled("schema.wlitem")?;
    println!("Compiled model loaded");
    Ok(())
}
```

### Compiling at build time

You can compile models as part of your build process using a build script:

```rust
// build.rs
use std::process::Command;

fn main() {
    let status = Command::new("wlite")
        .args(["compile", "schema.wlite", "schema.wlitem"])
        .status()
        .expect("Failed to run wlite compile");

    assert!(status.success(), "wlite compile failed");
}
```

Then load the compiled model in your application:

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let model = Model::load_compiled("schema.wlitem")?;
    let db = Database::open("app.db")?;

    db.migrate(&model)?;

    println!("Migration from compiled model complete");
    Ok(())
}
```

## Complete migration example

This example demonstrates a full migration workflow with error handling, diffing, planning, and snapshot verification:

```rust
use wlite::{Database, Error, Model};

fn run_migration(model_path: &str, db_path: &str) -> wlite::Result<()> {
    // Load the model
    let model = Model::load(model_path)?;

    // Open the database
    let db = Database::open(db_path)?;

    // Check for pending changes
    let diffs = db.diff(&model)?;

    if diffs.is_empty() {
        println!("Schema is already up to date");
        return Ok(());
    }

    println!("{} changes detected:", diffs.len());
    for diff in &diffs {
        println!("  {}", diff.detail);
    }

    // Generate a plan
    let plan = db.plan(&model)?;

    println!("\nMigration plan:");
    for (i, step) in plan.steps.iter().enumerate() {
        println!("  Step {}: {}", i + 1, step.sql);
    }

    // Apply the migration
    db.migrate(&model)?;

    // Verify the result
    db.check(&model)?;

    // Take a snapshot
    let snapshot = db.snapshot()?;
    println!("\nMigration complete. Schema snapshot: {snapshot}");

    Ok(())
}

fn main() {
    match run_migration("app.wlite", "app.db") {
        Ok(()) => {
            println!("All done");
        }
        Err(Error::Parse(msg)) => {
            eprintln!("Schema parse error: {msg}");
        }
        Err(Error::Model(msg)) => {
            eprintln!("Model error: {msg}");
        }
        Err(Error::Sqlite(msg)) => {
            eprintln!("SQLite error: {msg}");
        }
        Err(Error::Busy) => {
            eprintln!("Database is locked. Retry later.");
        }
        Err(e) => {
            eprintln!("Migration failed: {e}");
        }
    }
}
```

## Migration with multiple models

You can load multiple models and migrate against each one sequentially:

```rust
use wlite::{Database, Model};

fn main() -> wlite::Result<()> {
    let base_model = Model::load("base.wlite")?;
    let app_model = Model::load("app.wlite")?;

    let db = Database::open("app.db")?;

    db.migrate(&base_model)?;
    db.migrate(&app_model)?;

    println!("All models applied");
    Ok(())
}
```

## Migration in tests

Use migrations in integration tests to set up a fresh database:

```rust
#[cfg(test)]
mod tests {
    use wlite::{Database, Model};

    fn setup_db() -> wlite::Result<Database> {
        let model = Model::load("schema.wlite")?;
        let db = Database::open(":memory:")?;
        db.migrate(&model)?;
        Ok(db)
    }

    #[test]
    fn test_insert_and_query() -> wlite::Result<()> {
        let db = setup_db()?;

        db.execute("INSERT INTO users (name, email) VALUES ('Test', 'test@example.com')")?;

        let count: i64 = db.query_scalar("SELECT COUNT(*) FROM users")?;
        assert_eq!(count, 1);

        Ok(())
    }
}
```
