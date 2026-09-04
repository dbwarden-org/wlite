---
title: Query Guide
description: Prepared statements, parameter binding, column access, Query, QueryRow, records, transactions, and savepoints in the wlite Go binding.
---

# Query Guide

The Go binding provides a complete query interface built around prepared statements. You prepare SQL once, bind parameters, step through results, and read columns. For convenience, `Query` and `QueryRow` methods handle the prepare/bind/step cycle automatically.

## Prepare

Prepare a SQL statement with `db.Prepare`:

```go
stmt, err := db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()
```

Prepared statements are more efficient than raw SQL for repeated execution. The SQL is parsed and compiled once by SQLite, then reused with different parameters.

### Reusing a prepared statement

```go
stmt, err := db.Prepare("INSERT INTO todos (title, completed) VALUES (?, ?)")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

titles := []string{"Buy milk", "Write docs", "Run tests"}
for _, title := range titles {
    stmt.BindText(1, title)
    stmt.BindInt64(2, 0)
    stmt.Step()
    stmt.Reset()
}
```

After each execution, call `stmt.Reset()` to rewind the statement for the next set of parameters.

## Bind

Bind parameters to a prepared statement before stepping. Parameters are 1-indexed.

### BindInt64

```go
stmt.BindInt64(1, 42)
```

### BindDouble

```go
stmt.BindDouble(1, 3.14159)
```

### BindText

```go
stmt.BindText(1, "hello world")
```

### BindNull

```go
stmt.BindNull(1)
```

### Binding all parameter types

```go
stmt, err := db.Prepare(
    "INSERT INTO events (name, priority, score, metadata) VALUES (?, ?, ?, ?)",
)
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

stmt.BindText(1, "click")
stmt.BindInt64(2, 10)
stmt.BindDouble(3, 0.95)
stmt.BindNull(4)
stmt.Step()
```

## Step

The `Step` method advances the statement to the next row. It returns `true` if a row is available and `false` when there are no more rows or when an error occurs:

```go
stmt, err := db.Prepare("SELECT id, name FROM users")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

for stmt.Step() {
    id := stmt.ColumnInt64(0)
    name := stmt.ColumnText(1)
    fmt.Printf("%d: %s\n", id, name)
}
```

For statements that do not return rows (INSERT, UPDATE, DELETE), call `Step` once:

```go
stmt, err := db.Prepare("INSERT INTO users (name) VALUES (?)")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

stmt.BindText(1, "Alice")
stmt.Step()
```

## Column access

After a successful `Step`, read column values by index. Indices are 0-based.

### ColumnCount

Returns the number of columns in the result set:

```go
count := stmt.ColumnCount()
fmt.Printf("This query returns %d column(s)\n", count)
```

### ColumnName

Returns the name of a column:

```go
for i := 0; i < stmt.ColumnCount(); i++ {
    fmt.Printf("Column %d: %s\n", i, stmt.ColumnName(i))
}
```

### ColumnType

Returns the type of a column as an integer:

| Return value | Meaning |
|--------------|---------|
| 0 | NULL |
| 1 | INTEGER |
| 2 | REAL |
| 3 | TEXT |
| 4 | BLOB |

```go
for i := 0; i < stmt.ColumnCount(); i++ {
    vt := stmt.ColumnType(i)
    name := stmt.ColumnName(i)
    switch vt {
    case 0:
        fmt.Printf("%s: NULL\n", name)
    case 1:
        fmt.Printf("%s: INTEGER = %d\n", name, stmt.ColumnInt64(i))
    case 2:
        fmt.Printf("%s: REAL = %f\n", name, stmt.ColumnDouble(i))
    case 3:
        fmt.Printf("%s: TEXT = %s\n", name, stmt.ColumnText(i))
    default:
        fmt.Printf("%s: BLOB\n", name)
    }
}
```

### ColumnInt64

Returns an integer value:

```go
id := stmt.ColumnInt64(0)
```

### ColumnDouble

Returns a floating-point value:

```go
score := stmt.ColumnDouble(0)
```

