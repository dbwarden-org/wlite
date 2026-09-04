---
title: C# Error Handling
description: WliteException class, error codes, exception handling patterns, cleanup, thread safety, and memory management.
---

# C# Error Handling

The wlite C# binding uses exception-based error handling. All operations that can fail throw `WliteException` with a `Result` property indicating the specific error code. This guide covers the exception class, all error codes, handling patterns, resource cleanup, thread safety, and memory management.

## The WliteException class

`WliteException` is the single exception type thrown by all wlite operations. It inherits from `System.Exception` and adds a `Result` property of type `WliteResult`.

### Exception structure

```csharp
using Wlite;

public class WliteException : Exception
{
    public WliteResult Result { get; }

    public WliteException(WliteResult result)
        : base(GetMessage(result))
    {
        Result = result;
    }
}
```

### Catching WliteException

Always catch `WliteException` to access the error code. The `Message` property contains a human-readable description from the native library.

```csharp
using Wlite;

try
{
    using var db = Database.Open("app.db");
    db.Execute("INSERT INTO nonexistent (col) VALUES (1)");
}
catch (WliteException ex)
{
    Console.WriteLine($"Error code: {ex.Result}");
    Console.WriteLine($"Error message: {ex.Message}");
}
```

### Exception properties

| Property | Type | Description |
|----------|------|-------------|
| `Result` | `WliteResult` | The error code from the native library |
| `Message` | `string` | Human-readable error description |
| `StackTrace` | `string` | .NET stack trace at the throw point |

## Error codes

The `WliteResult` enum defines all possible error codes returned by the native library.

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WliteResult.OK` | Success, operation completed without error |
| -1 | `WliteResult.Error` | General or unspecified error |
| -2 | `WliteResult.NotFound` | Requested resource does not exist |
| -3 | `WliteResult.Memory` | Memory allocation failed |
| -4 | `WliteResult.Io` | I/O error reading or writing files |
| -5 | `WliteResult.Corrupt` | Database or schema file is corrupted |
| -6 | `WliteResult.Range` | Value out of valid range |

### OK (0)

The operation succeeded. You will never see this code in an exception because exceptions are only thrown on non-zero results.

```csharp
// This never throws
using var db = Database.Open("app.db");
// Result is implicitly OK
```

### Error (-1)

A general error occurred. This is a catch-all for errors that do not have a specific code. Check the exception message for details.

```csharp
try
{
    // Some operation that triggers a general error
    using var db = Database.Open("");
}
catch (WliteException ex) when (ex.Result == WliteResult.Error)
{
    Console.WriteLine($"General error: {ex.Message}");
}
```

### NotFound (-2)

A requested resource was not found. This can occur when trying to access a table, column, or file that does not exist.

```csharp
try
{
    using var model = Model.Load("missing_schema.wlite");
}
catch (WliteException ex) when (ex.Result == WliteResult.NotFound)
{
    Console.WriteLine("Schema file not found");
}
```

### Memory (-3)

The native library failed to allocate memory. This is rare in practice but can occur with very large result sets or under memory pressure.

```csharp
try
{
    using var db = Database.Open("app.db");
    using var stmt = db.Prepare("SELECT * FROM huge_table");
    // Processing might trigger memory allocation failure
}
catch (WliteException ex) when (ex.Result == WliteResult.Memory)
{
    Console.WriteLine("Out of memory");
}
```

### Io (-4)

An I/O error occurred. This can happen when the database file is on a network share, a removable drive is disconnected, or disk space is exhausted.

```csharp
try
{
    using var db = Database.Open("/mnt/network/share/app.db");
}
catch (WliteException ex) when (ex.Result == WliteResult.Io)
{
    Console.WriteLine($"I/O error: {ex.Message}");
}
```

### Corrupt (-5)

The database or schema file is corrupted. This can indicate disk corruption, incomplete writes, or file tampering.

```csharp
try
{
    using var db = Database.Open("possibly_corrupted.db");
}
catch (WliteException ex) when (ex.Result == WliteResult.Corrupt)
{
    Console.WriteLine("Database appears corrupted");
}
```

### Range (-6)

A value is out of the valid range. This can occur with invalid column indices, parameter indices, or numeric values.

```csharp
try
{
    using var db = Database.Open("app.db");
    using var stmt = db.Prepare("SELECT id FROM users");
    stmt.Step();

    // Column index 99 does not exist
    var value = stmt.ColumnInt64(99);
}
catch (WliteException ex) when (ex.Result == WliteResult.Range)
{
    Console.WriteLine("Column index out of range");
}
```

## Exception handling patterns

### Try-catch for individual operations

The simplest pattern wraps individual operations in try-catch blocks.

```csharp
using Wlite;

