---
title: Queries in the Zig Binding
description: Prepared statements, parameter binding, column access, records, and transactions.
---

# Queries

The wlite Zig binding provides prepared statements for executing parameterized
queries. This page covers statement preparation, parameter binding, stepping
through results, accessing columns, working with record slices, and using
transactions with savepoints.

## Prepare

Create a prepared statement with `Database.prepare`. The statement is finalized
automatically when it goes out of scope via `defer`.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    var db = try wlite.Database.openMemory();
    defer db.deinit();

    try db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

    var stmt = try db.prepare("SELECT name, age FROM users WHERE age > ?");
    defer stmt.deinit();
}
```

The SQL string must be a null-terminated C string. Use Zig multi-line string
literals for readability.

```zig
const sql =
    \\SELECT id, name, email
    \\FROM users
    \\WHERE active = 1
    \\ORDER BY name
;
var stmt = try db.prepare(sql);
defer stmt.deinit();
```

## Bind Parameters

Bind values to parameters by their 1-based index. The binding functions mirror
the C API.

### bindInt64

```zig
_ = try stmt.bindInt64(1, 42);
```

### bindDouble

```zig
_ = try stmt.bindDouble(1, 3.14);
```

### bindText

```zig
_ = try stmt.bindText(1, "hello");
```

### bindNull

```zig
_ = try stmt.bindNull(1);
```

### Full bind example

```zig
var stmt = try db.prepare("INSERT INTO users (name, age) VALUES (?, ?)");
defer stmt.deinit();

_ = try stmt.bindText(1, "Alice");
_ = try stmt.bindInt64(2, 30);
_ = try stmt.step();
_ = try stmt.reset();
```

The `reset` call clears the bound values so you can rebind and re-execute the
statement.

## Step

The `step` method advances to the next row. It returns `true` if a row is
available, `false` when there are no more rows.

```zig
var stmt = try db.prepare("SELECT name FROM users");
defer stmt.deinit();

while (try stmt.step()) {
    const name = stmt.columnText(0);
    std.debug.print("{s}\n", .{name});
}
```

If an error occurs during stepping, `step` returns an error. The `while` loop
with `try` handles both cases: errors propagate, and a false return ends the
loop.

### Single-row query

For queries that return at most one row, use `if` instead of `while`.

```zig
var stmt = try db.prepare("SELECT COUNT(*) FROM users");
defer stmt.deinit();

if (try stmt.step()) {
    const count = stmt.columnInt64(0);
    std.debug.print("Total users: {d}\n", .{count});
}
```

## Column Access

After a successful `step`, access column values by their 0-based index. The
column functions return Zig-native types.

### Column functions

| Function | Return Type | Description |
|----------|-------------|-------------|
| `columnCount()` | `c_int` | Number of columns in the result set |
| `columnName(index)` | `[*:0]const u8` | Name of the column at the given index |
| `columnInt64(index)` | `i64` | Integer value |
| `columnDouble(index)` | `f64` | Floating-point value |
| `columnText(index)` | `[*:0]const u8` | Null-terminated text value |

### Accessing columns

```zig
var stmt = try db.prepare("SELECT id, name, age, email FROM users");
defer stmt.deinit();

while (try stmt.step()) {
    const id = stmt.columnInt64(0);
    const name = stmt.columnText(1);
    const age = stmt.columnInt64(2);
    const email = stmt.columnText(3);

    std.debug.print("{d}: {s} ({d}) <{s}>\n", .{ id, name, age, email });
}
```

### Column count and names

```zig
var stmt = try db.prepare("SELECT * FROM users");
defer stmt.deinit();

const ncols = stmt.columnCount();
std.debug.print("Columns: {d}\n", .{ncols});