### ColumnText

Returns a text value. Returns an empty string if the column is NULL:

```go
name := stmt.ColumnText(0)
```

### Reading all columns from a row

```go
for stmt.Step() {
    for i := 0; i < stmt.ColumnCount(); i++ {
        vt := stmt.ColumnType(i)
        name := stmt.ColumnName(i)
        switch vt {
        case 0:
            fmt.Printf("  %s: NULL", name)
        case 1:
            fmt.Printf("  %s: %d", name, stmt.ColumnInt64(i))
        case 2:
            fmt.Printf("  %s: %f", name, stmt.ColumnDouble(i))
        case 3:
            fmt.Printf("  %s: %s", name, stmt.ColumnText(i))
        }
    }
    fmt.Println()
}
```

## Query

The `Query` method prepares a statement, binds arguments, steps through all rows, and returns them as a slice of maps. This is a convenience method for simple queries:

```go
rows, err := db.Query("SELECT id, name, email FROM users ORDER BY name")
if err != nil {
    log.Fatal(err)
}

for _, row := range rows {
    fmt.Printf("id=%v name=%v email=%v\n", row["id"], row["name"], row["email"])
}
```

### Query with parameters

```go
rows, err := db.Query(
    "SELECT id, name FROM users WHERE name LIKE ?",
    "%Ali%",
)
if err != nil {
    log.Fatal(err)
}

for _, row := range rows {
    fmt.Printf("Found: %v\n", row["name"])
}
```

### Query with multiple parameter types

```go
rows, err := db.Query(
    "SELECT * FROM events WHERE priority > ? AND score >= ?",
    int64(5), 0.8,
)
if err != nil {
    log.Fatal(err)
}
```

The `Query` method automatically binds `int`, `int64`, `float64`, `string`, and `nil` values. Other types are converted to strings with `fmt.Sprintf`.

## QueryRow

The `QueryRow` method is for queries that return a single row. It returns a `*Row` with a `Scan` method:

```go
var name string
err = db.QueryRow("SELECT name FROM users WHERE id = ?", 1).Scan(&name)
if err != nil {
    log.Fatal(err)
}
fmt.Println(name)
```

### QueryRow with multiple columns

```go
var name string
var email string
var age int64
err = db.QueryRow(
    "SELECT name, email, age FROM users WHERE id = ?", 1,
).Scan(&name, &email, &age)
if err != nil {
    log.Fatal(err)
}
fmt.Printf("%s <%s> age=%d\n", name, email, age)
```

### Checking for not-found errors

When `QueryRow` finds no matching row, it returns an error. Check for this specifically:

```go
var count int64
err = db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
if err != nil {
    var wliteErr *wlite.Error
    if errors.As(err, &wliteErr) && wliteErr.Code == wlite.NOT_FOUND {
        fmt.Println("No users found")
    } else {
        log.Fatal(err)
    }
}
```

## Records

The record API provides an alternative way to read query results. A record wraps a snapshot of the current row from a statement:

### Creating a record from a statement

```go
stmt, err := db.Prepare("SELECT id, name, email FROM users")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

for stmt.Step() {
    record, err := stmt.Record()
    if err != nil {
        log.Fatal(err)
    }
    defer record.Free()

    id := record.Int64(0)
    name := record.Text(1)
    email := record.Text(2)
    fmt.Printf("%d: %s <%s>\n", id, name, email)
}
```

### Finding a column by name

Records let you find a column index by name, which is useful when you do not know the column order:

```go
for stmt.Step() {
    record, err := stmt.Record()
    if err != nil {
        log.Fatal(err)
    }
    defer record.Free()

    nameIdx := record.Find("name")
    if nameIdx >= 0 {
        fmt.Println("Name:", record.Text(nameIdx))
    }

    emailIdx := record.Find("email")
    if emailIdx >= 0 {
        fmt.Println("Email:", record.Text(emailIdx))
    }
}
```

### Record column metadata