void InsertWithRecovery(Database db, string name, string email)
{
    try
    {
        using var stmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
        Console.WriteLine($"Inserted {name}");
    }
    catch (WliteException ex)
    {
        Console.WriteLine($"Failed to insert {name}: {ex.Message}");
    }
}
```

### Catching specific error codes

Use pattern matching to handle specific error codes differently.

```csharp
using Wlite;

void RobustOpen(string dbPath)
{
    Database db;

    try
    {
        db = Database.Open(dbPath);
    }
    catch (WliteException ex) when (ex.Result == WliteResult.NotFound)
    {
        Console.WriteLine("Database not found, creating new one");
        db = Database.Open(dbPath);
    }
    catch (WliteException ex) when (ex.Result == WliteResult.Corrupt)
    {
        Console.WriteLine("Database corrupted, attempting recovery");
        File.Delete(dbPath);
        db = Database.Open(dbPath);
    }
    catch (WliteException ex) when (ex.Result == WliteResult.Io)
    {
        Console.WriteLine("I/O error, check file permissions");
        throw;
    }

    using (db)
    {
        // Use the database
    }
}
```

### Retry with exponential backoff

For transient errors like `Busy`, retry the operation with increasing delays.

```csharp
using Wlite;
using System.Threading;

void RetryOnBusy(Action action, int maxRetries = 5)
{
    for (int attempt = 0; attempt < maxRetries; attempt++)
    {
        try
        {
            action();
            return;
        }
        catch (WliteException ex) when (ex.Result == WliteResult.Busy)
        {
            if (attempt == maxRetries - 1) throw;

            int delay = (int)Math.Pow(2, attempt) * 100;
            Console.WriteLine($"Database busy, retrying in {delay}ms...");
            Thread.Sleep(delay);
        }
    }
}
```

### Aggregate exception handling

When performing multiple operations, collect all errors rather than failing on the first one.

```csharp
using Wlite;
using System.Collections.Generic;

class MigrationResult
{
    public List<string> Successes { get; } = new();
    public List<(string Operation, string Error)> Failures { get; } = new();
}

MigrationResult BatchMigrate(string modelPath, string[] dbPaths)
{
    var result = new MigrationResult();

    using var model = Model.Load(modelPath);

    foreach (var dbPath in dbPaths)
    {
        try
        {
            using var db = Database.Open(dbPath);
            db.Migrate(model);
            result.Successes.Add(dbPath);
        }
        catch (WliteException ex)
        {
            result.Failures.Add((dbPath, ex.Message));
        }
    }

    return result;
}
```

## Cleanup with using statements

The `using` statement ensures deterministic cleanup of native resources. Always use `using` for `Database`, `Model`, `Statement`, and `Transaction` objects.

### Basic using pattern

```csharp
using Wlite;

void ProcessData()
{
    using var model = Model.Load("app.wlite");
    using var db = Database.Open("app.db");

    db.Migrate(model);

    using var stmt = db.Prepare("SELECT * FROM users");
    while (stmt.Step())
    {
        var name = stmt.ColumnText(1);
        Console.WriteLine(name);
    }
}
// stmt, db, and model are all disposed here
```

### Using with try-catch

Combine `using` with try-catch for both cleanup and error handling.

```csharp
using Wlite;