for (0..@intCast(ncols)) |i| {
    const col_name = stmt.columnName(@intCast(i));
    std.debug.print("  [{d}] {s}\n", .{ i, col_name });
}
```

### Converting text to Zig slices

Text pointers from `columnText` are null-terminated C strings. Convert them to
Zig slices with `std.mem.span`.

```zig
while (try stmt.step()) {
    const name_ptr = stmt.columnText(0);
    const name = std.mem.span(name_ptr);
    std.debug.print("Name: {s} (len: {d})\n", .{ name, name.len });
}
```

## Records

A record is a row fetched from the database. The binding returns text values as
`[*:0]const u8` pointers. To collect results into a Zig list, allocate and copy.

### Collecting rows into an ArrayList

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

const User = struct {
    id: i64,
    name: []const u8,
    email: []const u8,
};

fn listUsers(db: wlite.Database, allocator: std.mem.Allocator) !std.ArrayList(User) {
    var stmt = try db.prepare("SELECT id, name, email FROM users ORDER BY name");
    defer stmt.deinit();

    var users = std.ArrayList(User).init(allocator);
    errdefer users.deinit();

    while (try stmt.step()) {
        const id = stmt.columnInt64(0);
        const name = try allocator.dupe(u8, std.mem.span(stmt.columnText(1)));
        const email = try allocator.dupe(u8, std.mem.span(stmt.columnText(2)));

        try users.append(.{
            .id = id,
            .name = name,
            .email = email,
        });
    }

    return users;
}
```

### Freeing collected records

When you are done with collected records, free the duplicated strings and the
list.

```zig
var users = try listUsers(db, allocator);
defer {
    for (users.items) |user| {
        allocator.free(user.name);
        allocator.free(user.email);
    }
    users.deinit();
}
```

### Filtering with parameters

```zig
fn searchUsers(db: wlite.Database, pattern: []const u8, allocator: std.mem.Allocator) !std.ArrayList(User) {
    var stmt = try db.prepare("SELECT id, name, email FROM users WHERE name LIKE ?");
    defer stmt.deinit();

    var buf: [256]u8 = undefined;
    const like_pattern = try std.fmt.bufPrint(&buf, "%{s}%", .{pattern});
    _ = try stmt.bindText(1, like_pattern.ptr);

    var users = std.ArrayList(User).init(allocator);
    errdefer users.deinit();

    while (try stmt.step()) {
        const id = stmt.columnInt64(0);
        const name = try allocator.dupe(u8, std.mem.span(stmt.columnText(1)));
        const email = try allocator.dupe(u8, std.mem.span(stmt.columnText(2)));

        try users.append(.{
            .id = id,
            .name = name,
            .email = email,
        });
    }

    return users;
}
```

### Batch inserts

Reuse a prepared statement for batch inserts by resetting between iterations.

```zig
fn batchInsert(db: wlite.Database, names: []const []const u8) !void {
    var stmt = try db.prepare("INSERT INTO users (name) VALUES (?)");
    defer stmt.deinit();

    for (names) |name| {
        _ = try stmt.bindText(1, name.ptr);
        _ = try stmt.step();
        _ = try stmt.reset();
    }
}
```

## Transactions

Transactions ensure that a group of operations either all succeed or all fail.
The Zig binding provides `Transaction` with `begin`, `commit`, and `rollback`.

### Basic transaction

```zig
var tx = try wlite.Transaction.begin(db);
defer tx.deinit();

try db.execute("INSERT INTO users (name) VALUES ('Alice')");
try db.execute("INSERT INTO users (name) VALUES ('Bob')");

try tx.commit();
```

If `commit` is not called before the transaction goes out of scope, the
transaction is rolled back by `deinit`.

### Transaction with error handling

```zig
fn transferFunds(db: wlite.Database, from: i64, to: i64, amount: i64) !void {
    var tx = try wlite.Transaction.begin(db);
    defer tx.deinit();

    // Debit sender
    {
        var stmt = try db.prepare("UPDATE accounts SET balance = balance - ? WHERE id = ?");
        defer stmt.deinit();

        _ = try stmt.bindInt64(1, amount);
        _ = try stmt.bindInt64(2, from);
        _ = try stmt.step();
    }

    // Credit receiver
    {
        var stmt = try db.prepare("UPDATE accounts SET balance = balance + ? WHERE id = ?");
        defer stmt.deinit();

        _ = try stmt.bindInt64(1, amount);
        _ = try stmt.bindInt64(2, to);
        _ = try stmt.step();
    }

    // Verify sender balance
    {
        var stmt = try db.prepare("SELECT balance FROM accounts WHERE id = ?");
        defer stmt.deinit();

        _ = try stmt.bindInt64(1, from);
        if (try stmt.step()) {
            const balance = stmt.columnInt64(0);
            if (balance < 0) {
                try tx.rollback();
                return error.InsufficientFunds;
            }
        }
    }

    try tx.commit();
}
```