```go
for stmt.Step() {
    record, err := stmt.Record()
    if err != nil {
        log.Fatal(err)
    }
    defer record.Free()

    count := record.ColumnCount()
    for i := 0; i < count; i++ {
        name := record.ColumnName(i)
        vt := record.ColumnType(i)
        fmt.Printf("Column %d: %s (type %d)\n", i, name, vt)
    }
}
```

## Transactions

Transactions ensure that a group of operations either all succeed or all fail.

### Begin and Commit

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Alice")
if err != nil {
    tx.Rollback()
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Bob")
if err != nil {
    tx.Rollback()
    log.Fatal(err)
}

if err := tx.Commit(); err != nil {
    log.Fatal(err)
}
```

### Begin and Rollback

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Charlie")
if err != nil {
    tx.Rollback()
    log.Fatal(err)
}

tx.Rollback()
```

### Transaction with defer

Use a `committed` flag to ensure the transaction is rolled back if it was not committed:

```go
func processOrders(db *wlite.DB, orders []Order) error {
    tx, err := db.Transaction()
    if err != nil {
        return err
    }

    committed := false
    defer func() {
        if !committed {
            tx.Rollback()
        }
    }}()

    for _, order := range orders {
        _, err := db.Execute(
            "INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)",
            order.UserID, order.ProductID, order.Quantity,
        )
        if err != nil {
            return err
        }
    }

    if err := tx.Commit(); err != nil {
        return err
    }
    committed = true
    return nil
}
```

## Savepoints

Savepoints let you create partial rollback points within a transaction. This is useful when you want to undo a subset of operations without losing the entire transaction.

### Creating a savepoint

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}

err = tx.Savepoint("before_batch")
if err != nil {
    log.Fatal(err)
}
```

### Releasing a savepoint

Once you are past the point of no return, release the savepoint:

```go
err = tx.Release("before_batch")
if err != nil {
    log.Fatal(err)
}
```

### Rolling back to a savepoint

If something goes wrong within the savepoint scope, roll back to it:

```go
tx, err := db.Transaction()
if err != nil {
    log.Fatal(err)
}

