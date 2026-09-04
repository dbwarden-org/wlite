---
title: Zig Binding
description: Zig C interop binding for wlite.
---

# Zig Binding

The Zig binding uses `@cImport` to directly interface with libwlite's C ABI. It provides Zig-idiomatic error handling and memory management.

The binding leverages Zig's comptime features and C interop to provide a thin, efficient wrapper around the libwlite C library. It follows Zig conventions for error handling, memory allocation, and resource management.

## Usage

```zig
const std = @import("std");
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});

pub fn main() !void {
    var model: ?*wlite.wlite_model = null;
    var db: ?*wlite.wlite_db = null;

    _ = wlite.wlite_model_load_file("app.wlite", &model);
    defer if (model) |m| wlite.wlite_model_free(m);

    _ = wlite.wlite_open("app.db", &db);
    defer if (db) |d| wlite.wlite_close(d);

    _ = wlite.wlite_migrate(db.?, model.?);

    var stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(db.?, "SELECT * FROM users", &stmt);
    defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

    while (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
        const name = wlite.wlite_column_text(stmt.?, 0);
        std.debug.print("{s}\n", .{name});
    }
}
```

### Complete workflow example

```zig
const std = @import("std");
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});

pub fn main() !void {
    var model: ?*wlite.wlite_model = null;
    var db: ?*wlite.wlite_db = null;

    // Load model
    const model_result = wlite.wlite_model_load_file("app.wlite", &model);
    if (model_result != wlite.WLITE_OK) {
        const msg = wlite.wlite_strerror(model_result);
        std.debug.print("Failed to load model: {s}\n", .{msg});
        return error.WliteError;
    }
    defer if (model) |m| wlite.wlite_model_free(m);

    // Open database
    const open_result = wlite.wlite_open("app.db", &db);
    if (open_result != wlite.WLITE_OK) {
        const msg = wlite.wlite_strerror(open_result);
        std.debug.print("Failed to open database: {s}\n", .{msg});
        return error.WliteError;
    }
    defer if (db) |d| wlite.wlite_close(d);

    // Run migrations
    const migrate_result = wlite.wlite_migrate(db.?, model.?);
    if (migrate_result != wlite.WLITE_OK) {
        const msg = wlite.wlite_strerror(migrate_result);
        std.debug.print("Migration failed: {s}\n", .{msg});
        return error.WliteError;
    }

    // Insert data
    var insert_stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "INSERT INTO users (name, email) VALUES (?, ?)",
        &insert_stmt,
    );
    defer if (insert_stmt) |s| wlite.wlite_stmt_finalize(s);

    _ = wlite.wlite_bind_text(insert_stmt.?, 1, "Alice");
    _ = wlite.wlite_bind_text(insert_stmt.?, 2, "alice@example.com");
    _ = wlite.wlite_step(insert_stmt.?);
    _ = wlite.wlite_reset(insert_stmt.?);

    _ = wlite.wlite_bind_text(insert_stmt.?, 1, "Bob");
    _ = wlite.wlite_bind_text(insert_stmt.?, 2, "bob@example.com");
    _ = wlite.wlite_step(insert_stmt.?);

    // Query data
    var query_stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "SELECT id, name, email FROM users ORDER BY name",
        &query_stmt,
    );
    defer if (query_stmt) |s| wlite.wlite_stmt_finalize(s);

    while (wlite.wlite_step(query_stmt.?) == wlite.WLITE_OK) {
        const id = wlite.wlite_column_int64(query_stmt.?, 0);
        const name = wlite.wlite_column_text(query_stmt.?, 1);
        const email = wlite.wlite_column_text(query_stmt.?, 2);
        std.debug.print("{d}: {s} <{s}>\n", .{ id, name, email });
    }
}
```

## Integration

Add libwlite as a dependency in your `build.zig`:

```zig
const lib = b.addStaticLibrary("myapp", "src/main.zig");
lib.linkSystemLibrary("wlite");
lib.linkSystemLibrary("sqlite3");
```

Or compile from source:

```zig
const wlite_dep = b.addStaticLibrary("wlite", null);
wlite_dep.addCSourceFile("path/to/libwlite/wlite/schema.c", &.{"-std=c11"});
lib.linkLibrary(wlite_dep);
```

### Full build.zig example

```zig
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "myapp",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    exe.linkSystemLibrary("wlite");
    exe.linkSystemLibrary("sqlite3");
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
```

## Error handling

All libwlite functions return `wlite_result`. Check against `wlite.WLITE_OK`:

```zig
const result = wlite.wlite_open("app.db", &db);
if (result != wlite.WLITE_OK) {
    const msg = wlite.wlite_strerror(result);
    std.debug.print("Error: {s}\n", .{msg});
    return error.WliteError;
}
```

