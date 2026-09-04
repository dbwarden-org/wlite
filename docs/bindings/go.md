---
title: Go Binding
description: Go cgo binding for wlite.
---

# Go Binding

The Go binding uses cgo to call libwlite's C ABI. It provides Go-idiomatic types and error handling.

The binding wraps the libwlite C library and exposes it through Go types with proper error handling, resource management using `defer`, and idiomatic patterns.

## Installation

```bash
go get github.com/dbwarden-org/wlite/bindings/go
```

Requires libwlite to be installed on your system. Build and install it from the libwlite repository:

```bash
git clone https://github.com/dbwarden-org/wlite.git
cd wlite
make
sudo make install
```

### CGO configuration

When building, you may need to set CGO flags:

```bash
export CGO_ENABLED=1
export CGO_LDFLAGS="-L/usr/local/lib -lwlite -lsqlite3"
export CGO_CFLAGS="-I/usr/local/include"
```

### Using pkg-config

If libwlite provides a pkg-config file:

```bash
export CGO_LDFLAGS="$(pkg-config --libs wlite)"
export CGO_CFLAGS="$(pkg-config --cflags wlite)"
```

## Basic usage

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
    defer model.Free()

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }

    rows, err := db.Query("SELECT * FROM users")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var name string
        var email string
        rows.Scan(&name, &email)
        fmt.Printf("%s <%s>\n", name, email)
    }
}
```

### Working with multiple tables

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
    defer model.Free()

    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        log.Fatal(err)
    }

    // Insert a user
    _, err = db.Execute(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        "Alice", "alice@example.com",
    )
    if err != nil {
        log.Fatal(err)
    }

    // Query users
    rows, err := db.Query("SELECT id, name, email FROM users ORDER BY name")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var id int64
        var name string
        var email string
        if err := rows.Scan(&id, &name, &email); err != nil {
            log.Fatal(err)
        }
        fmt.Printf("%d: %s <%s>\n", id, name, email)
    }

    if err := rows.Err(); err != nil {
        log.Fatal(err)
    }
}
```

## Types

| Go Type | C Equivalent | Description |
|---------|--------------|-------------|
| `wlite.DB` | `wlite_db` | Open database connection |
| `wlite.Model` | `wlite_model` | Loaded .wlite schema |
| `wlite.Stmt` | `wlite_stmt` | Prepared SQL statement |
| `wlite.Tx` | `wlite_tx` | Active transaction |
| `wlite.Rows` | result set | Query result iterator |

The `DB` type manages the connection and provides methods for executing SQL, preparing statements, and querying data.

The `Model` type represents a parsed `.wlite` schema. It is immutable after loading and can be used to migrate multiple databases.

The `Stmt` type wraps a prepared SQL statement with parameter binding and column access methods.

## Database operations

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // Execute DDL
    _, err = db.Execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")
    if err != nil {
        log.Fatal(err)
    }

    _, err = db.Execute("CREATE INDEX idx_test_name ON test(name)")
    if err != nil {
        log.Fatal(err)
    }

    // Execute DML with parameters
    _, err = db.Execute("INSERT INTO test (name) VALUES (?)", "hello")
    if err != nil {
        log.Fatal(err)
    }

    _, err = db.Execute("UPDATE test SET name = ? WHERE id = ?", "world", 1)
    if err != nil {
        log.Fatal(err)
    }

    // Query
    rows, err := db.Query("SELECT * FROM test")
    if err != nil {
        log.Fatal(err)
    }
    defer rows.Close()

    for rows.Next() {
        var id int64
        var name string
        rows.Scan(&id, &name)
        fmt.Printf("%d: %s\n", id, name)
    }

    // Single value query
    var count int64
    err = db.QueryRow("SELECT COUNT(*) FROM test").Scan(&count)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Count: %d\n", count)
}
```

### Batch inserts

```go
func batchInsert(db *wlite.DB, users []User) error {
    stmt, err := db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    if err != nil {
        return err
    }
    defer stmt.Finalize()

    for _, user := range users {
        _, err = stmt.Exec(user.Name, user.Email)
        if err != nil {
            return err
        }
        stmt.Reset()
    }

    return nil
}
```

## Prepared statements

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    db, err := wlite.Open("app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    // Prepare an INSERT statement
    stmt, err := db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    if err != nil {
        log.Fatal(err)
    }
    defer stmt.Finalize()

    // Insert Alice
    stmt.BindText(1, "Alice")
    stmt.BindText(2, "alice@example.com")
    stmt.Step()
    stmt.Reset()

    // Insert Bob
    stmt.BindText(1, "Bob")
    stmt.BindText(2, "bob@example.com")
    stmt.Step()
    stmt.Reset()

    // Insert Charlie
    stmt.BindText(1, "Charlie")
    stmt.BindText(2, "charlie@example.com")
    stmt.Step()

    // Prepare a SELECT statement
    query, err := db.Prepare("SELECT * FROM users WHERE name = ?")
    if err != nil {
        log.Fatal(err)
    }
    defer query.Finalize()

    query.BindText(1, "Alice")
    for query.Step() {
        id := query.ColumnInt64(0)
        name := query.ColumnText(0)
        fmt.Printf("Found user %d: %s\n", id, name)
    }
}
```

