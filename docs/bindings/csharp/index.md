---
title: C# Binding Overview
description: C# P/Invoke binding for wlite. Installation, quick start, types, and IDisposable patterns.
---

# C# Binding Overview

The `wlite` NuGet package provides C# access to libwlite via P/Invoke. Define your schema in `.wlite` files and use it from .NET applications. The binding wraps the libwlite C library and exposes it through C# types with proper resource management using `IDisposable`, exception-based error handling, and idiomatic .NET conventions.

## Prerequisites

Before installing the NuGet package, ensure the following dependencies are available on your system.

- .NET SDK 6.0 or later
- libwlite installed from source
- SQLite3 development libraries

### Installing libwlite

Build and install libwlite from the repository:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

Verify the installation:

```bash
pkg-config --libs wlite
```

### Platform requirements

The binding uses P/Invoke to call into the native libwlite shared library. The library must be discoverable at runtime through the standard library search path.

| Platform | Library file | Location |
|----------|-------------|----------|
| Linux | `libwlite.so` | `/usr/local/lib` |
| macOS | `libwlite.dylib` | `/usr/local/lib` |
| Windows | `wlite.dll` | PATH or application directory |

## Installation

Add the wlite NuGet package to your project:

```bash
dotnet add package wlite
```

Or add the package reference directly to your project file:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net8.0</TargetFramework>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="wlite" Version="0.1.*" />
  </ItemGroup>
</Project>
```

### Verifying the installation

```csharp
using Wlite;

var version = WliteVersion.Get();
Console.WriteLine($"wlite version: {version}");
```

## Quick start

The fastest way to get started is to load a model, open a database, run migrations, and execute queries. This section walks through a minimal example.

### Create a model file

Create a file named `app.wlite` in your project directory:

```
table users {
  id integer primary key
  name text not null
  email text not null unique
  created_at text default (datetime('now'))
}

table posts {
  id integer primary key
  user_id integer not null references users(id)
  title text not null
  body text
  published_at text
}
```

### Full example

```csharp
using Wlite;

class Program
{
    static void Main()
    {
        using var model = Model.Load("app.wlite");
        using var db = Database.Open("app.db");

        db.Migrate(model);

        using var insertStmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );

        insertStmt.Bind(1, "Alice");
        insertStmt.Bind(2, "alice@example.com");
        insertStmt.Step();
        insertStmt.Reset();

        insertStmt.Bind(1, "Bob");
        insertStmt.Bind(2, "bob@example.com");
        insertStmt.Step();