### Error handling helper

```zig
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});

fn checkResult(result: c_int) !void {
    if (result != wlite.WLITE_OK) {
        const msg = wlite.wlite_strerror(result);
        std.debug.print("wlite error: {s}\n", .{msg});
        return error.WliteError;
    }
}

fn example() !void {
    var db: ?*wlite.wlite_db = null;

    try checkResult(wlite.wlite_open("app.db", &db));
    defer if (db) |d| wlite.wlite_close(d);

    try checkResult(wlite.wlite_execute(db.?, "SELECT 1", null));
}
```

### Error codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `WLITE_OK` | Success |
| 1 | `WLITE_ERROR` | General error |
| 2 | `WLITE_NOT_FOUND` | File not found |
| 3 | `WLITE_MEMORY` | Allocation failed |
| 4 | `WLITE_IO` | I/O error |
| 5 | `WLITE_CORRUPT` | Corrupt data |
| 6 | `WLITE_RANGE` | Index out of range |

## Prepared statements

```zig
var stmt: ?*wlite.wlite_stmt = null;
try wlite.wlite_prepare(db.?, "SELECT * FROM users WHERE id = ?", &stmt);
defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

_ = wlite.wlite_bind_int64(stmt.?, 1, 42);

while (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
    const name = wlite.wlite_column_text(stmt.?, 0);
    std.debug.print("{s}\n", .{name});
}
```

### Batch inserts

```zig
fn batchInsert(db: ?*wlite.wlite_db, users: []const struct { []const u8, []const u8 }) !void {
    var stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "INSERT INTO users (name, email) VALUES (?, ?)",
        &stmt,
    );
    defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

    for (users) |user| {
        _ = wlite.wlite_bind_text(stmt.?, 1, user[0].ptr);
        _ = wlite.wlite_bind_text(stmt.?, 2, user[1].ptr);
        _ = wlite.wlite_step(stmt.?);
        _ = wlite.wlite_reset(stmt.?);
    }
}
```

### Column access methods

| Function | Return Type | Description |
|----------|-------------|-------------|
| `wlite_column_count(stmt)` | `c_int` | Number of columns in result |
| `wlite_column_name(stmt, i)` | `[*:0]const u8` | Name of column at index |
| `wlite_column_type(stmt, i)` | `c_int` | Data type of column |
| `wlite_column_int64(stmt, i)` | `i64` | Integer value |
| `wlite_column_double(stmt, i)` | `f64` | Floating point value |
| `wlite_column_text(stmt, i)` | `[*:0]const u8` | Text value |

## Transactions

```zig
var tx: ?*wlite.wlite_tx = null;
_ = wlite.wlite_begin(db.?, &tx);
defer if (tx) |t| wlite.wlite_tx_free(t);

_ = wlite.wlite_execute(db.?, "INSERT INTO users (name) VALUES ('Alice')", null);

if (error_occurred) {
    _ = wlite.wlite_rollback(tx.?);
} else {
    _ = wlite.wlite_commit(tx.?);
}
```

### Transaction with error handling

```zig
fn transferFunds(
    db: ?*wlite.wlite_db,
    from: i64,
    to: i64,
    amount: i64,
) !void {
    var tx: ?*wlite.wlite_tx = null;
    _ = wlite.wlite_begin(db.?, &tx);
    defer if (tx) |t| wlite.wlite_tx_free(t);

    // Debit sender
    var debit_stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "UPDATE accounts SET balance = balance - ? WHERE id = ?",
        &debit_stmt,
    );
    defer if (debit_stmt) |s| wlite.wlite_stmt_finalize(s);

    _ = wlite.wlite_bind_int64(debit_stmt.?, 1, amount);
    _ = wlite.wlite_bind_int64(debit_stmt.?, 2, from);
    _ = wlite.wlite_step(debit_stmt.?);

    // Credit receiver
    var credit_stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "UPDATE accounts SET balance = balance + ? WHERE id = ?",
        &credit_stmt,
    );
    defer if (credit_stmt) |s| wlite.wlite_stmt_finalize(s);

    _ = wlite.wlite_bind_int64(credit_stmt.?, 1, amount);
    _ = wlite.wlite_bind_int64(credit_stmt.?, 2, to);
    _ = wlite.wlite_step(credit_stmt.?);

    // Verify balance
    var balance_stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(
        db.?,
        "SELECT balance FROM accounts WHERE id = ?",
        &balance_stmt,
    );
    defer if (balance_stmt) |s| wlite.wlite_stmt_finalize(s);

    _ = wlite.wlite_bind_int64(balance_stmt.?, 1, from);
    if (wlite.wlite_step(balance_stmt.?) == wlite.WLITE_OK) {
        const balance = wlite.wlite_column_int64(balance_stmt.?, 0);
        if (balance < 0) {
            _ = wlite.wlite_rollback(tx.?);
            return error.InsufficientFunds;
        }
    }

    _ = wlite.wlite_commit(tx.?);
}
```