void SafeProcess()
{
    using var model = Model.Load("app.wlite");
    using var db = Database.Open("app.db");

    db.Migrate(model);

    using var tx = db.Begin();
    bool committed = false;

    try
    {
        using var stmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.Bind(1, "Alice");
        stmt.Bind(2, "alice@example.com");
        stmt.Step();

        tx.Commit();
        committed = true;
    }
    catch (WliteException ex)
    {
        Console.WriteLine($"Error: {ex.Message}");
    }
    finally
    {
        if (!committed)
        {
            tx.Rollback();
        }
    }
}
// stmt, tx, db, and model are all disposed in order
```

### Manual disposal patterns

When you cannot use `using` declarations, use try-finally blocks.

```csharp
using Wlite;

void ManualCleanup()
{
    Database db = null;
    Statement stmt = null;

    try
    {
        db = Database.Open("app.db");
        stmt = db.Prepare("SELECT * FROM users");

        while (stmt.Step())
        {
            var name = stmt.ColumnText(1);
            Console.WriteLine(name);
        }
    }
    finally
    {
        stmt?.Dispose();
        db?.Dispose();
    }
}
```

### Disposing in reverse order

Always dispose resources in reverse order of creation. Statements depend on databases, and databases may depend on models.

```csharp
using Wlite;

void CorrectDisposalOrder()
{
    using var model = Model.Load("app.wlite");    // Created first
    using var db = Database.Open("app.db");       // Created second
    using var stmt = db.Prepare("SELECT 1");      // Created third

    // Disposal order: stmt, db, model (reverse of creation)
}
```

## Thread safety

The wlite C# binding has specific thread safety guarantees that you must understand to use it correctly.

### Thread-safe objects

- `Model` objects are immutable after loading and can be shared across threads
- `WliteResult` and `ValueType` enums are value types and are inherently thread-safe
- Static methods like `WliteVersion.Get` are thread-safe

### Thread-unsafe objects

- `Database` objects are not thread-safe. Each thread must use its own instance.
- `Statement` objects are not thread-safe. They belong to a specific database connection.
- `Transaction` objects are not thread-safe. They belong to a specific database connection.

### Sharing models across threads

Models can be shared because they are immutable after loading. This is the recommended pattern for multi-threaded applications.

```csharp
using Wlite;
using System.Threading.Tasks;

void MultiThreadedWork()
{
    using var model = Model.Load("app.wlite");

    var tasks = new Task[4];
    for (int i = 0; i < 4; i++)
    {
        int workerId = i;
        tasks[i] = Task.Run(() =>
        {
            // Each thread gets its own database connection
            using var db = Database.Open("app.db");
            db.Migrate(model); // model is safe to share

            for (int j = 0; j < 100; j++)
            {
                db.Execute(
                    "INSERT INTO work (thread_id, data) VALUES (?, ?)",
                    workerId, $"item_{j}"
                );
            }
        });
    }

    Task.WaitAll(tasks);
}
```

### Database connection per thread

Never share a `Database` instance across threads. Use one connection per thread or a connection pool.

```csharp
using Wlite;
using System.Collections.Concurrent;

class ConnectionPool : IDisposable
{
    private readonly ConcurrentBag<Database> _connections = new();
    private readonly Model _model;

    public ConnectionPool(string modelPath, string dbPath, int size)
    {
        _model = Model.Load(modelPath);

        for (int i = 0; i < size; i++)
        {
            var db = Database.Open(dbPath);
            db.Migrate(_model);
            _connections.Add(db);
        }
    }

    public Database Acquire()
    {
        return _connections.TryTake(out var db) ? db : null;
    }

    public void Release(Database db)
    {
        _connections.Add(db);
    }

    public void Dispose()
    {
        while (_connections.TryTake(out var db))
        {
            db.Dispose();
        }
        _model.Dispose();
    }
}
```

### Synchronizing database access

If you must share a database connection, synchronize access explicitly.

```csharp
using Wlite;
using System.Threading;

class SynchronizedDatabase : IDisposable
{
    private readonly Database _db;
    private readonly object _lock = new();

    public SynchronizedDatabase(string dbPath)
    {
        _db = Database.Open(dbPath);
    }

    public void Execute(string sql)
    {
        lock (_lock)
        {
            _db.Execute(sql);
        }
    }

    public T ExecuteWithResult<T>(Func<Database, T> func)
    {
        lock (_lock)
        {
            return func(_db);
        }
    }

