---
title: C# Queries
description: Prepared statements, parameter binding, column access, records, transactions, and savepoints for the C# binding.
---

# C# Queries

The wlite C# binding provides prepared statements for executing SQL with parameters, reading result columns, and managing transactions. This guide covers the full query lifecycle from preparation through execution and result retrieval.

## Preparing statements

The `Database.Prepare` method compiles an SQL statement and returns a `Statement` object. Prepared statements can be executed multiple times with different parameters.

### Basic preparation

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT * FROM users WHERE id = ?");
```

### Preparation with validation

If the SQL is malformed or references nonexistent tables, `Prepare` throws a `WliteException`.

```csharp
using Wlite;

using var db = Database.Open("app.db");

try
{
    using var stmt = db.Prepare("SELECT * FROM nonexistent_table");
}
catch (WliteException ex)
{
    Console.WriteLine($"Prepare failed: {ex.Result} - {ex.Message}");
}
```

### Preparing multiple statements

You can prepare multiple statements from the same database connection. Each statement is independent and holds its own state.

```csharp
using Wlite;

using var db = Database.Open("app.db");

using var insertStmt = db.Prepare(
    "INSERT INTO users (name, email) VALUES (?, ?)"
);
using var selectStmt = db.Prepare(
    "SELECT id, name, email FROM users WHERE email = ?"
);
using var deleteStmt = db.Prepare(
    "DELETE FROM users WHERE id = ?"
);
```

## Binding parameters

The `Statement.Bind` method sets parameter values for prepared statements. Parameters are 1-indexed. There are overloads for `long`, `double`, `string`, and null values.

### Binding integers

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("INSERT INTO users (id, name) VALUES (?, ?)");

stmt.Bind(1, 42L);
stmt.Bind(2, "Alice");
stmt.Step();
stmt.Reset();
```

### Binding doubles

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare(
    "INSERT INTO measurements (sensor_id, value) VALUES (?, ?)"
);

stmt.Bind(1, 1L);
stmt.Bind(2, 3.14159);
stmt.Step();
stmt.Reset();
```

### Binding strings

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");

stmt.Bind(1, "Alice");
stmt.Bind(2, "alice@example.com");
stmt.Step();
```

### Binding null values

Use the parameterless `Bind` overload to set a parameter to null.

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare(
    "INSERT INTO users (name, email, phone) VALUES (?, ?, ?)"
);

stmt.Bind(1, "Alice");
stmt.Bind(2, "alice@example.com");
stmt.Bind(3); // null
stmt.Step();
```

### Rebinding parameters

After calling `Reset`, you can rebind parameters for the next execution. This is the standard pattern for batch operations.

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)");

var users = new (string Name, string Email)[]
{
    ("Alice", "alice@example.com"),
    ("Bob", "bob@example.com"),
    ("Charlie", "charlie@example.com"),
};

foreach (var (name, email) in users)
{
    stmt.Bind(1, name);
    stmt.Bind(2, email);
    stmt.Step();
    stmt.Reset();
}
```

## Stepping through results

The `Statement.Step` method advances to the next row in a result set. It returns `true` if a row is available and `false` when there are no more rows.

### Reading a single row

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT id, name FROM users WHERE id = ?");
stmt.Bind(1, 1L);

if (stmt.Step())
{
    var id = stmt.ColumnInt64(0);
    var name = stmt.ColumnText(1);
    Console.WriteLine($"User {id}: {name}");
}
else
{
    Console.WriteLine("No user found");
}
```

### Reading all rows

Use a while loop to iterate through all result rows.

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT id, name, email FROM users ORDER BY name");

while (stmt.Step())
{
    var id = stmt.ColumnInt64(0);
    var name = stmt.ColumnText(1);
    var email = stmt.ColumnText(2);
    Console.WriteLine($"{id}: {name} <{email}>");
}
```

### Step returns for DML statements

For INSERT, UPDATE, and DELETE statements, `Step` returns `true` once to indicate the statement executed. There are no result rows.

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("DELETE FROM users WHERE id = ?");
stmt.Bind(1, 42L);

if (stmt.Step())
{
    Console.WriteLine("Delete executed");
}
```

## Column access

After `Step` returns `true`, you can read column values using typed accessor methods. Columns are 0-indexed.

### Column accessor methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `ColumnCount()` | `int` | Number of columns in the result |
| `ColumnName(i)` | `string` | Name of the column at index |
| `ColumnType(i)` | `ValueType` | Data type of the column |
| `ColumnInt64(i)` | `long` | Integer value at column index |
| `ColumnDouble(i)` | `double` | Floating point value at column index |
| `ColumnText(i)` | `string` | Text value at column index |

### Reading column names

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare("SELECT id, name, email FROM users");

int colCount = stmt.ColumnCount();
Console.WriteLine($"Columns: {colCount}");

for (int i = 0; i < colCount; i++)
{
    var name = stmt.ColumnName(i);
    var type = stmt.ColumnType(i);
    Console.WriteLine($"  {name}: {type}");
}
```

