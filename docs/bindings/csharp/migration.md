---
title: C# Migration Guide
description: Schema migration, diff, plan, check, snapshot, and hash operations for the C# binding.
---

# C# Migration Guide

The wlite C# binding provides a complete migration system for managing database schemas. You define your schema in `.wlite` files, load them as `Model` objects, and apply them to databases using `Database.Migrate`. The binding also supports previewing changes, generating migration plans, verifying schema integrity, and producing snapshots.

## Loading models

The `Model.Load` static method reads a `.wlite` schema file and returns a `Model` object. The model is immutable after loading and can be reused to migrate multiple databases.

### Basic model loading

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
```

### Loading from different paths

Models can be loaded from any accessible path. Use relative paths for files in the application directory and absolute paths for system-wide schemas.

```csharp
using Wlite;

// Relative path (from working directory)
using var model = Model.Load("schema/app.wlite");

// Absolute path
using var model = Model.Load("/etc/myapp/schema.wlite");

// Combine with path utilities
var schemaPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "app.wlite");
using var model = Model.Load(schemaPath);
```

### Handling load errors

If the file does not exist or contains invalid syntax, `Model.Load` throws a `WliteException`.

```csharp
using Wlite;

try
{
    using var model = Model.Load("missing.wlite");
}
catch (WliteException ex)
{
    Console.WriteLine($"Failed to load model: {ex.Result} - {ex.Message}");
}
```

### Loading models in bulk

When you need to load multiple schema files, load each one and dispose them when done. Models are independent and do not share state.

```csharp
using Wlite;

var schemas = new[] { "users.wlite", "orders.wlite", "analytics.wlite" };
var models = new List<Model>();

try
{
    foreach (var schema in schemas)
    {
        models.Add(Model.Load(schema));
    }

    using var db = Database.Open("app.db");
    foreach (var model in models)
    {
        db.Migrate(model);
    }
}
finally
{
    foreach (var model in models)
    {
        model.Dispose();
    }
}
```

## Opening databases

The `Database.Open` static method creates or opens an SQLite database at the specified path. If the file does not exist, it is created.

### Basic database opening

```csharp
using Wlite;

using var db = Database.Open("app.db");
```

### Opening in-memory databases

For testing or temporary operations, use an in-memory database. Note that in-memory databases exist only for the lifetime of the connection.

```csharp
using Wlite;

using var db = Database.Open(":memory:");
```

### Handling open errors

If the database cannot be opened due to permissions, corruption, or other issues, `Database.Open` throws a `WliteException`.

```csharp
using Wlite;

try
{
    using var db = Database.Open("/read-only-path/app.db");
}
catch (WliteException ex)
{
    Console.WriteLine($"Cannot open database: {ex.Result} - {ex.Message}");
}
```

## Running migrations

The `Database.Migrate` method applies a model to a database. It creates tables, adds columns, and modifies constraints to match the model definition. Existing data is preserved whenever possible.

### Basic migration

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

db.Migrate(model);
```

### Migrating multiple models

You can apply multiple models to the same database. Tables from each model are created or altered independently.

```csharp
using Wlite;

using var usersModel = Model.Load("users.wlite");
using var ordersModel = Model.Load("orders.wlite");
using var db = Database.Open("app.db");

db.Migrate(usersModel);
db.Migrate(ordersModel);
```

### Migration with error handling

Migration can fail if the schema is invalid or if the database is locked. Always wrap migrations in try-catch blocks for production code.

```csharp
using Wlite;

void SafeMigrate(string modelPath, string dbPath)
{
    try
    {
        using var model = Model.Load(modelPath);
        using var db = Database.Open(dbPath);
        db.Migrate(model);
        Console.WriteLine("Migration completed successfully");
    }
    catch (WliteException ex)
    {
        Console.WriteLine($"Migration failed: {ex.Result} - {ex.Message}");
    }
}
```

