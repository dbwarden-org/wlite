---
title: C# Binding
description: C# P/Invoke binding for wlite.
---

# C# Binding

The `wlite` NuGet package provides C# access to libwlite via P/Invoke. Define your schema in `.wlite` and use it from .NET.

The binding wraps the libwlite C library and exposes it through C# types with proper resource management using `IDisposable`, exception-based error handling, and .NET conventions.

## Installation

```bash
dotnet add package wlite
```

Requires libwlite to be installed on your system. Build and install it from the libwlite repository:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

### NuGet package reference

```xml
<PackageReference Include="wlite" Version="0.1.*" />
```

### Verifying the installation

```csharp
using Wlite;

var version = Wlite.Version.GetVersion();
Console.WriteLine($"wlite version: {version}");
```

## Basic usage

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

db.Migrate(model);

using var stmt = db.Prepare("SELECT * FROM users");
while (stmt.Step())
{
    var id = stmt.ColumnInt64(0);
    var name = stmt.ColumnText(1);
    var email = stmt.ColumnText(2);
    Console.WriteLine($"{id}: {name} <{email}>");
}
```

### Full workflow example

```csharp
using Wlite;

class Program
{
    static void Main()
    {
        using var model = Model.Load("app.wlite");
        using var db = Database.Open("app.db");

        db.Migrate(model);

        // Create tables if not using migrations
        db.Execute(@"
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                created_at TEXT DEFAULT (datetime('now'))
            )
        ");

        // Insert users
        var users = new (string Name, string Email)[]
        {
            ("Alice", "alice@example.com"),
            ("Bob", "bob@example.com"),
            ("Charlie", "charlie@example.com"),
        };

        using var insertStmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );

        foreach (var (name, email) in users)
        {
            insertStmt.Bind(1, name);
            insertStmt.Bind(2, email);
            insertStmt.Step();
            insertStmt.Reset();
        }

        // Query users
        using var queryStmt = db.Prepare("SELECT id, name, email FROM users ORDER BY name");
        while (queryStmt.Step())
        {
            var id = queryStmt.ColumnInt64(0);
            var name = queryStmt.ColumnText(1);
            var email = queryStmt.ColumnText(2);
            Console.WriteLine($"{id}: {name} <{email}>");
        }
    }
}
```

## Types

| C# Type | C Equivalent | Description |
|---------|--------------|-------------|
| `Wlite.Database` | `wlite_db` | Open database connection |
| `Wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `Wlite.Statement` | `wlite_stmt` | Prepared SQL statement |
| `Wlite.Transaction` | `wlite_tx` | Active transaction |
| `Wlite.Error` | `wlite_result` | Exception type |

The `Database` type manages the connection lifetime and provides methods for executing SQL, preparing statements, and querying data.

The `Model` type represents a parsed `.wlite` schema. It is immutable after loading and can be used to migrate multiple databases.

The `Statement` type wraps a prepared SQL statement with parameter binding and column access methods.

## Database operations

```csharp
using Wlite;

using var db = Database.Open("app.db");

// Execute DDL
db.Execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
db.Execute("CREATE INDEX idx_test_name ON test(name)");

// Execute DML with parameters
db.Execute("INSERT INTO test (name) VALUES (?)", "hello");
db.Execute("UPDATE test SET name = ? WHERE id = ?", "world", 1);
db.Execute("DELETE FROM test WHERE id = ?", 1);

// Query returns list of dictionaries
using var rows = db.Query("SELECT * FROM test");
while (rows.Step())
{
    var id = rows.ColumnInt64(0);
    var name = rows.ColumnText(1);
    Console.WriteLine($"{id}: {name}");
}

// Single value query
var count = db.QueryScalar("SELECT COUNT(*) FROM test");
Console.WriteLine($"Count: {count}");
```

### Batch operations

```csharp
using Wlite;

void BatchInsert(Database db, IEnumerable<(string Name, string Email)> users)
{
    using var stmt = db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");

    foreach (var (name, email) in users)
    {
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
        stmt.Reset();
    }
}
```

### Query with column access

```csharp
using Wlite;

void QueryColumns(Database db)
{
    using var stmt = db.Prepare("SELECT id, name, email, created_at FROM users");

    Console.WriteLine($"Columns: {stmt.ColumnCount()}");

    while (stmt.Step())
    {
        var id = stmt.ColumnInt64(0);
        var name = stmt.ColumnText(1);
        var email = stmt.ColumnText(2);
        var created = stmt.ColumnText(3);

        Console.WriteLine($"User {id}: {name} <{email}> created at {created}");
    }
}
```

## Prepared statements

```csharp
using Wlite;

using var db = Database.Open("app.db");

// Prepare an INSERT statement
using var stmt = db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");

// Insert Alice
stmt.Bind(1, "Alice");
stmt.Bind(2, "alice@example.com");
stmt.Step();
stmt.Reset();

// Insert Bob
stmt.Bind(1, "Bob");
stmt.Bind(2, "bob@example.com");
stmt.Step();
stmt.Reset();

// Insert Charlie
stmt.Bind(1, "Charlie");
stmt.Bind(2, "charlie@example.com");
stmt.Step();

// Prepare a SELECT statement
using var query = db.Prepare("SELECT * FROM users WHERE name = ?");
query.Bind(1, "Alice");

while (query.Step())
{
    var id = query.ColumnInt64(0);
    var name = query.ColumnText(1);
    Console.WriteLine($"Found user {id}: {name}");
}
```