    public void Dispose()
    {
        _db?.Dispose();
    }
}
```

## Memory management

The wlite C# binding manages native memory through P/Invoke. Understanding how memory is allocated and freed helps you avoid leaks and performance issues.

### Native memory lifecycle

Each wlite type wraps a native pointer. The lifecycle is:

1. Allocation: The native library allocates memory when you call `Open`, `Load`, `Prepare`, or `Begin`
2. Usage: You interact with the object through its methods
3. Deallocation: The `Dispose` method or finalizer frees the native memory

### Avoiding memory leaks

Always dispose wlite objects. The most common leak pattern is forgetting to dispose statements.

```csharp
using Wlite;

void LeakExample(Database db)
{
    // BAD: Statement is never disposed
    var stmt = db.Prepare("SELECT * FROM users");
    while (stmt.Step()) { }
    // Native memory leaks here
}

void NoLeakExample(Database db)
{
    // GOOD: Statement is disposed automatically
    using var stmt = db.Prepare("SELECT * FROM users");
    while (stmt.Step()) { }
}
```

### Statement reuse and memory

Reusing statements is more efficient than creating new ones. Each `Prepare` call allocates native memory for the compiled statement.

```csharp
using Wlite;

void EfficientBatch(Database db, IEnumerable<(string Name, string Email)> users)
{
    // GOOD: One statement, reused for all inserts
    using var stmt = db.Prepare(
        "INSERT INTO users (name, email) VALUES (?, ?)"
    );

    foreach (var (name, email) in users)
    {
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
        stmt.Reset();
    }
}

void InefficientBatch(Database db, IEnumerable<(string Name, string Email)> users)
{
    // BAD: New statement for each insert
    foreach (var (name, email) in users)
    {
        using var stmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
    }
}
```

### Transaction memory

Transactions hold native resources until they are committed, rolled back, or disposed. Always ensure transactions reach a terminal state.

```csharp
using Wlite;

void TransactionMemory(Database db)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
        db.Execute("INSERT INTO users (name) VALUES ('Alice')");
        tx.Commit();
        committed = true;
    }
    finally
    {
        if (!committed)
        {
            tx.Rollback();
        }
    }
    // tx.Dispose() is called automatically by using
    // Native memory is freed here
}
```

### Monitoring memory usage

For long-running applications, monitor memory usage to detect leaks.

```csharp
using Wlite;
using System.Diagnostics;

void MonitorMemory()
{
    var process = Process.GetCurrentProcess();

    Console.WriteLine($"Before: {process.WorkingSet64 / 1024 / 1024} MB");

    using var model = Model.Load("app.wlite");
    Console.WriteLine($"After model load: {process.WorkingSet64 / 1024 / 1024} MB");

    using var db = Database.Open("app.db");
    Console.WriteLine($"After db open: {process.WorkingSet64 / 1024 / 1024} MB");

    db.Migrate(model);
    Console.WriteLine($"After migrate: {process.WorkingSet64 / 1024 / 1024} MB");

    using var stmt = db.Prepare("SELECT * FROM users");
    while (stmt.Step()) { }
    Console.WriteLine($"After query: {process.WorkingSet64 / 1024 / 1024} MB");
}
```

## Complete error handling example

This example demonstrates comprehensive error handling for a production application.

```csharp
using Wlite;
using System;
using System.Collections.Generic;

class ProductionDatabase : IDisposable
{
    private readonly Database _db;
    private readonly Model _model;

    public ProductionDatabase(string modelPath, string dbPath)
    {
        _model = LoadModelWithRetry(modelPath);
        _db = OpenDatabaseWithRetry(dbPath);
        MigrateWithRetry();
    }

    private Model LoadModelWithRetry(string path, int maxRetries = 3)
    {
        for (int i = 0; i < maxRetries; i++)
        {
            try
            {
                return Model.Load(path);
            }
            catch (WliteException ex) when (ex.Result == WliteResult.Io && i < maxRetries - 1)
            {
                Console.WriteLine($"Retry {i + 1}: {ex.Message}");
                System.Threading.Thread.Sleep(100 * (i + 1));
            }
        }

        return Model.Load(path);
    }

