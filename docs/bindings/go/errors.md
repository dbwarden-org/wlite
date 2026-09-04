---
title: Error Handling Guide
description: Error types, error constants, error handling patterns, cleanup with defer, thread safety, and memory management in the wlite Go binding.
---

# Error Handling Guide

The Go binding uses Go's standard `error` interface for all operations. Every function that can fail returns an `error` as its last return value. The `wlite.Error` type provides structured error information including a numeric code and a human-readable message.

## The Error type

The `wlite.Error` type implements the `error` interface:

```go
type Error struct {
    Code    int
    Message string
}

func (e *Error) Error() string {
    return fmt.Sprintf("wlite error %d: %s", e.Code, e.Message)
}
```

When a wlite operation fails, the returned error is always a pointer to `*wlite.Error`. You can extract the code and message for programmatic handling.

### Accessing error details

```go
db, err := wlite.Open("app.db")
if err != nil {
    var wliteErr *wlite.Error
    if errors.As(err, &wliteErr) {
        fmt.Printf("Error code: %d\n", wliteErr.Code)
        fmt.Printf("Error message: %s\n", wliteErr.Message)
    } else {
        fmt.Printf("Non-wlite error: %v\n", err)
    }
}
```

### String representation

The `Error()` method produces a formatted string:

```go
err := &wlite.Error{Code: 7, Message: "SQLITE_ERROR: near \"SELEC\": syntax error"}
fmt.Println(err.Error())
// Output: wlite error 7: SQLITE_ERROR: near "SELEC": syntax error
```

## Error constants

The Go binding exposes the following error constants. Each corresponds to a `wlite_result` value from the C API.

| Constant | Value | Meaning |
|----------|-------|---------|
| `wlite.OK` | 0 | Operation succeeded |
| `wlite.ERROR` | 1 | General or unspecified error |
| `wlite.INVALID_ARGUMENT` | 2 | A required argument was invalid or missing |
| `wlite.OUT_OF_MEMORY` | 3 | Memory allocation failed |
| `wlite.IO_ERROR` | 4 | A file I/O operation failed |
| `wlite.PARSE_ERROR` | 5 | The schema file could not be parsed |
| `wlite.MODEL_ERROR` | 6 | The model is invalid or corrupted |
| `wlite.SQLITE_ERROR` | 7 | SQLite returned an error |
| `wlite.CONSTRAINT_ERROR` | 8 | A database constraint was violated |
| `wlite.NOT_FOUND` | 9 | The requested resource was not found |
| `wlite.BUSY` | 10 | The database is locked by another connection |
| `wlite.TRANSACTION_ERROR` | 11 | A transaction operation failed |

The binding also provides these aliases for backward compatibility:

| Alias | Maps to |
|-------|---------|
| `wlite.ERR_SYNTAX` | `wlite.PARSE_ERROR` |
| `wlite.ERR_NULL_PTR` | `wlite.INVALID_ARGUMENT` |
| `wlite.ERR_IO` | `wlite.IO_ERROR` |

### Using error constants

```go
db, err := wlite.Open("app.db")
if err != nil {
    var wliteErr *wlite.Error
    if errors.As(err, &wliteErr) {
        switch wliteErr.Code {
        case wlite.NOT_FOUND:
            log.Println("Database file not found")
        case wlite.IO_ERROR:
            log.Println("I/O error:", wliteErr.Message)
        case wlite.SQLITE_ERROR:
            log.Println("SQLite error:", wliteErr.Message)
        case wlite.CONSTRAINT_ERROR:
            log.Println("Constraint violation:", wliteErr.Message)
        case wlite.BUSY:
            log.Println("Database is locked")
        case wlite.OUT_OF_MEMORY:
            log.Println("Out of memory")
        case wlite.TRANSACTION_ERROR:
            log.Println("Transaction error:", wliteErr.Message)
        case wlite.PARSE_ERROR:
            log.Println("Schema parse error:", wliteErr.Message)
        case wlite.MODEL_ERROR:
            log.Println("Model error:", wliteErr.Message)
        case wlite.INVALID_ARGUMENT:
            log.Println("Invalid argument:", wliteErr.Message)
        case wlite.ERROR:
            log.Println("General error:", wliteErr.Message)
        default:
            log.Printf("Unknown error %d: %s", wliteErr.Code, wliteErr.Message)
        }
    } else {
        log.Println("Non-wlite error:", err)
    }
}
```

## Error handling patterns

### The if err != nil pattern

The standard Go pattern for error handling works with wlite. Check every error:

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()

