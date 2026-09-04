---
title: C# Binding
description: C# P/Invoke binding for wlite.
---

# C# Binding

The `wlite` NuGet package provides C# access to libwlite via P/Invoke. Define your schema in `.wlite` and use it from .NET.

## Installation

```bash
dotnet add package wlite
```

Requires libwlite to be installed on your system.

## Basic usage

```csharp
using Wlite;

using var model = Model.Load("app.wlite");
using var db = Database.Open("app.db");

db.Migrate(model);

using var stmt = db.Prepare("SELECT * FROM users");
while (stmt.Step())
{
    var name = stmt.ColumnText(0);
    Console.WriteLine(name);
}
```

## Types

| C# Type | C Equivalent | Description |
|---------|--------------|-------------|
| `Wlite.Database` | `wlite_db` | Open database connection |
| `Wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `Wlite.Statement` | `wlite_stmt` | Prepared SQL statement |
| `Wlite.Transaction` | `wlite_tx` | Active transaction |

## Database operations

```csharp
using var db = Database.Open("app.db");

// Execute DDL/DML
db.Execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
db.Execute("INSERT INTO test (name) VALUES ('hello')");

// Prepare and query
using var stmt = db.Prepare("SELECT * FROM test WHERE id = ?");
stmt.Bind(1, 1L);
while (stmt.Step())
{
    var name = stmt.ColumnText(0);
    var id = stmt.ColumnInt64(0);
    Console.WriteLine($"{id}: {name}");
}
```

## Prepared statements

```csharp
using var stmt = db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");
stmt.Bind(1, "Alice");
stmt.Bind(2, "alice@example.com");
stmt.Step();

// Reset and reuse
stmt.Reset();
stmt.Bind(1, "Bob");
stmt.Bind(2, "bob@example.com");
stmt.Step();
```

## Transactions

```csharp
using var tx = db.Begin();
db.Execute("INSERT INTO users (name) VALUES ('Alice')");
db.Execute("INSERT INTO users (name) VALUES ('Bob')");
tx.Commit();
// Rollback on exception: tx.Dispose() without calling Commit()
```

## Error handling

All operations throw `WliteException` on failure:

```csharp
try
{
    var db = Database.Open("app.db");
}
catch (WliteException ex)
{
    Console.WriteLine($"Error: {ex.Result} - {ex.Message}");
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

## Thread safety

Models are immutable after loading and can be shared via `System.Threading.Tasks.Task` or `Thread`. Database connections are not thread-safe; use one per thread.