    private Database OpenDatabaseWithRetry(string path, int maxRetries = 3)
    {
        for (int i = 0; i < maxRetries; i++)
        {
            try
            {
                return Database.Open(path);
            }
            catch (WliteException ex) when (
                (ex.Result == WliteResult.Busy || ex.Result == WliteResult.Io)
                && i < maxRetries - 1
            )
            {
                Console.WriteLine($"Retry {i + 1}: {ex.Message}");
                System.Threading.Thread.Sleep(100 * (i + 1));
            }
        }

        return Database.Open(path);
    }

    private void MigrateWithRetry()
    {
        for (int i = 0; i < 3; i++)
        {
            try
            {
                _db.Migrate(_model);
                return;
            }
            catch (WliteException ex) when (ex.Result == WliteResult.Busy && i < 2)
            {
                Console.WriteLine($"Migration retry {i + 1}");
                System.Threading.Thread.Sleep(100 * (i + 1));
            }
        }

        _db.Migrate(_model);
    }

    public void Dispose()
    {
        _db?.Dispose();
        _model?.Dispose();
    }

    public bool InsertUser(string name, string email)
    {
        try
        {
            using var stmt = _db.Prepare(
                "INSERT INTO users (name, email) VALUES (?, ?)"
            );
            stmt.Bind(1, name);
            stmt.Bind(2, email);
            stmt.Step();
            return true;
        }
        catch (WliteException ex) when (ex.Result == WliteResult.Range)
        {
            Console.WriteLine("Invalid input data");
            return false;
        }
        catch (WliteException ex)
        {
            Console.WriteLine($"Insert failed: {ex.Result} - {ex.Message}");
            return false;
        }
    }

    public List<(long Id, string Name, string Email)> ListUsers()
    {
        var users = new List<(long Id, string Name, string Email)>();

        try
        {
            using var stmt = _db.Prepare(
                "SELECT id, name, email FROM users ORDER BY name"
            );

            while (stmt.Step())
            {
                users.Add((
                    stmt.ColumnInt64(0),
                    stmt.ColumnText(1),
                    stmt.ColumnText(2)
                ));
            }
        }
        catch (WliteException ex)
        {
            Console.WriteLine($"Query failed: {ex.Message}");
        }

        return users;
    }

    public bool TransferCredits(long fromId, long toId, long amount)
    {
        using var tx = _db.Begin();
        bool committed = false;

        try
        {
            using var debit = _db.Prepare(
                "UPDATE accounts SET balance = balance - ? WHERE id = ?"
            );
            debit.Bind(1, amount);
            debit.Bind(2, fromId);
            debit.Step();

            using var credit = _db.Prepare(
                "UPDATE accounts SET balance = balance + ? WHERE id = ?"
            );
            credit.Bind(1, amount);
            credit.Bind(2, toId);
            credit.Step();

            using var check = _db.Prepare(
                "SELECT balance FROM accounts WHERE id = ?"
            );
            check.Bind(1, fromId);

            if (check.Step() && check.ColumnInt64(0) < 0)
            {
                throw new InvalidOperationException("Insufficient funds");
            }

            tx.Commit();
            committed = true;
            return true;
        }
        catch (WliteException ex)
        {
            Console.WriteLine($"Transfer failed: {ex.Message}");
            return false;
        }
        catch (InvalidOperationException ex)
        {
            Console.WriteLine($"Transfer rejected: {ex.Message}");
            return false;
        }
        finally
        {
            if (!committed)
            {
                tx.Rollback();
            }
        }
    }
}

class Program
{
    static void Main()
    {
        try
        {
            using var db = new ProductionDatabase("app.wlite", "app.db");

            db.InsertUser("Alice", "alice@example.com");
            db.InsertUser("Bob", "bob@example.com");

            Console.WriteLine("All users:");
            foreach (var (id, name, email) in db.ListUsers())
            {
                Console.WriteLine($"  {id}: {name} <{email}>");
            }
        }
        catch (WliteException ex)
        {
            Console.WriteLine($"Fatal error: {ex.Result} - {ex.Message}");
            Environment.Exit(1);
        }
    }
}
```