db, err := wlite.Open("app.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()

if err := db.Migrate(model); err != nil {
    log.Fatal(err)
}

_, err = db.Execute("INSERT INTO users (name) VALUES (?)", "Alice")
if err != nil {
    log.Fatal(err)
}
```

### Error wrapping

Use `fmt.Errorf` with `%w` to wrap errors with additional context:

```go
func createUser(db *wlite.DB, name, email string) error {
    _, err := db.Execute(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        name, email,
    )
    if err != nil {
        return fmt.Errorf("creating user %s: %w", name, err)
    }
    return nil
}
```

The wrapped error preserves the original `*wlite.Error` so you can still extract the code:

```go
err := createUser(db, "Alice", "alice@example.com")
if err != nil {
    var wliteErr *wlite.Error
    if errors.As(err, &wliteErr) {
        log.Printf("wlite error %d: %s (context: %v)", wliteErr.Code, wliteErr.Message, err)
    }
}
```

### Multi-step error handling

When a function performs multiple operations, handle each error separately:

```go
func setupDatabase(modelPath, dbPath string) error {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return fmt.Errorf("loading model: %w", err)
    }
    defer model.Close()

    if err := model.Validate(); err != nil {
        return fmt.Errorf("validating model: %w", err)
    }

    db, err := wlite.Open(dbPath)
    if err != nil {
        return fmt.Errorf("opening database: %w", err)
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        return fmt.Errorf("migrating database: %w", err)
    }

    fmt.Println("Database setup complete.")
    return nil
}
```

### Switch on error codes

When you need different behavior for different error types, switch on the code:

```go
func handleQueryError(err error) error {
    var wliteErr *wlite.Error
    if !errors.As(err, &wliteErr) {
        return err
    }

    switch wliteErr.Code {
    case wlite.NOT_FOUND:
        return fmt.Errorf("resource not found: %w", err)
    case wlite.BUSY:
        return fmt.Errorf("database is busy, retry later: %w", err)
    case wlite.CONSTRAINT_ERROR:
        return fmt.Errorf("constraint violation: %w", err)
    case wlite.SQLITE_ERROR:
        return fmt.Errorf("SQL error: %s: %w", wliteErr.Message, err)
    default:
        return fmt.Errorf("unexpected wlite error %d: %w", wliteErr.Code, err)
    }
}
```

## Cleanup with defer

The `defer` pattern is the primary mechanism for ensuring resources are released. Every resource type in the binding has a cleanup method.

### Model cleanup

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()
```

### Database cleanup

```go
db, err := wlite.Open("app.db")
if err != nil {
    log.Fatal(err)
}
defer db.Close()
```

### Statement cleanup

```go
stmt, err := db.Prepare("SELECT * FROM users")
if err != nil {
    log.Fatal(err)
}
defer stmt.Close()
```

### Rows cleanup

```go
rows, err := db.Query("SELECT * FROM users")
if err != nil {
    log.Fatal(err)
}
defer rows.Close()
```

### Transaction cleanup

Transactions must be explicitly committed or rolled back. Use a deferred rollback with a flag:

```go
func transactionalInsert(db *wlite.DB, data []Record) error {
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

    for _, rec := range data {
        _, err := db.Execute("INSERT INTO records (name, value) VALUES (?, ?)", rec.Name, rec.Value)
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

### Nested resource cleanup

When multiple resources are created, defer them in reverse order of creation. Go executes deferred calls in LIFO order, so deferring in creation order ensures proper cleanup:

```go
func processWithModel(modelPath, dbPath string) error {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return err
    }
    defer model.Close()

    db, err := wlite.Open(dbPath)
    if err != nil {
        return err
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        return err
    }

    stmt, err := db.Prepare("SELECT * FROM users")
    if err != nil {
        return err
    }
    defer stmt.Close()

    rows, err := db.Query("SELECT * FROM orders")
    if err != nil {
        return err
    }
    defer rows.Close()

    // Use all resources here...

    return nil
}
```

## Thread safety

The Go binding follows these concurrency rules:

### Models are safe to share

A `Model` is immutable after loading. Multiple goroutines can read from the same model concurrently without synchronization:

```go
model, err := wlite.LoadModel("app.wlite")
if err != nil {
    log.Fatal(err)
}
defer model.Close()

var wg sync.WaitGroup
for i := 0; i < 4; i++ {
    wg.Add(1)
    go func(workerID int) {
        defer wg.Done()
        // All goroutines share the same model safely
        fmt.Printf("Worker %d: model has %d tables\n", workerID, model.TableCount())
    }(i)
}
wg.Wait()
```

### Database connections are not safe to share

Each goroutine that needs database access must open its own connection. Do not share a `DB` across goroutines:

```go
// INCORRECT - do not do this
var db *wlite.DB
go func() { db.Execute("...") }()  // Race condition

// CORRECT - each goroutine opens its own connection
go func() {
    db, err := wlite.Open("app.db")
    if err != nil {
        return
    }
    defer db.Close()
    db.Execute("...")
}()
```

### Connection pool pattern

For applications that need concurrent database access, use a connection pool:

```go
type Pool struct {
    connections chan *wlite.DB
}

func NewPool(size int, dbPath string) (*Pool, error) {
    pool := &Pool{
        connections: make(chan *wlite.DB, size),
    }

    for i := 0; i < size; i++ {
        db, err := wlite.Open(dbPath)
        if err != nil {
            pool.Close()
            return nil, err
        }
        pool.connections <- db
    }

    return pool, nil
}