### Idempotent migrations

Running the same migration multiple times is safe. The `Migrate` method checks the current schema state and only applies changes that are needed.

```csharp
using Wlite;

using var model = Model.Load("app.wlite");

// First run creates tables
using var db1 = Database.Open("first.db");
db1.Migrate(model);

// Second run is a no-op if schema already matches
db1.Migrate(model);

// Different database, same model
using var db2 = Database.Open("second.db");
db2.Migrate(model);
```

## Diffing schemas

The `Diff` operation compares a model against the current database schema and returns the changes that would be applied. This is useful for previewing migrations before running them.

### Basic diff

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

var plan = db.Diff(model);
```

### Using diff to preview changes

Before applying a migration, run a diff to see what changes will be made. This helps avoid unexpected schema modifications.

```csharp
using Wlite;

void PreviewMigration(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    var plan = db.Diff(model);

    if (plan.HasChanges)
    {
        Console.WriteLine("Migration will apply the following changes:");
        foreach (var change in plan.Changes)
        {
            Console.WriteLine($"  {change}");
        }
    }
    else
    {
        Console.WriteLine("Database is already up to date");
    }
}
```

## Migration plans

The `Plan` operation generates a detailed migration plan without executing it. The plan includes the SQL statements that will be run and the order in which they will execute.

### Generating a migration plan

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

var plan = db.Plan(model);

Console.WriteLine($"Planned operations: {plan.Operations.Count}");
foreach (var op in plan.Operations)
{
    Console.WriteLine($"  {op.Type}: {op.Sql}");
}
```

### Dry run pattern

Use the plan to perform a dry run of your migration. This is useful in continuous integration pipelines where you want to verify migrations without modifying databases.

```csharp
using Wlite;

bool DryRunMigration(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    var plan = db.Plan(model);

    if (plan.HasChanges)
    {
        Console.WriteLine("Dry run results:");
        Console.WriteLine($"  Operations: {plan.Operations.Count}");
        Console.WriteLine($"  Will modify: {plan.WillModifyTables}");

        // Log the SQL that would be executed
        foreach (var op in plan.Operations)
        {
            Console.WriteLine($"  [{op.Type}] {op.Sql}");
        }

        return true;
    }

    Console.WriteLine("No changes needed");
    return false;
}
```

## Schema checking

The `Check` operation verifies that the current database schema matches the model. It returns a result indicating whether the schema is in sync.

### Basic check

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

var result = db.Check(model);

if (result.IsValid)
{
    Console.WriteLine("Schema is valid");
}
else
{
    Console.WriteLine("Schema mismatch detected");
    foreach (var issue in result.Issues)
    {
        Console.WriteLine($"  {issue}");
    }
}
```

### Pre-deployment schema validation

Use the check operation in deployment scripts to verify that databases are ready for new code.

```csharp
using Wlite;

bool ValidateDeployment(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    var result = db.Check(model);

    if (!result.IsValid)
    {
        Console.WriteLine("Deployment validation failed:");
        foreach (var issue in result.Issues)
        {
            Console.WriteLine($"  {issue}");
        }
        return false;
    }

    Console.WriteLine("Deployment validation passed");
    return true;
}
```

### Checking multiple databases

You can check multiple databases against the same model to find inconsistencies across your infrastructure.

```csharp
using Wlite;

void CheckAllDatabases(string modelPath, string[] dbPaths)
{
    using var model = Model.Load(modelPath);

    foreach (var dbPath in dbPaths)
    {
        try
        {
            using var db = Database.Open(dbPath);
            var result = db.Check(model);

            var status = result.IsValid ? "OK" : "MISMATCH";
            Console.WriteLine($"  {dbPath}: {status}");

            if (!result.IsValid)
            {
                foreach (var issue in result.Issues)
                {
                    Console.WriteLine($"    {issue}");
                }
            }
        }
        catch (WliteException ex)
        {
            Console.WriteLine($"  {dbPath}: ERROR - {ex.Message}");
        }
    }
}
```

## Snapshotting

The `Snapshot` operation captures the current state of the database schema. Snapshots are useful for versioning, rollback, and auditing.

### Creating a snapshot

```csharp
using Wlite;