### Column access methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `ColumnCount()` | `int` | Number of columns in result |
| `ColumnName(i)` | `string` | Name of column at index |
| `ColumnType(i)` | `ColumnType` | Data type of column |
| `ColumnInt64(i)` | `int64` | Integer value |
| `ColumnDouble(i)` | `float64` | Floating point value |
| `ColumnText(i)` | `string` | Text value |

## Transactions

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func transferFunds(db *wlite.DB, from, to, amount int64) error {
    tx, err := db.Begin()
    if err != nil {
        return err
    }

    // Debit sender
    _, err = db.Execute(
        "UPDATE accounts SET balance = balance - ? WHERE id = ?",
        amount, from,
    )
    if err != nil {
        tx.Rollback()
        return err
    }

    // Credit receiver
    _, err = db.Execute(
        "UPDATE accounts SET balance = balance + ? WHERE id = ?",
        amount, to,
    )
    if err != nil {
        tx.Rollback()
        return err
    }

    // Verify sender has sufficient funds
    var balance int64
    err = db.QueryRow(
        "SELECT balance FROM accounts WHERE id = ?", from,
    ).Scan(&balance)
    if err != nil {
        tx.Rollback()
        return err
    }

    if balance < 0 {
        tx.Rollback()
        return fmt.Errorf("insufficient funds for account %d", from)
    }

    return tx.Commit()
}
```

### Transaction with defer

```go
func batchWithTransaction(db *wlite.DB, orders []Order) error {
    tx, err := db.Begin()
    if err != nil {
        return err
    }

    committed := false
    defer func() {
        if !committed {
            tx.Rollback()
        }
    }()

    stmt, err := db.Prepare(
        "INSERT INTO orders (user_id, product_id, quantity) VALUES (?, ?, ?)",
    )
    if err != nil {
        return err
    }
    defer stmt.Finalize()

    for _, order := range orders {
        stmt.BindInt64(1, order.UserID)
        stmt.BindInt64(2, order.ProductID)
        stmt.BindInt64(3, order.Quantity)
        stmt.Step()
        stmt.Reset()
    }

    if err := tx.Commit(); err != nil {
        return err
    }
    committed = true

    return nil
}
```

## Error handling

All operations return `error` as the second return value:

```go
db, err := wlite.Open("app.db")
if err != nil {
    log.Printf("Failed to open database: %v", err)
    return
}
```

### Error types

```go
package main