func (p *Pool) Get() *wlite.DB {
    return <-p.connections
}

func (p *Pool) Put(db *wlite.DB) {
    p.connections <- db
}

func (p *Pool) Close() {
    close(p.connections)
    for db := range p.connections {
        db.Close()
    }
}
```

### Using the pool

```go
pool, err := NewPool(4, "app.db")
if err != nil {
    log.Fatal(err)
}
defer pool.Close()

var wg sync.WaitGroup
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        db := pool.Get()
        defer pool.Put(db)
        db.Execute("INSERT INTO work (data) VALUES (?)", fmt.Sprintf("item-%d", n))
    }(i)
}
wg.Wait()
```

## Memory management

Every C-level resource must be freed. The Go binding provides cleanup methods for each type.

### Resource lifecycle

| Type | Allocate | Free | Notes |
|------|----------|------|-------|
| `Model` | `LoadModel`, `ModelFromBytes` | `Close()` | Free after use |
| `DB` | `Open`, `OpenMemory` | `Close()` | Free after use |
| `Stmt` | `db.Prepare` | `Close()` | Finalizes the C statement |
| `Tx` | `db.Transaction` | `Commit()` or `Rollback()` | Must be committed or rolled back |
| `Rows` | `db.Query` | `Close()` | Free after iteration |

### Double-close safety

Calling `Close()` on an already-closed resource is safe. The methods check for nil pointers:

```go
db, _ := wlite.Open("app.db")
db.Close()
db.Close() // Safe, no-op
```

### Cleanup on error

When initialization fails partway through, clean up resources created before the failure:

```go
func setup(modelPath, dbPath string) (*wlite.DB, error) {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return nil, err
    }

    if err := model.Validate(); err != nil {
        model.Close()
        return nil, err
    }

    db, err := wlite.Open(dbPath)
    if err != nil {
        model.Close()
        return nil, err
    }

    if err := db.Migrate(model); err != nil {
        db.Close()
        model.Close()
        return nil, err
    }

    // Model can be freed after migration if not needed further
    model.Close()

    return db, nil
}
```

### Using defer for complex cleanup

```go
func complexOperation(modelPath, dbPath string) error {
    model, err := wlite.LoadModel(modelPath)
    if err != nil {
        return err
    }
    defer model.Close()

    db, err := wlite.Open(dbPath)
    if err != nil {
        return err
    }
    defer db.Close()

    if err := db.Migrate(model); err != nil {
        return err
    }

    stmt, err := db.Prepare("SELECT * FROM users WHERE id = ?")
    if err != nil {
        return err
    }
    defer stmt.Close()

    // ... use stmt ...

    return nil
}
```

## Error handling with the error interface

Because `*wlite.Error` implements the `error` interface, you can use it anywhere Go expects an error:

### Checking error identity

```go
if err == wlite.ErrNotFound {
    // Direct comparison (not recommended for wrapped errors)
}
```

### Using errors.Is and errors.As

```go
err := someWliteOperation()
if errors.Is(err, wlite.ErrBusy) {
    // Database is busy
}

var wliteErr *wlite.Error
if errors.As(err, &wliteErr) {
    // Handle wlite-specific error
}
```

### Collecting errors

```go
type ErrorCollector struct {
    errors []error
}

func (ec *ErrorCollector) Add(err error) {
    if err != nil {
        ec.errors = append(ec.errors, err)
    }
}

func (ec *ErrorCollector) HasErrors() bool {
    return len(ec.errors) > 0
}

func (ec *ErrorCollector) Error() string {
    var sb strings.Builder
    for i, err := range ec.errors {
        if i > 0 {
            sb.WriteString("; ")
        }
        sb.WriteString(err.Error())
    }
    return sb.String()
}
```

## Panic recovery

The `Step` method panics on unexpected errors. If you cannot tolerate panics, recover from them:

```go
func safeStep(stmt *wlite.Stmt) (result bool, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("recovered from panic: %v", r)
        }
    }()
    return stmt.Step(), nil
}
```

In practice, panics from `Step` indicate serious errors like out-of-memory conditions. For most applications, letting the panic propagate and crash is appropriate.

## Best practices

Always check errors immediately after the operation that produced them. Use `defer` for cleanup. Wrap errors with context using `fmt.Errorf` and `%w`. Use `errors.As` to extract `*wlite.Error` for code-based handling. Open one database connection per goroutine. Free models, statements, and rows with their cleanup methods. Use the `committed` flag pattern for transaction cleanup with defer.

## Further reading

- [Index](index.md) for types, installation, and CGO configuration.
- [Migration guide](migration.md) for schema management with `LoadModel`, `Migrate`, `Diff`, `Plan`, and `Snapshot`.
- [Query guide](queries.md) for prepared statements, binding, and transactions.