### Column access methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `ColumnCount()` | `int` | Number of columns in result |
| `ColumnName(i)` | `string` | Name of column at index |
| `ColumnType(i)` | `ColumnType` | Data type of column |
| `ColumnInt64(i)` | `long` | Integer value |
| `ColumnDouble(i)` | `double` | Floating point value |
| `ColumnText(i)` | `string` | Text value |

## Transactions

```csharp
using Wlite;

using var db = Database.Open("app.db");

// Using try/finally for transaction safety
using var tx = db.Begin();
try
{
    db.Execute("INSERT INTO users (name) VALUES ('Alice')");
    db.Execute("INSERT INTO users (name) VALUES ('Bob')");

    // Verify before committing
    var count = db.QueryScalar("SELECT COUNT(*) FROM users");
    if (count > 100)
    {
        throw new InvalidOperationException("Too many users");
    }

    tx.Commit();
}
catch
{
    tx.Rollback();
    throw;
}
```

### Transaction with automatic rollback

```csharp
using Wlite;

void BatchWithTransaction(Database db, IEnumerable<Order> orders)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
        using var stmt = db.Prepare(
            "INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)"
        );

        foreach (var order in orders)
        {
            stmt.Bind(1, order.UserId);
            stmt.Bind(2, order.ProductId);
            stmt.Bind(3, order.Quantity);
            stmt.Step();
            stmt.Reset();
        }

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
}
```

## Error handling

All operations throw `WliteException` on failure:

```csharp
using Wlite;

try
{
    var db = Database.Open("app.db");
}
catch (WliteException ex)
{
    Console.WriteLine($"Error: {ex.Result} - {ex.Message}");
}
```

### Error codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WliteResult.OK` | Success |
| 1 | `WliteResult.Error` | General error |
| 2 | `WliteResult.InvalidArgument` | Null pointer or invalid parameter |
| 3 | `WliteResult.OutOfMemory` | Allocation failed |
| 4 | `WliteResult.IoError` | I/O error |
| 5 | `WliteResult.ParseError` | Schema parse error |
| 6 | `WliteResult.ModelError` | Schema model error |
| 7 | `WliteResult.SqliteError` | SQLite error |
| 8 | `WliteResult.ConstraintError` | Constraint violation |
| 9 | `WliteResult.NotFound` | Resource not found |
| 10 | `WliteResult.Busy` | Database locked |
| 11 | `WliteResult.TransactionError` | Transaction error |

### Custom error handling

```csharp
using Wlite;

T SafeExecute<T>(Func<T> func, T defaultValue = default)
{
    try
    {
        return func();
    }
    catch (WliteException ex)
    {
        Console.WriteLine($"wlite error: {ex.Result} - {ex.Message}");
        return defaultValue;
    }
}

void ErrorHandlingExample()
{
    var db = SafeExecute(() => Database.Open("app.db"));
    if (db == null)
    {
        Console.WriteLine("Failed to open database");
        return;
    }

    using (db)
    {
        // Use the database...
    }
}
```

## IDisposable

All types implement `IDisposable`. Use `using` statements for automatic cleanup:

```csharp
using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT * FROM users");
// All resources freed automatically at end of scope
```

### Resource lifecycle

```csharp
void ProcessDatabase(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

    db.Migrate(model);

    // Use the database...

} // db and model are disposed here
```

### Preventing resource leaks

```csharp
// Good: using statement ensures disposal
void GoodExample()
{
    using var db = Database.Open("app.db");
    // Use db...
}

// Bad: manual disposal is error-prone
void BadExample()
{
    var db = Database.Open("app.db");
    try
    {
        // Use db...
    }
    finally
    {
        db?.Dispose();
    }
}
```

## Thread safety

Models are immutable after loading and can be shared via `System.Threading.Tasks.Task` or `Thread`. Database connections are not thread-safe; use one per thread.

```csharp
using Wlite;
using System.Threading.Tasks;

void ThreadSafetyExample()
{
    var model = Model.Load("app.wlite");

    var tasks = new Task[4];
    for (int i = 0; i < 4; i++)
    {
        int workerId = i;
        tasks[i] = Task.Run(() =>
        {
            using var db = Database.Open("app.db");
            db.Migrate(model);

            for (int j = 0; j < 100; j++)
            {
                db.Execute(
                    "INSERT INTO work_items (thread_id, data) VALUES (?, ?)",
                    workerId, $"item_{j}"
                );
            }
        });
    }

    Task.WaitAll(tasks);
    model.Dispose();
}
```

### Connection pool pattern