import (
    "errors"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func errorHandlingExample() {
    db, err := wlite.Open("app.db")
    if err != nil {
        var wliteErr *wlite.Error
        if errors.As(err, &wliteErr) {
            switch wliteErr.Code {
            case wlite.NOT_FOUND:
                log.Println("Database file not found")
            case wlite.IO:
                log.Println("I/O error:", wliteErr.Message)
            case wlite.CORRUPT:
                log.Println("Database is corrupt")
            default:
                log.Println("wlite error:", err)
            }
        } else {
            log.Println("Non-wlite error:", err)
        }
        return
    }
    defer db.Close()
}
```

## Memory management

Use `defer` to ensure resources are freed:

```go
model, _ := wlite.LoadModel("app.wlite")
defer model.Free()

db, _ := wlite.Open("app.db")
defer db.Close()

stmt, _ := db.Prepare("SELECT * FROM users")
defer stmt.Finalize()
```

### Resource lifecycle

```go
func processDatabase(modelPath, dbPath string) error {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return err
    }
    defer model.Free()

    db, err := wlite.Open(dbPath)
    if err != nil {
        return err
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        return err
    }

    // Use the database...

    return nil
}
```

## Thread safety

Models are immutable after loading and can be shared across goroutines. Database connections are not thread-safe; use one per goroutine.

```go
package main

import (
    "sync"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

func main() {
    model, _ := wlite.LoadModel("app.wlite")
    defer model.Free()

    var wg sync.WaitGroup

    for i := 0; i < 4; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()

            db, err := wlite.Open("app.db")
            if err != nil {
                return
            }
            defer db.Close()

            for j := 0; j < 100; j++ {
                db.Execute(
                    "INSERT INTO work_items (thread_id, data) VALUES (?, ?)",
                    workerID, "item",
                )
            }
        }(i)
    }

    wg.Wait()
}
```

### Connection pool pattern

```go
type ConnectionPool struct {
    connections chan *wlite.DB
}

func NewConnectionPool(size int, modelPath, dbPath string) (*ConnectionPool, error) {
    pool := &ConnectionPool{
        connections: make(chan *wlite.DB, size),
    }

    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return nil, err
    }
    defer model.Free()

    for i := 0; i < size; i++ {
        db, err := wlite.Open(dbPath)
        if err != nil {
            return nil, err
        }
        pool.connections <- db
    }

    return pool, nil
}

func (p *ConnectionPool) Get() *wlite.DB {
    return <-p.connections
}

func (p *ConnectionPool) Put(db *wlite.DB) {
    p.connections <- db
}
```

## CGO configuration

When building, you may need to set CGO flags:

```bash
CGO_ENABLED=1
CGO_LDFLAGS="-L/usr/local/lib -lwlite -lsqlite3"
CGO_CFLAGS="-I/usr/local/include"
```

### Building for different platforms

```bash
# Linux
CGO_ENABLED=1 go build -o myapp .

# macOS
CGO_ENABLED=1 go build -o myapp .

# Cross-compilation (requires toolchain)
CC=x86_64-linux-gnu-gcc CGO_ENABLED=1 GOOS=linux GOARCH=amd64 go build -o myapp .
```

## Complete example

Here is a complete, working program that demonstrates all major features:

```go
package main

import (
    "fmt"
    "log"

    wlite "github.com/dbwarden-org/wlite/bindings/go"
)

type User struct {
    ID     int64
    Name   string
    Email  string
    Active bool
}

type UserDatabase struct {
    model *wlite.Model
    db    *wlite.DB
}

func NewUserDatabase(modelPath, dbPath string) (*UserDatabase, error) {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return nil, fmt.Errorf("loading model: %w", err)
    }

    db, err := wlite.Open(dbPath)
    if err != nil {
        model.Free()
        return nil, fmt.Errorf("opening database: %w", err)
    }

    if err := db.Migrate(model); err != nil {
        db.Close()
        model.Free()
        return nil, fmt.Errorf("migrating: %w", err)
    }

    return &UserDatabase{model: model, db: db}, nil
}

func (udb *UserDatabase) Close() {
    if udb.db != nil {
        udb.db.Close()
    }
    if udb.model != nil {
        udb.model.Free()
    }
}

func (udb *UserDatabase) CreateTables() error {
    _, err := udb.db.Execute(`
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT NOT NULL UNIQUE,
            active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT (datetime('now'))
        )
    `)
    return err
}