### Reading different data types

```csharp
using Wlite;

using var db = Database.Open("app.db");
using var stmt = db.Prepare(
    "SELECT id, name, score, active, avatar FROM users WHERE id = ?"
);
stmt.Bind(1, 1L);

if (stmt.Step())
{
    var id = stmt.ColumnInt64(0);        // INTEGER
    var name = stmt.ColumnText(1);        // TEXT
    var score = stmt.ColumnDouble(2);     // REAL
    var active = stmt.ColumnInt64(3);     // INTEGER (boolean)
    var avatar = stmt.ColumnText(4);      // TEXT (or blob as text)

    Console.WriteLine($"ID: {id}");
    Console.WriteLine($"Name: {name}");
    Console.WriteLine($"Score: {score}");
    Console.WriteLine($"Active: {active != 0}");
}
```

### Checking column types dynamically

```csharp
using Wlite;

void PrintRow(Statement stmt)
{
    int colCount = stmt.ColumnCount();
    for (int i = 0; i < colCount; i++)
    {
        var name = stmt.ColumnName(i);
        var type = stmt.ColumnType(i);

        string value = type switch
        {
            ValueType.Integer => stmt.ColumnInt64(i).ToString(),
            ValueType.Float => stmt.ColumnDouble(i).ToString(),
            ValueType.Text => stmt.ColumnText(i) ?? "(null)",
            ValueType.Null => "(null)",
            _ => "(unsupported)"
        };

        Console.Write($"{name}={value}");
        if (i < colCount - 1) Console.Write(", ");
    }
    Console.WriteLine();
}
```

### Reading all columns as a dictionary

```csharp
using Wlite;
using System.Collections.Generic;

List<Dictionary<string, object>> ReadAllRows(Statement stmt)
{
    var rows = new List<Dictionary<string, object>>();

    while (stmt.Step())
    {
        var row = new Dictionary<string, object>();
        int colCount = stmt.ColumnCount();

        for (int i = 0; i < colCount; i++)
        {
            var name = stmt.ColumnName(i);
            var type = stmt.ColumnType(i);

            row[name] = type switch
            {
                ValueType.Integer => stmt.ColumnInt64(i),
                ValueType.Float => stmt.ColumnDouble(i),
                ValueType.Text => (object)(stmt.ColumnText(i) ?? ""),
                ValueType.Null => null,
                _ => null
            };
        }

        rows.Add(row);
    }

    return rows;
}
```

## Records

You can map query results to C# record types for strongly typed data access.

### Simple record mapping

```csharp
using Wlite;

public record UserRecord(long Id, string Name, string Email);

UserRecord? GetUser(Database db, long id)
{
    using var stmt = db.Prepare("SELECT id, name, email FROM users WHERE id = ?");
    stmt.Bind(1, id);

    if (stmt.Step())
    {
        return new UserRecord(
            stmt.ColumnInt64(0),
            stmt.ColumnText(1),
            stmt.ColumnText(2)
        );
    }

    return null;
}
```

### Collecting records

```csharp
using Wlite;
using System.Collections.Generic;

public record ProductRecord(long Id, string Name, double Price);

List<ProductRecord> ListProducts(Database db)
{
    using var stmt = db.Prepare(
        "SELECT id, name, price FROM products ORDER BY name"
    );

    var products = new List<ProductRecord>();
    while (stmt.Step())
    {
        products.Add(new ProductRecord(
            stmt.ColumnInt64(0),
            stmt.ColumnText(1),
            stmt.ColumnDouble(2)
        ));
    }

    return products;
}
```

### Record with computed fields

```csharp
using Wlite;

public record OrderSummary(
    long OrderId,
    string CustomerName,
    int ItemCount,
    double TotalAmount
);

List<OrderSummary> GetOrderSummaries(Database db)
{
    using var stmt = db.Prepare(@"
        SELECT o.id, c.name, COUNT(oi.id), SUM(oi.quantity * oi.price)
        FROM orders o
        JOIN customers c ON o.customer_id = c.id
        JOIN order_items oi ON oi.order_id = o.id
        GROUP BY o.id
        ORDER BY o.created_at DESC
    ");

    var summaries = new List<OrderSummary>();
    while (stmt.Step())
    {
        summaries.Add(new OrderSummary(
            stmt.ColumnInt64(0),
            stmt.ColumnText(1),
            (int)stmt.ColumnInt64(2),
            stmt.ColumnDouble(3)
        ));
    }

    return summaries;
}
```

