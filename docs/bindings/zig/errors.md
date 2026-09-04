---
title: Error Handling in the Zig Binding
description: Error types, result checking, cleanup with defer, thread safety, and memory management.
---

# Error Handling

The wlite Zig binding maps C integer result codes to Zig error unions. Every
function that can fail returns `Error!T`, where `T` is the success type and
`Error` is a Zig error set covering all wlite failure modes.

## Error Types

The `Error` set is defined in `wlite.zig` and covers every `wlite_result` code
returned by the C library.

```zig
pub const Error = error{
    GeneralError,
    InvalidArgument,
    OutOfMemory,
    IoError,
    ParseError,
    ModelError,
    SqliteError,
    ConstraintError,
    NotFound,
    Busy,
    TransactionError,
};
```

Each error corresponds to a C constant.

| Error | C Constant | Meaning |
|-------|------------|---------|
| `GeneralError` | `WLITE_ERROR` | Unspecified error |
| `InvalidArgument` | `WLITE_INVALID_ARGUMENT` | Null pointer or bad parameter |
| `OutOfMemory` | `WLITE_OUT_OF_MEMORY` | Allocation failed |
| `IoError` | `WLITE_IO_ERROR` | File system error |
| `ParseError` | `WLITE_PARSE_ERROR` | Schema syntax error |
| `ModelError` | `WLITE_MODEL_ERROR` | Schema semantic error |
| `SqliteError` | `WLITE_SQLITE_ERROR` | SQLite runtime error |
| `ConstraintError` | `WLITE_CONSTRAINT_ERROR` | UNIQUE, CHECK, or FK violation |
| `NotFound` | `WLITE_NOT_FOUND` | No rows returned |
| `Busy` | `WLITE_BUSY` | Database is locked |
| `TransactionError` | `WLITE_TRANSACTION_ERROR` | Transaction misuse |

## Result Types

The binding uses Zig error unions for all fallible operations. A function that
returns `void` on success is typed as `Error!void`. A function that returns a
value is typed as `Error!T`.

```zig
// Returns nothing on success
pub fn execute(self: Database, sql: [*:0]const u8) Error!void {
    try checkResult(c.wlite_execute(self.ptr, sql, null));
}

// Returns a Statement on success
pub fn prepare(self: Database, sql: [*:0]const u8) Error!Statement {
    var ptr: ?*c.wlite_stmt = null;
    try checkResult(c.wlite_prepare(self.ptr, sql, &ptr));
    return .{ .stmt = ptr.? };
}
```

### Using try

The `try` keyword propagates errors to the caller. If the function succeeds,
execution continues. If it fails, the error is returned immediately.

```zig
var db = try wlite.Database.open("app.db");
defer db.deinit();

try db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");
```

If `open` or `execute` fails, the error is propagated up the call stack.

### Catching errors explicitly

Use `catch` to handle errors without propagating them.

```zig
var db = wlite.Database.open("app.db") catch |err| {
    switch (err) {
        error.IoError => std.debug.print("File not found\n", .{}),
        error.OutOfMemory => std.debug.print("Out of memory\n", .{}),
        else => std.debug.print("Unknown error: {}\n", .{err}),
    }
    return;
};
defer db.deinit();
```

### Catch and recover

You can catch an error, perform recovery, and continue.

```zig
fn openOrCreate(path: []const u8) !wlite.Database {
    return wlite.Database.open(path) catch |err| {
        if (err == error.NotFound) {
            // Database does not exist yet, create it
            return wlite.Database.open(path);
        }
        return err;
    };
}
```

## wlite_result Checking

The `checkResult` function converts a C integer result code into a Zig error.

```zig
fn checkResult(r: c_int) Error!void {
    if (r == c.WLITE_OK) return;
    return switch (r) {
        c.WLITE_ERROR => Error.GeneralError,
        c.WLITE_INVALID_ARGUMENT => Error.InvalidArgument,
        c.WLITE_OUT_OF_MEMORY => Error.OutOfMemory,
        c.WLITE_IO_ERROR => Error.IoError,
        c.WLITE_PARSE_ERROR => Error.ParseError,
        c.WLITE_MODEL_ERROR => Error.ModelError,
        c.WLITE_SQLITE_ERROR => Error.SqliteError,
        c.WLITE_CONSTRAINT_ERROR => Error.ConstraintError,
        c.WLITE_NOT_FOUND => Error.NotFound,
        c.WLITE_BUSY => Error.Busy,
        c.WLITE_TRANSACTION_ERROR => Error.TransactionError,
        else => Error.GeneralError,
    };
}
```

### Calling checkResult directly

When calling raw C functions, use `checkResult` to translate the result code.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