## Memory management

Use `defer` for cleanup. The Zig binding follows libwlite's ownership rules:

- Caller owns: `wlite_db`, `wlite_model`, `wlite_stmt`, `wlite_tx`
- Library owns: `wlite_table`, `wlite_field` (freed with model)

```zig
defer if (model) |m| wlite.wlite_model_free(m);
defer if (db) |d| wlite.wlite_close(d);
defer if (stmt) |s| wlite.wlite_stmt_finalize(s);
defer if (tx) |t| wlite.wlite_tx_free(t);
```

### Allocator usage

```zig
const std = @import("std");

fn processWithAllocator(allocator: std.mem.Allocator) !void {
    var model: ?*wlite.wlite_model = null;
    var db: ?*wlite.wlite_db = null;

    _ = wlite.wlite_model_load_file("app.wlite", &model);
    defer if (model) |m| wlite.wlite_model_free(m);

    _ = wlite.wlite_open("app.db", &db);
    defer if (db) |d| wlite.wlite_close(d);

    // Query and collect results
    var stmt: ?*wlite.wlite_stmt = null;
    _ = wlite.wlite_prepare(db.?, "SELECT name FROM users", &stmt);
    defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

    var names = std.ArrayList([]const u8).init(allocator);
    defer names.deinit();

    while (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
        const name = wlite.wlite_column_text(stmt.?, 0);
        try names.append(name);
    }

    for (names.items) |name| {
        std.debug.print("{s}\n", .{name});
    }
}
```

## Thread safety

Models are immutable after loading and can be shared across threads. Database connections are not thread-safe; use one per thread.

```zig
const std = @import("std");
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});

fn worker(model: ?*wlite.wlite_model, id: u32) void {
    var db: ?*wlite.wlite_db = null;
    _ = wlite.wlite_open("app.db", &db);
    defer if (db) |d| wlite.wlite_close(d);

    _ = wlite.wlite_migrate(db.?, model.?);

    for (0..100) |_| {
        var stmt: ?*wlite.wlite_stmt = null;
        _ = wlite.wlite_prepare(
            db.?,
            "INSERT INTO work_items (thread_id, data) VALUES (?, ?)",
            &stmt,
        );
        defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

        _ = wlite.wlite_bind_int64(stmt.?, 1, @intCast(id));
        _ = wlite.wlite_bind_text(stmt.?, 2, "item");
        _ = wlite.wlite_step(stmt.?);
    }
}

pub fn main() !void {
    var model: ?*wlite.wlite_model = null;
    _ = wlite.wlite_model_load_file("app.wlite", &model);
    defer if (model) |m| wlite.wlite_model_free(m);

    var threads: [4]std.Thread = undefined;

    for (0..4) |i| {
        threads[i] = try std.Thread.spawn(.{}, worker, .{ model, @intCast(i) });
    }

    for (threads) |t| {
        t.join();
    }
}
```

## Complete example

Here is a complete, working program that demonstrates all major features:

```zig
const std = @import("std");
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});

const User = struct {
    id: i64,
    name: []const u8,
    email: []const u8,
    active: bool,
};

const UserDatabase = struct {
    model: ?*wlite.wlite_model,
    db: ?*wlite.wlite_db,

    fn init(model_path: []const u8, db_path: []const u8) !UserDatabase {
        var model: ?*wlite.wlite_model = null;
        var db: ?*wlite.wlite_db = null;

        const model_result = wlite.wlite_model_load_file(model_path.ptr, &model);
        if (model_result != wlite.WLITE_OK) {
            return error.WliteError;
        }

        const open_result = wlite.wlite_open(db_path.ptr, &db);
        if (open_result != wlite.WLITE_OK) {
            if (model) |m| wlite.wlite_model_free(m);
            return error.WliteError;
        }

        const migrate_result = wlite.wlite_migrate(db.?, model.?);
        if (migrate_result != wlite.WLITE_OK) {
            if (db) |d| wlite.wlite_close(d);
            if (model) |m| wlite.wlite_model_free(m);
            return error.WliteError;
        }

        return UserDatabase{ .model = model, .db = db };
    }

    fn deinit(self: *UserDatabase) void {
        if (self.db) |d| wlite.wlite_close(d);
        if (self.model) |m| wlite.wlite_model_free(m);
    }

    fn createTables(self: *UserDatabase) !void {
        const sql =
            \\CREATE TABLE IF NOT EXISTS users (
            \\    id INTEGER PRIMARY KEY,
            \\    name TEXT NOT NULL,
            \\    email TEXT NOT NULL UNIQUE,
            \\    active INTEGER DEFAULT 1,
            \\    created_at TEXT DEFAULT (datetime('now'))
            \\)
        ;
        const result = wlite.wlite_execute(self.db.?, sql.ptr, null);
        if (result != wlite.WLITE_OK) return error.WliteError;
    }

    fn insertUser(self: *UserDatabase, name: []const u8, email: []const u8) !void {
        var stmt: ?*wlite.wlite_stmt = null;
        const prepare_result = wlite.wlite_prepare(
            self.db.?,
            "INSERT INTO users (name, email) VALUES (?, ?)",
            &stmt,
        );
        if (prepare_result != wlite.WLITE_OK) return error.WliteError;
        defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

        _ = wlite.wlite_bind_text(stmt.?, 1, name.ptr);
        _ = wlite.wlite_bind_text(stmt.?, 2, email.ptr);

        const step_result = wlite.wlite_step(stmt.?);
        if (step_result != wlite.WLITE_OK) return error.WliteError;
    }

    fn listUsers(self: *UserDatabase, allocator: std.mem.Allocator) !std.ArrayList(User) {
        var stmt: ?*wlite.wlite_stmt = null;
        const prepare_result = wlite.wlite_prepare(
            self.db.?,
            "SELECT id, name, email, active FROM users ORDER BY name",
            &stmt,
        );
        if (prepare_result != wlite.WLITE_OK) return error.WliteError;
        defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

        var users = std.ArrayList(User).init(allocator);
        errdefer users.deinit();

        while (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
            const user = User{
                .id = wlite.wlite_column_int64(stmt.?, 0),
                .name = std.mem.sliceTo(wlite.wlite_column_text(stmt.?, 1), 0),
                .email = std.mem.sliceTo(wlite.wlite_column_text(stmt.?, 2), 0),
                .active = wlite.wlite_column_int64(stmt.?, 3) != 0,
            };
            try users.append(user);
        }

        return users;
    }

    fn countUsers(self: *UserDatabase) !i64 {
        var stmt: ?*wlite.wlite_stmt = null;
        const prepare_result = wlite.wlite_prepare(
            self.db.?,
            "SELECT COUNT(*) FROM users",
            &stmt,
        );
        if (prepare_result != wlite.WLITE_OK) return error.WliteError;
        defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

        if (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
            return wlite.wlite_column_int64(stmt.?, 0);
        }

        return 0;
    }

    fn searchUsers(self: *UserDatabase, pattern: []const u8, allocator: std.mem.Allocator) !std.ArrayList(User) {
        var stmt: ?*wlite.wlite_stmt = null;
        const prepare_result = wlite.wlite_prepare(
            self.db.?,
            "SELECT id, name, email, active FROM users WHERE name LIKE ?",
            &stmt,
        );
        if (prepare_result != wlite.WLITE_OK) return error.WliteError;
        defer if (stmt) |s| wlite.wlite_stmt_finalize(s);

        var buf: [256]u8 = undefined;
        const like_pattern = std.fmt.bufPrint(&buf, "%{s}%", .{pattern}) catch return error.WliteError;
        _ = wlite.wlite_bind_text(stmt.?, 1, like_pattern.ptr);

        var users = std.ArrayList(User).init(allocator);
        errdefer users.deinit();

        while (wlite.wlite_step(stmt.?) == wlite.WLITE_OK) {
            const user = User{
                .id = wlite.wlite_column_int64(stmt.?, 0),
                .name = std.mem.sliceTo(wlite.wlite_column_text(stmt.?, 1), 0),
                .email = std.mem.sliceTo(wlite.wlite_column_text(stmt.?, 2), 0),
                .active = wlite.wlite_column_int64(stmt.?, 3) != 0,
            };
            try users.append(user);
        }

        return users;
    }
};

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var db = try UserDatabase.init("app.wlite", "app.db");
    defer db.deinit();

    try db.createTables();

    try db.insertUser("Alice", "alice@example.com");
    try db.insertUser("Bob", "bob@example.com");
    try db.insertUser("Charlie", "charlie@example.com");

    const count = try db.countUsers();
    std.debug.print("Total users: {d}\n\n", .{count});

    std.debug.print("All users:\n", .{});
    var users = try db.listUsers(allocator);
    defer users.deinit();

    for (users.items) |user| {
        const active_str = if (user.active) "" else " [inactive]";
        std.debug.print("  {d}: {s} <{s}>{s}\n", .{ user.id, user.name, user.email, active_str });
    }

    std.debug.print("\nSearch results for 'Ali':\n", .{});
    var search_results = try db.searchUsers("Ali", allocator);
    defer search_results.deinit();

    for (search_results.items) |user| {
        std.debug.print("  {s}\n", .{user.name});
    }
}
```