using var db = Database.Open("app.db");

var snapshot = db.Snapshot();

Console.WriteLine($"Snapshot captured at {snapshot.Timestamp}");
Console.WriteLine($"Tables: {snapshot.Tables.Count}");
```

### Snapshot before migration

Always snapshot your database before applying a migration so you can verify or rollback changes.

```csharp
using Wlite;

void SafeMigrateWithSnapshot(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    // Snapshot before migration
    var beforeSnapshot = db.Snapshot();
    Console.WriteLine($"Before: {beforeSnapshot.Tables.Count} tables");

    // Apply migration
    db.Migrate(model);

    // Snapshot after migration
    var afterSnapshot = db.Snapshot();
    Console.WriteLine($"After: {afterSnapshot.Tables.Count} tables");

    // Compare snapshots
    var differences = beforeSnapshot.Compare(afterSnapshot);
    if (differences.Count > 0)
    {
        Console.WriteLine("Changes applied:");
        foreach (var diff in differences)
        {
            Console.WriteLine($"  {diff}");
        }
    }
}
```

### Comparing snapshots

Snapshots can be compared to detect changes between two points in time.

```csharp
using Wlite;

void CompareDatabaseStates(string dbPath)
{
    using var db = Database.Open(dbPath);

    // Take a snapshot of the current state
    var snapshot1 = db.Snapshot();

    // Perform some operations
    db.Execute(@"
        ALTER TABLE users ADD COLUMN phone TEXT
    ");

    // Take another snapshot
    var snapshot2 = db.Snapshot();

    // Compare
    var changes = snapshot1.Compare(snapshot2);
    Console.WriteLine($"Changes detected: {changes.Count}");
    foreach (var change in changes)
    {
        Console.WriteLine($"  {change}");
    }
}
```

## Schema hashing

The `Hash` operation computes a hash of the current database schema. This is useful for detecting schema drift across environments.

### Computing schema hash

```csharp
using Wlite;

using var db = Database.Open("app.db");

var hash = db.Hash();
Console.WriteLine($"Schema hash: {hash}");
```

### Comparing schema hashes

Compare schema hashes across environments to ensure consistency.

```csharp
using Wlite;

bool CompareSchemas(string devDbPath, string prodDbPath)
{
    using var devDb = Database.Open(devDbPath);
    using var prodDb = Database.Open(prodDbPath);

    var devHash = devDb.Hash();
    var prodHash = prodDb.Hash();

    if (devHash == prodHash)
    {
        Console.WriteLine("Schemas are identical");
        return true;
    }

    Console.WriteLine("Schemas differ:");
    Console.WriteLine($"  Dev:  {devHash}");
    Console.WriteLine($"  Prod: {prodHash}");
    return false;
}
```

### Hash-based migration detection

Use schema hashes to detect when a migration is needed without running a full diff.

```csharp
using Wlite;

bool NeedsMigration(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    // Take snapshot and hash before migration
    var beforeHash = db.Hash();

    // Simulate migration to compute target hash
    // The model hash represents the desired state
    var modelHash = model.Hash();

    return beforeHash != modelHash;
}
```

## Compiled models

For performance-sensitive applications, you can pre-compile models. Compiled models are faster to load and validate because they cache the parsed schema.

### Using compiled models

```csharp
using Wlite;

// Load a compiled model
using var model = Model.LoadCompiled("app.wlitem");

// Use it like a regular model
using var db = Database.Open("app.db");
db.Migrate(model);
```

### Compiling a model

```csharp
using Wlite;

// Compile a model file
Model.Compile("app.wlite", "app.wlitem");

// Load the compiled version
using var model = Model.LoadCompiled("app.wlitem");
```

### Compiled model in build pipeline

Integrate model compilation into your build process for faster startup times.

```csharp
using Wlite;

class ModelManager : IDisposable
{
    private Model _model;
    private readonly string _modelPath;
    private readonly string _compiledPath;

    public ModelManager(string modelPath)
    {
        _modelPath = modelPath;
        _compiledPath = Path.ChangeExtension(modelPath, ".wlitem");

        if (File.Exists(_compiledPath))
        {
            // Load compiled model if available
            _model = Model.LoadCompiled(_compiledPath);
        }
        else
        {
            // Load and compile the model
            _model = Model.Load(_modelPath);
            _model.Compile(_compiledPath);
        }
    }

    public void Migrate(Database db)
    {
        db.Migrate(_model);
    }

    public void Dispose()
    {
        _model?.Dispose();
    }
}
```

## Complete migration workflow

This example demonstrates a complete migration workflow with all operations.

```csharp
using Wlite;
using System;

class MigrationWorkflow : IDisposable
{
    private readonly Model _model;
    private readonly Database _db;

    public MigrationWorkflow(string modelPath, string dbPath)
    {
        _model = Model.Load(modelPath);
        _db = Database.Open(dbPath);
    }

    public void Dispose()
    {
        _db?.Dispose();
        _model?.Dispose();
    }

    public void RunFullWorkflow()
    {
        Console.WriteLine("=== Migration Workflow ===");
        Console.WriteLine();

        // Step 1: Check current state
        Console.WriteLine("1. Checking current schema...");
        var checkResult = _db.Check(_model);
        if (checkResult.IsValid)
        {
            Console.WriteLine("   Schema is up to date");
            return;
        }
        Console.WriteLine($"   Found {checkResult.Issues.Count} issues");

        // Step 2: Preview changes
        Console.WriteLine();
        Console.WriteLine("2. Previewing migration...");
        var plan = _db.Plan(_model);
        Console.WriteLine($"   {plan.Operations.Count} operations planned");

        // Step 3: Snapshot before migration
        Console.WriteLine();
        Console.WriteLine("3. Taking snapshot...");
        var beforeSnapshot = _db.Snapshot();
        Console.WriteLine($"   {beforeSnapshot.Tables.Count} tables before migration");

        // Step 4: Compute diff
        Console.WriteLine();
        Console.WriteLine("4. Computing diff...");
        var diff = _db.Diff(_model);
        Console.WriteLine($"   {diff.Changes.Count} changes detected");

        // Step 5: Apply migration
        Console.WriteLine();
        Console.WriteLine("5. Applying migration...");
        _db.Migrate(_model);
        Console.WriteLine("   Migration complete");

        // Step 6: Snapshot after migration
        Console.WriteLine();
        Console.WriteLine("6. Taking post-migration snapshot...");
        var afterSnapshot = _db.Snapshot();
        Console.WriteLine($"   {afterSnapshot.Tables.Count} tables after migration");

        // Step 7: Verify
        Console.WriteLine();
        Console.WriteLine("7. Verifying migration...");
        var verifyResult = _db.Check(_model);
        if (verifyResult.IsValid)
        {
            Console.WriteLine("   Migration verified successfully");
        }
        else
        {
            Console.WriteLine("   WARNING: Schema verification failed");
            foreach (var issue in verifyResult.Issues)
            {
                Console.WriteLine($"   {issue}");
            }
        }

        // Step 8: Hash for comparison
        Console.WriteLine();
        Console.WriteLine("8. Schema hash:");
        var hash = _db.Hash();
        Console.WriteLine($"   {hash}");
    }
}

class Program
{
    static void Main()
    {
        using var workflow = new MigrationWorkflow("app.wlite", "app.db");
        workflow.RunFullWorkflow();
    }
}
```