### Streaming records with yield

```csharp
using Wlite;
using System.Collections.Generic;

IEnumerable<UserRecord> StreamUsers(Database db, string pattern)
{
    using var stmt = db.Prepare(
        "SELECT id, name, email FROM users WHERE name LIKE ?"
    );
    stmt.Bind(1, $"%{pattern}%");

    while (stmt.Step())
    {
        yield return new UserRecord(
            stmt.ColumnInt64(0),
            stmt.ColumnText(1),
            stmt.ColumnText(2)
        );
    }
}
```

## Transactions

Transactions group multiple operations into a single atomic unit. Either all operations succeed and the changes are committed, or any failure rolls back all changes.

### Basic transaction

```csharp
using Wlite;

using var db = Database.Open("app.db");

using var tx = db.Begin();
try
{
    db.Execute("INSERT INTO users (name) VALUES ('Alice')");
    db.Execute("INSERT INTO users (name) VALUES ('Bob')");
    tx.Commit();
}
catch
{
    tx.Rollback();
    throw;
}
```

### Transaction with automatic rollback

The safest pattern uses a boolean flag to track whether the transaction was committed. If an exception occurs, the transaction is rolled back in the finally block.

```csharp
using Wlite;

void SafeTransaction(Database db)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
        db.Execute("INSERT INTO users (name) VALUES ('Alice')");
        db.Execute("INSERT INTO users (name) VALUES ('Bob')");

        var count = db.QueryScalar("SELECT COUNT(*) FROM users");
        if (count > 100)
        {
            throw new InvalidOperationException("Too many users");
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

### Transaction with prepared statements

You can use prepared statements inside a transaction for batch operations.

```csharp
using Wlite;

void BatchInsert(Database db, IEnumerable<(string Name, string Email)> users)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
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

### Transaction with error recovery

When a transaction fails, you can catch the exception, roll back, and retry.

```csharp
using Wlite;

void RetryTransaction(Database db, int maxRetries = 3)
{
    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        using var tx = db.Begin();
        bool committed = false;

        try
        {
            db.Execute("INSERT INTO work_queue (status) VALUES ('pending')");

            var count = db.QueryScalar(
                "SELECT COUNT(*) FROM work_queue WHERE status = 'pending'"
            );
            if (count > 1000)
            {
                throw new InvalidOperationException("Queue full");
            }

            tx.Commit();
            committed = true;
            return;
        }
        catch (WliteException ex) when (ex.Result == WliteResult.Busy)
        {
            Console.WriteLine($"Attempt {attempt}: Database busy, retrying...");
            System.Threading.Thread.Sleep(100 * attempt);
        }
        catch (Exception)
        {
            if (!committed)
            {
                tx.Rollback();
            }
            throw;
        }
    }

    throw new InvalidOperationException("Max retries exceeded");
}
```

## Savepoints

Savepoints allow you to create partial rollback points within a transaction. If an operation fails, you can roll back to the savepoint without discarding the entire transaction.

### Basic savepoint pattern

```csharp
using Wlite;

void ProcessWithSavepoint(Database db)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
        // First operation
        db.Execute("INSERT INTO users (name) VALUES ('Alice')");

        // Create savepoint
        db.Execute("SAVEPOINT sp1");

        try
        {
            // Second operation that might fail
            db.Execute("INSERT INTO users (name) VALUES ('Bob')");
            db.Execute("INSERT INTO invalid_table (col) VALUES ('test')");
        }
        catch
        {
            // Rollback to savepoint, keep first operation
            db.Execute("ROLLBACK TO SAVEPOINT sp1");
        }

        // Third operation
        db.Execute("INSERT INTO users (name) VALUES ('Charlie')");

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

### Multiple savepoints

You can nest savepoints for fine-grained rollback control.

```csharp
using Wlite;