```csharp
using Wlite;
using System.Collections.Concurrent;

class ConnectionPool : IDisposable
{
    private readonly ConcurrentBag<Database> _connections = new();
    private readonly Model _model;

    public ConnectionPool(string modelPath, string dbPath, int poolSize)
    {
        _model = Model.Load(modelPath);

        for (int i = 0; i < poolSize; i++)
        {
            var db = Database.Open(dbPath);
            db.Migrate(_model);
            _connections.Add(db);
        }
    }

    public Database Get() => _connections.TryTake(out var db) ? db : null;

    public void Return(Database db) => _connections.Add(db);

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

## Complete example

Here is a complete, working program that demonstrates all major features:

```csharp
using Wlite;
using System;
using System.Collections.Generic;

public class User
{
    public long Id { get; set; }
    public string Name { get; set; }
    public string Email { get; set; }
    public bool Active { get; set; }
}

public class UserDatabase : IDisposable
{
    private readonly Model _model;
    private readonly Database _db;

    public UserDatabase(string modelPath, string dbPath)
    {
        _model = Model.Load(modelPath);
        _db = Database.Open(dbPath);
        _db.Migrate(_model);
    }

    public void Dispose()
    {
        _db?.Dispose();
        _model?.Dispose();
    }

    public void CreateTables()
    {
        _db.Execute(@"
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                active INTEGER DEFAULT 1,
                created_at TEXT DEFAULT (datetime('now'))
            )
        ");
    }

    public void InsertUser(string name, string email)
    {
        using var stmt = _db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
    }

    public void InsertUsers(IEnumerable<(string Name, string Email)> users)
    {
        using var stmt = _db.Prepare("INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)");
        foreach (var (name, email) in users)
        {
            stmt.Bind(1, name);
            stmt.Bind(2, email);
            stmt.Step();
            stmt.Reset();
        }
    }

    public User GetUser(long id)
    {
        using var stmt = _db.Prepare("SELECT id, name, email, active FROM users WHERE id = ?");
        stmt.Bind(1, id);

        if (stmt.Step())
        {
            return new User
            {
                Id = stmt.ColumnInt64(0),
                Name = stmt.ColumnText(1),
                Email = stmt.ColumnText(2),
                Active = stmt.ColumnInt64(3) != 0,
            };
        }

        return null;
    }

    public List<User> SearchUsers(string pattern)
    {
        using var stmt = _db.Prepare(
            "SELECT id, name, email, active FROM users WHERE name LIKE ?"
        );
        stmt.Bind(1, $"%{pattern}%");

        var results = new List<User>();
        while (stmt.Step())
        {
            results.Add(new User
            {
                Id = stmt.ColumnInt64(0),
                Name = stmt.ColumnText(1),
                Email = stmt.ColumnText(2),
                Active = stmt.ColumnInt64(3) != 0,
            });
        }

        return results;
    }

    public List<User> ListUsers()
    {
        using var stmt = _db.Prepare(
            "SELECT id, name, email, active FROM users ORDER BY name"
        );

        var users = new List<User>();
        while (stmt.Step())
        {
            users.Add(new User
            {
                Id = stmt.ColumnInt64(0),
                Name = stmt.ColumnText(1),
                Email = stmt.ColumnText(2),
                Active = stmt.ColumnInt64(3) != 0,
            });
        }

        return users;
    }

    public long CountUsers()
    {
        return _db.QueryScalar("SELECT COUNT(*) FROM users");
    }

    public void BatchTransfer(IEnumerable<(long From, long To, long Amount)> transfers)
    {
        using var tx = _db.Begin();
        bool committed = false;

        try
        {
            foreach (var (from, to, amount) in transfers)
            {
                _db.Execute(
                    "UPDATE accounts SET balance = balance - ? WHERE id = ?",
                    amount, from
                );
                _db.Execute(
                    "UPDATE accounts SET balance = balance + ? WHERE id = ?",
                    amount, to
                );

                var balance = _db.QueryScalar(
                    "SELECT balance FROM accounts WHERE id = ?", from
                );
                if (balance < 0)
                {
                    throw new InvalidOperationException(
                        $"Insufficient funds for account {from}"
                    );
                }
            }

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
    }
}

class Program
{
    static void Main()
    {
        using var db = new UserDatabase("app.wlite", "app.db");

        db.CreateTables();

        var users = new (string Name, string Email)[]
        {
            ("Alice", "alice@example.com"),
            ("Bob", "bob@example.com"),
            ("Charlie", "charlie@example.com"),
        };
        db.InsertUsers(users);

        Console.WriteLine($"Total users: {db.CountUsers()}");
        Console.WriteLine();

        Console.WriteLine("All users:");
        foreach (var user in db.ListUsers())
        {
            var active = user.Active ? "" : " [inactive]";
            Console.WriteLine($"  {user.Id}: {user.Name} <{user.Email}>{active}");
        }

        Console.WriteLine();
        Console.WriteLine("Search results for 'Ali':");
        foreach (var user in db.SearchUsers("Ali"))
        {
            Console.WriteLine($"  {user.Name}");
        }

        var alice = db.GetUser(1);
        if (alice != null)
        {
            Console.WriteLine();
            Console.WriteLine($"Got user: {alice.Name}");
        }
    }
}
```