const result = c.wlite_open("app.db", &db);
try checkResult(result);
```

### Getting error messages

Use `wlite_strerror` to get a human-readable description of an error code.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

const result = c.wlite_open("app.db", &db);
if (result != c.WLITE_OK) {
    const msg = c.wlite_strerror(result);
    std.debug.print("Error: {s}\n", .{msg});
}
```

## Cleanup with defer

Every handle type provides a `deinit` method that frees the underlying C
resource. Use `defer` to ensure cleanup runs when the scope exits.

### Basic cleanup

```zig
{
    var db = try wlite.Database.open("app.db");
    defer db.deinit(); // always runs

    var stmt = try db.prepare("SELECT 1");
    defer stmt.deinit(); // always runs

    // If step() fails, both deinit calls still run
    _ = try stmt.step();
}
```

### Cleanup order

`defer` statements execute in reverse order of declaration. Statements finalized
first, then the database is closed. This matches the required teardown order.

```zig
{
    defer std.debug.print("3. db closed\n", .{});
    var db = try wlite.Database.open("app.db");
    defer db.deinit();

    defer std.debug.print("2. stmt finalized\n", .{});
    var stmt = try db.prepare("SELECT 1");
    defer stmt.deinit();

    defer std.debug.print("1. step done\n", .{});
    _ = try stmt.step();
}
// Output: 1, 2, 3
```

### errdefer

Use `errdefer` to run cleanup only on error. This is useful when a function
allocates a resource and wants to release it if a later step fails.

```zig
fn createDatabase(path: []const u8) !wlite.Database {
    var db = try wlite.Database.open(path);
    errdefer db.deinit(); // only on error

    try db.execute("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY)");
    // If this execute fails, db is closed. If we reach return, db stays open.
    return db;
}
```

### Multiple resources

When a function allocates multiple resources, each needs its own cleanup.

```zig
fn setup() !struct { wlite.Model, wlite.Database } {
    var model = try wlite.Model.load("schema.wlite");
    errdefer model.deinit();

    var db = try wlite.Database.open("app.db");
    errdefer db.deinit();

    try db.migrate(model);

    return .{ model, db };
}
```

If `Database.open` fails, `model.deinit()` runs. If `migrate` fails, both
`db.deinit()` and `model.deinit()` run. On success, both resources are returned
to the caller.

### Cleanup in loops

Resources allocated inside loops need cleanup within the loop body.

```zig
fn processRows(db: wlite.Database) !void {
    var stmt = try db.prepare("SELECT id FROM users");
    defer stmt.deinit();

    while (try stmt.step()) {
        const id = stmt.columnInt64(0);
        // Each iteration may allocate a sub-statement
        var sub = try db.prepare("INSERT INTO log (user_id) VALUES (?)");
        defer sub.deinit();

        _ = try sub.bindInt64(1, id);
        _ = try sub.step();
    }
}
```

## Thread Safety

### Models

Models are immutable after loading. They can be shared across threads without
synchronization.

```zig
const model = try wlite.Model.load("schema.wlite");
defer model.deinit();

// Safe to share across threads
const t1 = try std.Thread.spawn(.{}, worker, .{model});
const t2 = try std.Thread.spawn(.{}, worker, .{model});
```

### Database connections

Database connections are not thread-safe. Each thread must open its own
connection. Do not share a `Database` across threads.

```zig
// WRONG: do not do this
const db = try wlite.Database.open("app.db");
const t1 = try std.Thread.spawn(.{}, worker, .{db}); // data race!

// CORRECT: each thread opens its own connection
fn worker(model: wlite.Model) void {
    var db = wlite.Database.open("app.db") catch return;
    defer db.deinit();
    db.migrate(model) catch return;
}
```

### Statements

Statements are bound to a specific database connection. Do not use a statement
from one connection on another.

```zig
var db = try wlite.Database.open("app.db");
defer db.deinit();

var stmt = try db.prepare("SELECT 1");
defer stmt.deinit();

// stmt is only valid on db
_ = try stmt.step();
```

### Transactions

Transactions are also bound to a single connection. Do not share transactions
across threads.

```zig
fn worker(db: wlite.Database) void {
    var tx = wlite.Transaction.begin(db) catch return;
    defer tx.deinit();

    // All operations on the same connection
    db.execute("INSERT INTO t VALUES (1)") catch return;
    tx.commit() catch return;
}
```

## Memory Management

### Ownership rules

The binding follows libwlite's ownership conventions:

- **Caller owns** `Database`, `Model`, `Statement`, `Transaction`
- **Library owns** internal schema tables and fields (freed with model)
- **Text pointers** from `columnText` are valid until the next `step` or
  `deinit` call

### Copying text values

Text returned by `columnText` is borrowed from the statement. Copy it if you
need it to outlive the step.