        using var queryStmt = db.Prepare("SELECT id, name, email FROM users");
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

### What happens in this example

1. `Model.Load` reads and parses the `.wlite` schema file
2. `Database.Open` creates or opens an SQLite database at the given path
3. `db.Migrate` applies the schema to the database, creating or altering tables as needed
4. `db.Prepare` compiles an SQL statement for reuse
5. `Bind` sets parameter values, `Step` executes the statement, and `Reset` rewinds it for the next iteration
6. Column access methods retrieve result data from the current row

## Types

The binding exposes five core types in the `Wlite` namespace.

| C# Type | C Equivalent | Description |
|---------|--------------|-------------|
| `Wlite.Database` | `wlite_db` | Open database connection |
| `Wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `Wlite.Statement` | `wlite_stmt` | Prepared SQL statement |
| `Wlite.Transaction` | `wlite_tx` | Active database transaction |
| `Wlite.WliteException` | `wlite_result` | Exception type for errors |

### Database

The `Database` type manages a connection to an SQLite database. It provides methods for executing SQL, preparing statements, running migrations, and starting transactions.

```csharp
using var db = Database.Open("app.db");
```

The `Database` constructor is private. Use the static `Database.Open` method to create instances. The `Open` method throws `WliteException` if the database cannot be opened.

Key methods on `Database`:

- `Execute(string sql)` runs a single SQL statement with no result rows
- `Prepare(string sql)` returns a `Statement` for parameterized queries
- `Migrate(Model model)` applies a schema to the database
- `Begin()` starts a new `Transaction`
- `Query(string sql)` returns a `Statement` for reading results
- `QueryScalar(string sql)` returns a single value from a query

### Model

The `Model` type represents a parsed `.wlite` schema file. It is immutable after loading and can be used to migrate multiple databases.

```csharp
using var model = Model.Load("app.wlite");
```

The `Model` type is created via the static `Model.Load` method. It holds a pointer to the native model handle and frees it on disposal. Models can be shared across threads since they are immutable after loading.

### Statement

The `Statement` type wraps a prepared SQL statement. It supports parameter binding, step-by-step execution, and column access.

```csharp
using var stmt = db.Prepare("SELECT * FROM users WHERE id = ?");
stmt.Bind(1, 42);
while (stmt.Step())
{
    var name = stmt.ColumnText(1);
}
```

Statements are created by `Database.Prepare` or `Database.Query`. They are not thread-safe and should be used on a single thread.

### Transaction

The `Transaction` type represents an active database transaction. It must be committed or rolled back before disposal.

```csharp
using var tx = db.Begin();
// ... perform operations ...
tx.Commit();
```

Transactions are created by `Database.Begin`. If a transaction is disposed without being committed or rolled back, it is rolled back automatically.

### WliteException

The `WliteException` type is thrown when any wlite operation fails. It carries a `Result` property of type `WliteResult` indicating the error code.

```csharp
try
{
    var db = Database.Open("/nonexistent/path/db.db");
}
catch (WliteException ex)
{
    Console.WriteLine($"Error: {ex.Result} - {ex.Message}");
}
```

## IDisposable pattern

All wlite types implement `IDisposable`. This ensures native resources are freed deterministically when you are done with them. The recommended pattern is to use `using` statements.

### Using declarations

```csharp
using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT * FROM users");
// All resources freed automatically at end of scope
```

### Using blocks

```csharp
using (var model = Model.Load("app.wlite"))
using (var db = Database.Open("app.db"))
{
    db.Migrate(model);
    // Use the database
}
// Both model and db are disposed here
```

### Finalizer safety net

Each type includes a finalizer that calls `Dispose`. This ensures native memory is freed even if you forget to dispose manually. However, relying on finalizers is not recommended because:

1. Finalizers run on the garbage collector thread at an unpredictable time
2. Finalized objects require multiple garbage collection cycles to reclaim
3. Native memory leaks can occur between the time you stop using an object and when the finalizer runs

Always use `using` statements for deterministic cleanup.

### Preventing double disposal

Each type tracks whether it has been disposed. Calling `Dispose` more than once is safe and has no effect. The `Dispose` method uses a `_disposed` flag to prevent double-free of native handles.

```csharp
var db = Database.Open("app.db");
db.Dispose();
db.Dispose(); // Safe, no effect
```

### Resource lifecycle

The order of disposal matters. Always dispose child resources before parent resources. In practice, this means disposing statements before the database, and the database before the model.

```csharp
using var stmt = db.Prepare("SELECT 1");
// stmt is disposed first

using var db = Database.Open("app.db");
// db is disposed second

using var model = Model.Load("app.wlite");
// model is disposed last
```

With `using` declarations, disposal order follows reverse declaration order. Declare resources in the order they depend on each other.

### Complete lifecycle example

```csharp
using Wlite;

void ProcessData(string modelPath, string dbPath)
{
    using var model = Model.Load(modelPath);
    using var db = Database.Open(dbPath);

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
        stmt.Reset();

        stmt.Bind(1, "Bob");
        stmt.Bind(2, "bob@example.com");
        stmt.Step();

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
// stmt disposed, then tx, then db, then model
```

## Thread safety

Models are immutable after loading and can be shared across threads safely. Database connections are not thread-safe. Each thread should use its own `Database` instance.

```csharp
using Wlite;
using System.Threading.Tasks;

void ConcurrentWork()
{
    using var model = Model.Load("app.wlite");

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
}
```

## Complete example

Here is a complete program demonstrating all major features:

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
        using var stmt = _db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.Bind(1, name);
        stmt.Bind(2, email);
        stmt.Step();
    }

    public void InsertUsers(IEnumerable<(string Name, string Email)> users)
    {
        using var stmt = _db.Prepare(
            "INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)"
        );
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
        using var stmt = _db.Prepare(
            "SELECT id, name, email, active FROM users WHERE id = ?"
        );
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

        var alice = db.GetUser(1);
        if (alice != null)
        {
            Console.WriteLine();
            Console.WriteLine($"Got user: {alice.Name}");
        }
    }
}
```