### Commit and rollback

```zig
var tx = try wlite.Transaction.begin(db);
defer tx.deinit();

// Do work...
try db.execute("INSERT INTO logs (msg) VALUES ('started')");

if (something_failed) {
    try tx.rollback();
    return;
}

try db.execute("INSERT INTO logs (msg) VALUES ('completed')");
try tx.commit();
```

## Savepoints

Savepoints let you roll back part of a transaction without losing all changes.
Use `savepoint` to create one, `rollbackTo` to undo changes since the savepoint,
and `release` to discard the savepoint.

### Basic savepoint

```zig
var tx = try wlite.Transaction.begin(db);
defer tx.deinit();

try tx.savepoint("before_import");

// Import data that might fail
var stmt = try db.prepare("INSERT INTO large_table SELECT * FROM staging");
defer stmt.deinit();

_ = try stmt.step() catch |err| {
    try tx.rollbackTo("before_import");
    std.debug.print("Import failed, rolled back to savepoint: {}\n", .{err});
    try tx.release("before_import");
    return;
};

try tx.release("before_import");
try tx.commit();
```

### Multiple savepoints

You can nest savepoints. Each savepoint has a unique name.

```zig
var tx = try wlite.Transaction.begin(db);
defer tx.deinit();

try tx.savepoint("phase1");
try db.execute("INSERT INTO t VALUES (1)");

try tx.savepoint("phase2");
try db.execute("INSERT INTO t VALUES (2)");

// Undo phase 2 only
try tx.rollbackTo("phase2");
try tx.release("phase2");

// phase 1 changes remain
try tx.commit();
```

### Savepoint with error recovery

```zig
fn importWithRecovery(db: wlite.Database, rows: []const struct { []const u8, i64 }) !void {
    var tx = try wlite.Transaction.begin(db);
    defer tx.deinit();

    var stmt = try db.prepare("INSERT INTO data (name, value) VALUES (?, ?)");
    defer stmt.deinit();

    for (rows, 0..) |row, i| {
        const sp_name = try std.fmt.allocPrintZ(std.heap.page_allocator, "sp_{d}", .{i});

        try tx.savepoint(sp_name.ptr);
        _ = try stmt.bindText(1, row[0].ptr);
        _ = try stmt.bindInt64(2, row[1]);
        _ = try stmt.step() catch |err| {
            try tx.rollbackTo(sp_name.ptr);
            std.debug.print("Row {d} failed: {}\n", .{ i, err });
            continue;
        };
        try tx.release(sp_name.ptr);
    }

    try tx.commit();
}
```

## Complete Query Example

This example demonstrates a full query workflow: create a table, insert data,
query with parameters, and use transactions.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    var db = try wlite.Database.openMemory();
    defer db.deinit();

    // Create table
    try db.execute(
        \\CREATE TABLE users (
        \\    id INTEGER PRIMARY KEY,
        \\    name TEXT NOT NULL,
        \\    age INTEGER,
        \\    email TEXT
        \\)
    );

    // Insert data in a transaction
    {
        var tx = try wlite.Transaction.begin(db);
        defer tx.deinit();

        var stmt = try db.prepare("INSERT INTO users (name, age, email) VALUES (?, ?, ?)");
        defer stmt.deinit();

        _ = try stmt.bindText(1, "Alice");
        _ = try stmt.bindInt64(2, 30);
        _ = try stmt.bindText(3, "alice@example.com");
        _ = try stmt.step();
        _ = try stmt.reset();

        _ = try stmt.bindText(1, "Bob");
        _ = try stmt.bindInt64(2, 25);
        _ = try stmt.bindText(3, "bob@example.com");
        _ = try stmt.step();

        try tx.commit();
    }

    // Query with parameter
    {
        var stmt = try db.prepare("SELECT name, age FROM users WHERE age > ?");
        defer stmt.deinit();

        _ = try stmt.bindInt64(1, 20);

        while (try stmt.step()) {
            const name = std.mem.span(stmt.columnText(0));
            const age = stmt.columnInt64(1);
            std.debug.print("{s} is {d}\n", .{ name, age });
        }
    }
}
```

## Next Steps

- [Errors](errors.md) covers error types, cleanup, and thread safety
- [Migration](migration.md) covers model loading and schema migration