```zig
fn getName(stmt: wlite.Statement) ![]const u8 {
    const ptr = stmt.columnText(0);
    return std.mem.span(ptr);
}

// Or allocate a copy
fn getNameOwned(stmt: wlite.Statement, allocator: std.mem.Allocator) ![]const u8 {
    const ptr = stmt.columnText(0);
    return try allocator.dupe(u8, std.mem.span(ptr));
}
```

### Allocator usage

The binding does not use Zig allocators internally. All memory is managed by the
C library. Use Zig allocators only for your own data structures.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

fn collectUsers(db: wlite.Database, allocator: std.mem.Allocator) !std.ArrayList([]const u8) {
    var stmt = try db.prepare("SELECT name FROM users");
    defer stmt.deinit();

    var names = std.ArrayList([]const u8).init(allocator);
    errdefer names.deinit();

    while (try stmt.step()) {
        const name = try allocator.dupe(u8, std.mem.span(stmt.columnText(0)));
        try names.append(name);
    }

    return names;
}

fn freeNames(names: std.ArrayList([]const u8), allocator: std.mem.Allocator) void {
    for (names.items) |name| {
        allocator.free(name);
    }
    names.deinit();
}
```

### Stack allocation for small buffers

For small, fixed-size buffers, use stack allocation to avoid allocator overhead.

```zig
fn formatUserId(id: i64) ![20]u8 {
    var buf: [20]u8 = undefined;
    const result = std.fmt.bufPrint(&buf, "{d}", .{id}) catch return error.WliteError;
    return buf;
}
```

## Error Handling Patterns

### Propagate with try

The simplest pattern: let errors bubble up.

```zig
fn runQuery(db: wlite.Database) !i64 {
    var stmt = try db.prepare("SELECT COUNT(*) FROM users");
    defer stmt.deinit();

    if (try stmt.step()) {
        return stmt.columnInt64(0);
    }
    return 0;
}
```

### Switch on specific errors

Handle different error types differently.

```zig
fn robustOpen(path: []const u8) !wlite.Database {
    return wlite.Database.open(path) catch |err| {
        switch (err) {
            error.IoError => {
                std.debug.print("Cannot open {s}, trying memory database\n", .{path});
                return wlite.Database.openMemory();
            },
            error.OutOfMemory => {
                std.debug.print("Out of memory\n", .{});
                return err;
            },
            else => return err,
        }
    };
}
```

### Log and continue

Log the error but continue processing.

```zig
fn processBatch(db: wlite.Database, items: []const []const u8) void {
    for (items, 0..) |item, i| {
        var stmt = db.prepare("INSERT INTO t VALUES (?)") catch |err| {
            std.debug.print("Row {d}: prepare failed: {}\n", .{ i, err });
            continue;
        };
        defer stmt.deinit();

        _ = stmt.bindText(1, item.ptr) catch |err| {
            std.debug.print("Row {d}: bind failed: {}\n", .{ i, err });
            continue;
        };

        _ = stmt.step() catch |err| {
            std.debug.print("Row {d}: step failed: {}\n", .{ i, err });
        };
    }
}
```

### Accumulate errors

Collect all errors and return them at the end.

```zig
const BatchResult = struct {
    inserted: usize,
    errors: std.ArrayList(struct { index: usize, err: anyerror }),
};

fn batchInsert(db: wlite.Database, rows: []const []const u8, allocator: std.mem.Allocator) !BatchResult {
    var result = BatchResult{
        .inserted = 0,
        .errors = std.ArrayList(struct { index: usize, err: anyerror }).init(allocator),
    };
    errdefer result.errors.deinit();

    var stmt = try db.prepare("INSERT INTO t (val) VALUES (?)");
    defer stmt.deinit();

    for (rows, 0..) |row, i| {
        _ = stmt.bindText(1, row.ptr) catch |err| {
            try result.errors.append(.{ .index = i, .err = err });
            continue;
        };
        _ = stmt.step() catch |err| {
            try result.errors.append(.{ .index = i, .err = err });
            continue;
        };
        result.inserted += 1;
    }

    return result;
}
```

### Retry on BUSY

When the database is locked, retry after a short delay.

```zig
fn executeRetry(db: wlite.Database, sql: [*:0]const u8, max_retries: u32) !void {
    var attempts: u32 = 0;
    while (attempts < max_retries) : (attempts += 1) {
        var stmt = db.prepare(sql) catch |err| {
            if (err == error.Busy and attempts < max_retries - 1) {
                std.time.sleep(10 * std.time.ns_per_ms);
                continue;
            }
            return err;
        };
        defer stmt.deinit();

        _ = stmt.step() catch |err| {
            if (err == error.Busy and attempts < max_retries - 1) {
                std.time.sleep(10 * std.time.ns_per_ms);
                continue;
            }
            return err;
        };
        return;
    }
    return error.Busy;
}
```

## Next Steps

- [Migration](migration.md) covers model loading and schema migration
- [Queries](queries.md) covers prepared statements, bindings, and transactions