err = tx.Savepoint("sp1")
if err != nil {
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Alice")
if err != nil {
    tx.RollbackTo("sp1")
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Bob")
if err != nil {
    tx.RollbackTo("sp1")
    log.Fatal(err)
}

tx.Release("sp1")
tx.Commit()
```

### Nested savepoints

You can nest savepoints for fine-grained rollback control:

```go
tx, _ := db.Transaction()

tx.Savepoint("outer")
db.Execute("INSERT INTO users (name) VALUES (?)", "Alice")

tx.Savepoint("inner")
db.Execute("INSERT INTO users (name) VALUES (?)", "Bob")

tx.RollbackTo("inner")

tx.Savepoint("inner")
db.Execute("INSERT INTO users (name) VALUES (?)", "Charlie")

tx.Release("inner")
tx.Release("outer")
tx.Commit()
```

In this example, Bob's insert is rolled back but Alice and Charlie are committed.

## Batch inserts with transactions

Combining prepared statements with transactions gives the best performance for bulk data loading:

```go
func batchInsert(db *wlite.DB, items []Item) error {
    tx, err := db.Transaction()
    if err != nil {
        return err
    }

    committed := false
    defer func() {
        if !committed {
            tx.Rollback()
        }
    }()

    stmt, err := db.Prepare("INSERT INTO items (name, value) VALUES (?, ?)")
    if err != nil {
        return err
    }
    defer stmt.Close()

    for _, item := range items {
        stmt.BindText(1, item.Name)
        stmt.BindDouble(2, item.Value)
        if err := stmt.StepError(); err != nil {
            return err
        }
        stmt.Reset()
    }

    if err := tx.Commit(); err != nil {
        return err
    }
    committed = true
    return nil
}
```

## Error handling for queries

Always check errors from `Prepare`, `Query`, `QueryRow`, `Step`, and `Scan`:

```go
stmt, err := db.Prepare("SELECT * FROM users WHERE id = ?")
if err != nil {
    var wliteErr *wlite.Error
    if errors.As(err, &wliteErr) {
        switch wliteErr.Code {
        case wlite.SQLITE_ERROR:
            log.Printf("SQL error: %s", wliteErr.Message)
        case wlite.CONSTRAINT_ERROR:
            log.Printf("Constraint violation: %s", wliteErr.Message)
        default:
            log.Printf("wlite error %d: %s", wliteErr.Code, wliteErr.Message)
        }
    } else {
        log.Printf("Unexpected error: %v", err)
    }
    return
}
defer stmt.Close()

stmt.BindInt64(1, 1)
if !stmt.Step() {
    fmt.Println("No rows found")
    return
}

var name string
if err := stmt.ColumnText(0), name); err != nil {
    log.Fatal(err)
}
```

## Resetting a statement

After stepping through all rows or executing a statement, reset it to reuse with new parameters:

```go
stmt, err := db.Prepare("SELECT name FROM users WHERE id = ?")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()

ids := []int64{1, 2, 3}
for _, id := range ids {
    stmt.BindInt64(1, id)
    if stmt.Step() {
        fmt.Println(stmt.ColumnText(0))
    }
    stmt.Reset()
}
```

## Complete query example

The following program demonstrates all major query operations:

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    db, err := wlite.OpenMemory()
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    db.Execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT, active INTEGER)")
    db.Execute("CREATE TABLE todos (id INTEGER PRIMARY KEY, user_id INTEGER, title TEXT, completed INTEGER)")

    db.Execute("INSERT INTO users (name, email, active) VALUES (?, ?, ?)", "Alice", "alice@example.com", 1)
    db.Execute("INSERT INTO users (name, email, active) VALUES (?, ?, ?)", "Bob", "bob@example.com", 1)
    db.Execute("INSERT INTO users (name, email, active) VALUES (?, ?, ?)", "Charlie", "charlie@example.com", 0)

    db.Execute("INSERT INTO todos (user_id, title, completed) VALUES (?, ?, ?)", 1, "Buy milk", 0)
    db.Execute("INSERT INTO todos (user_id, title, completed) VALUES (?, ?, ?)", 1, "Write docs", 1)
    db.Execute("INSERT INTO todos (user_id, title, completed) VALUES (?, ?, ?)", 2, "Run tests", 0)

    fmt.Println("=== Prepared statement ===")
    stmt, err := db.Prepare("SELECT id, name, email FROM users WHERE active = ?")
    if err != nil {
        log.Fatal(err)
    }
    defer stmt.Close()

    stmt.BindInt64(1, 1)
    for stmt.Step() {
        fmt.Printf("  %d: %s <%s>\n", stmt.ColumnInt64(0), stmt.ColumnText(1), stmt.ColumnText(2))
    }

    fmt.Println("\n=== Query ===")
    rows, err := db.Query("SELECT name, title FROM users JOIN todos ON users.id = todos.user_id")
    if err != nil {
        log.Fatal(err)
    }
    for _, row := range rows {
        fmt.Printf("  %s: %s\n", row["name"], row["title"])
    }

    fmt.Println("\n=== QueryRow ===")
    var count int64
    err = db.QueryRow("SELECT COUNT(*) FROM todos WHERE completed = ?", 0).Scan(&count)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("  Incomplete todos: %d\n", count)

    fmt.Println("\n=== Transaction with savepoint ===")
    tx, err := db.Transaction()
    if err != nil {
        log.Fatal(err)
    }

    tx.Savepoint("batch")
    db.Execute("INSERT INTO todos (user_id, title, completed) VALUES (?, ?, ?)", 1, "New task", 0)
    tx.Release("batch")
    tx.Commit()

    fmt.Println("  Transaction committed.")

    fmt.Println("\nDone.")
}
```

## Further reading

- [Index](index.md) for types, installation, and CGO configuration.
- [Migration guide](migration.md) for schema management with `LoadModel`, `Migrate`, `Diff`, `Plan`, and `Snapshot`.
- [Error handling guide](errors.md) for the `Error` type and error constants.