void NestedSavepoints(Database db)
{
    using var tx = db.Begin();
    bool committed = false;

    try
    {
        db.Execute("INSERT INTO users (name) VALUES ('Alice')");
        db.Execute("SAVEPOINT sp1");

        db.Execute("INSERT INTO users (name) VALUES ('Bob')");
        db.Execute("SAVEPOINT sp2");

        try
        {
            db.Execute("INSERT INTO users (name) VALUES ('Charlie')");
            db.Execute("INSERT INTO users (name) VALUES ('Dave')");
        }
        catch
        {
            db.Execute("ROLLBACK TO SAVEPOINT sp2");
        }

        // Bob is still inserted
        db.Execute("RELEASE SAVEPOINT sp2");

        // Release sp1, both Alice and Bob remain
        db.Execute("RELEASE SAVEPOINT sp1");

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

### Savepoint with batch processing

Use savepoints to isolate failures in batch operations. Successful items are committed, failed items are rolled back individually.

```csharp
using Wlite;
using System.Collections.Generic;

void BatchWithSavepoints(
    Database db,
    IEnumerable<(string Name, string Email)> users
)
{
    using var tx = db.Begin();
    bool committed = false;
    int succeeded = 0;
    int failed = 0;

    try
    {
        using var stmt = db.Prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );

        foreach (var (name, email) in users)
        {
            db.Execute("SAVEPOINT sp_batch");

            try
            {
                stmt.Bind(1, name);
                stmt.Bind(2, email);
                stmt.Step();
                stmt.Reset();
                db.Execute("RELEASE SAVEPOINT sp_batch");
                succeeded++;
            }
            catch
            {
                db.Execute("ROLLBACK TO SAVEPOINT sp_batch");
                failed++;
            }
        }

        tx.Commit();
        committed = true;
        Console.WriteLine($"Inserted {succeeded}, skipped {failed}");
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

## Complete query example

This example demonstrates the full query lifecycle with all features.

```csharp
using Wlite;
using System;
using System.Collections.Generic;

public record User(long Id, string Name, string Email, bool Active);

public class UserQueries : IDisposable
{
    private readonly Database _db;

    public UserQueries(string dbPath)
    {
        _db = Database.Open(dbPath);
    }

    public void Dispose()
    {
        _db?.Dispose();
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
        using var tx = _db.Begin();
        bool committed = false;

        try
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

            tx.Commit();
            committed = true;
        }
        finally
        {
            if (!committed) tx.Rollback();
        }
    }

    public User? GetUser(long id)
    {
        using var stmt = _db.Prepare(
            "SELECT id, name, email, active FROM users WHERE id = ?"
        );
        stmt.Bind(1, id);

        if (stmt.Step())
        {
            return new User(
                stmt.ColumnInt64(0),
                stmt.ColumnText(1),
                stmt.ColumnText(2),
                stmt.ColumnInt64(3) != 0
            );
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
            results.Add(new User(
                stmt.ColumnInt64(0),
                stmt.ColumnText(1),
                stmt.ColumnText(2),
                stmt.ColumnInt64(3) != 0
            ));
        }

        return results;
    }

    public long CountUsers()
    {
        return _db.QueryScalar("SELECT COUNT(*) FROM users");
    }

    public void TransferEmail(long fromId, long toId)
    {
        using var tx = _db.Begin();
        bool committed = false;

        try
        {
            using var getStmt = _db.Prepare(
                "SELECT email FROM users WHERE id = ?"
            );
            getStmt.Bind(1, fromId);

            if (!getStmt.Step())
            {
                throw new InvalidOperationException($"User {fromId} not found");
            }

            var email = getStmt.ColumnText(0);

            using var updateStmt = _db.Prepare(
                "UPDATE users SET email = ? WHERE id = ?"
            );
            updateStmt.Bind(1, email);
            updateStmt.Bind(2, toId);
            updateStmt.Step();

            using var deleteStmt = _db.Prepare(
                "UPDATE users SET email = NULL WHERE id = ?"
            );
            deleteStmt.Bind(1, fromId);
            deleteStmt.Step();

            tx.Commit();
            committed = true;
        }
        finally
        {
            if (!committed) tx.Rollback();
        }
    }
}

class Program
{
    static void Main()
    {
        using var queries = new UserQueries("app.db");

        queries.InsertUsers(new[]
        {
            ("Alice", "alice@example.com"),
            ("Bob", "bob@example.com"),
            ("Charlie", "charlie@example.com"),
        });

        Console.WriteLine($"Total users: {queries.CountUsers()}");
        Console.WriteLine();

        var alice = queries.GetUser(1);
        if (alice != null)
        {
            Console.WriteLine($"Got user: {alice.Name} <{alice.Email}>");
        }

        Console.WriteLine();
        Console.WriteLine("Search results for 'Ali':");
        foreach (var user in queries.SearchUsers("Ali"))
        {
            Console.WriteLine($"  {user.Name}");
        }
    }
}
```
