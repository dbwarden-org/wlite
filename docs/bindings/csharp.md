---
title: C# Binding
description: C# P/Invoke binding for wlite. Use .wlite schemas from .NET.
---

# C# Binding

The `wlite` NuGet package provides C# access to libwlite via P/Invoke. Define your schema in `.wlite` and use it from .NET.

## Installation

```bash
dotnet add package wlite
```

Requires libwlite to be installed on your system.

## Usage

```csharp
using Wlite;

// Load a model
using var model = Model.Load("app.wlite");

// Open a database
using var db = Database.Open("app.db");

// Migrate
db.Migrate(model);

// Query
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

## Prepared Statements

```csharp
using var stmt = db.Prepare("SELECT * FROM users WHERE id = ?");
stmt.Bind(1, 42L);
while (stmt.Step())
{
    Console.WriteLine(stmt.ColumnText(0));
}
```

## Transactions

```csharp
using var tx = db.Begin();
db.Execute("INSERT INTO users (name) VALUES ('Alice')");
tx.Commit();
// Rollback on exception: tx.Dispose() without Commit()
```

## Error Handling

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
// Resources freed automatically at end of scope
```