func (udb *UserDatabase) InsertUser(name, email string) error {
    stmt, err := udb.db.Prepare("INSERT INTO users (name, email) VALUES (?, ?)")
    if err != nil {
        return err
    }
    defer stmt.Finalize()

    stmt.BindText(1, name)
    stmt.BindText(2, email)
    return stmt.StepError()
}

func (udb *UserDatabase) InsertUsers(users []User) error {
    stmt, err := udb.db.Prepare("INSERT OR IGNORE INTO users (name, email) VALUES (?, ?)")
    if err != nil {
        return err
    }
    defer stmt.Finalize()

    for _, user := range users {
        stmt.BindText(1, user.Name)
        stmt.BindText(2, user.Email)
        if err := stmt.StepError(); err != nil {
            return err
        }
        stmt.Reset()
    }

    return nil
}

func (udb *UserDatabase) GetUser(id int64) (*User, error) {
    stmt, err := udb.db.Prepare("SELECT id, name, email, active FROM users WHERE id = ?")
    if err != nil {
        return nil, err
    }
    defer stmt.Finalize()

    stmt.BindInt64(1, id)
    if !stmt.Step() {
        return nil, fmt.Errorf("user not found")
    }

    return &User{
        ID:     stmt.ColumnInt64(0),
        Name:   stmt.ColumnText(1),
        Email:  stmt.ColumnText(2),
        Active: stmt.ColumnInt64(3) != 0,
    }, nil
}

func (udb *UserDatabase) SearchUsers(pattern string) ([]User, error) {
    stmt, err := udb.db.Prepare(
        "SELECT id, name, email, active FROM users WHERE name LIKE ?",
    )
    if err != nil {
        return nil, err
    }
    defer stmt.Finalize()

    stmt.BindText(1, "%"+pattern+"%")

    var users []User
    for stmt.Step() {
        users = append(users, User{
            ID:     stmt.ColumnInt64(0),
            Name:   stmt.ColumnText(1),
            Email:  stmt.ColumnText(2),
            Active: stmt.ColumnInt64(3) != 0,
        })
    }

    return users, nil
}

func (udb *UserDatabase) ListUsers() ([]User, error) {
    rows, err := udb.db.Query("SELECT id, name, email, active FROM users ORDER BY name")
    if err != nil {
        return nil, err
    }
    defer rows.Close()

    var users []User
    for rows.Next() {
        var user User
        if err := rows.Scan(&user.ID, &user.Name, &user.Email, &user.Active); err != nil {
            return nil, err
        }
        users = append(users, user)
    }

    return users, rows.Err()
}

func (udb *UserDatabase) CountUsers() (int64, error) {
    var count int64
    err := udb.db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
    return count, err
}

func main() {
    udb, err := NewUserDatabase("app.wlite", "app.db")
    if err != nil {
        log.Fatal(err)
    }
    defer udb.Close()

    if err := udb.CreateTables(); err != nil {
        log.Fatal(err)
    }

    users := []User{
        {Name: "Alice", Email: "alice@example.com"},
        {Name: "Bob", Email: "bob@example.com"},
        {Name: "Charlie", Email: "charlie@example.com"},
    }
    if err := udb.InsertUsers(users); err != nil {
        log.Fatal(err)
    }

    count, err := udb.CountUsers()
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Total users: %d\n\n", count)

    allUsers, err := udb.ListUsers()
    if err != nil {
        log.Fatal(err)
    }

    fmt.Println("All users:")
    for _, user := range allUsers {
        active := ""
        if !user.Active {
            active = " [inactive]"
        }
        fmt.Printf("  %d: %s <%s>%s\n", user.ID, user.Name, user.Email, active)
    }

    fmt.Println("\nSearch results for 'Ali':")
    searchResults, err := udb.SearchUsers("Ali")
    if err != nil {
        log.Fatal(err)
    }
    for _, user := range searchResults {
        fmt.Printf("  %s\n", user.Name)
    }

    alice, err := udb.GetUser(1)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("\nGot user: %s\n", alice.Name)
}
```
