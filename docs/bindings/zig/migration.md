---
title: Migration in the Zig Binding
description: Model loading, database opening, schema migration, diff, plan, check, snapshot, hash, and compiled models.
---

# Migration

Migration in wlite is driven by a schema model. The model defines the desired
state of the database. The library compares the model against the live schema,
computes a diff, and applies the necessary DDL statements. The Zig binding wraps
all of these steps with Zig-idiomatic types and error handling.

## Model Loading

A model is a parsed representation of a `.wlite` schema file. Load one from a
file path or from an in-memory byte buffer.

### From a file

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    var model = try wlite.Model.load("schema.wlite");
    defer model.deinit();

    std.debug.print("Tables: {d}\n", .{model.tableCount()});
}
```

`Model.load` calls `wlite_model_load_file` under the hood. If the file does not
exist or contains invalid syntax, it returns an error.

### From memory

If you already have the schema bytes (for example, embedded in your binary or
fetched from a network resource), use `Model.fromBytes`.

```zig
const schema_bytes = @embedFile("schema.wlite");
var model = try wlite.Model.fromBytes(schema_bytes);
defer model.deinit();
```

### Model validation

You can validate a model without opening a database. This is useful for
checking schema syntax early.

```zig
var model = try wlite.Model.load("schema.wlite");
defer model.deinit();

try model.validate();
```

## Database Opening

Open a database with `Database.open` or `Database.openMemory`.

### File-backed database

```zig
var db = try wlite.Database.open("app.db");
defer db.deinit();
```

### In-memory database

In-memory databases are useful for testing. They exist only for the lifetime of
the connection.

```zig
var db = try wlite.Database.openMemory();
defer db.deinit();
```

## Running Migrations

The `migrate` method on `Database` compares the model against the current
database schema and applies any necessary changes.

```zig
var model = try wlite.Model.load("schema.wlite");
defer model.deinit();

var db = try wlite.Database.open("app.db");
defer db.deinit();

try db.migrate(model);
```

Under the hood, `migrate` calls `wlite_diff` with a null plan callback. This
means all differences are applied automatically. If you want to inspect the
planned changes before applying them, use the plan callback approach described
below.

### Migration is idempotent

Running `migrate` multiple times with the same model and database is safe. If the
database already matches the model, no changes are made.

```zig
// First run: creates tables
try db.migrate(model);

// Second run: no changes
try db.migrate(model);
```

## Schema Diff

The diff operation compares a model against the live database and produces a list
of differences. The Zig binding exposes this through the `migrate` method, but
you can also call the C function directly for more control.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

const result = c.wlite_diff(db.ptr, model.ptr, null);
if (result != c.WLITE_OK) {
    const msg = c.wlite_strerror(result);
    std.debug.print("diff failed: {s}\n", .{msg});
}
```

Passing a null callback applies all changes. To intercept the planned changes, see
the plan section below.

## Plan

A plan callback lets you inspect each migration step before it is executed. This
is useful for logging, auditing, or selectively applying changes.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

fn planCallback(step: [*:0]const u8, arg: ?*anyopaque) callconv(.C) void {
    _ = arg;
    const step_str = std.mem.span(step);
    std.debug.print("Migration step: {s}\n", .{step_str});
}

const result = c.wlite_plan(db.ptr, model.ptr, planCallback, null);
if (result != c.WLITE_OK) {
    const msg = c.wlite_strerror(result);
    std.debug.print("plan failed: {s}\n", .{msg});
}
```

The callback receives each DDL statement as a null-terminated C string. You can
log it, store it, or skip it by returning without applying.

### Plan with user data

Pass an opaque pointer through the callback for context.

```zig
const LogContext = struct {
    count: usize,
};

fn planCallback(step: [*:0]const u8, arg: ?*anyopaque) callconv(.C) void {
    const ctx: *LogContext = @ptrCast(@alignCast(arg orelse return));
    ctx.count += 1;
    const step_str = std.mem.span(step);
    std.debug.print("[{d}] {s}\n", .{ ctx.count, step_str });
}

var ctx = LogContext{ .count = 0 };
const result = c.wlite_plan(db.ptr, model.ptr, planCallback, &ctx);
```

## Check

The check operation verifies that the database schema matches the model without
making any changes. This is useful for validation in CI pipelines or startup
checks.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

const result = c.wlite_check(db.ptr, model.ptr);
if (result == c.WLITE_OK) {
    std.debug.print("Schema is up to date\n", .{});
} else {
    std.debug.print("Schema drift detected\n", .{});
}
```

### Check in tests

Use check in your test suite to verify that migrations produce the expected
schema.

```zig
test "schema matches model" {
    var model = try wlite.Model.load("schema.wlite");
    defer model.deinit();

    var db = try wlite.Database.openMemory();
    defer db.deinit();

    try db.migrate(model);

    const c = @cImport({
        @cInclude("wlite/wlite.h"),
    });

    const result = c.wlite_check(db.ptr, model.ptr);
    try std.testing.expectEqual(c.WLITE_OK, result);
}
```

## Snapshot

A snapshot captures the current state of the schema for comparison or rollback.
This is useful for detecting drift between environments.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

var snapshot: ?*c.wlite_snapshot = null;
const result = c.wlite_snapshot(db.ptr, &snapshot);
if (result != c.WLITE_OK) {
    const msg = c.wlite_strerror(result);
    std.debug.print("snapshot failed: {s}\n", .{msg});
    return;
}
defer if (snapshot) |s| c.wlite_snapshot_free(s);

// Compare snapshot against a model
const cmp = c.wlite_snapshot_diff(snapshot.?, model.ptr);
if (cmp == c.WLITE_OK) {
    std.debug.print("Snapshot matches model\n", .{});
}
```

## Schema Hash

The hash function computes a fingerprint of the current database schema. Use it
to quickly check whether the schema has changed without running a full diff.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

var hash_buf: [65]u8 = undefined; // SHA-256 hex string + null
const result = c.wlite_hash(db.ptr, &hash_buf, hash_buf.len);
if (result == c.WLITE_OK) {
    std.debug.print("Schema hash: {s}\n", .{std.mem.span(@as([*:0]const u8, @ptrCast(&hash_buf)))});
}
```

### Comparing hashes across databases

```zig
fn schemasMatch(db1: wlite.Database, db2: wlite.Database) bool {
    var h1: [65]u8 = undefined;
    var h2: [65]u8 = undefined;

    const c = @cImport({
        @cInclude("wlite/wlite.h"),
    });

    const r1 = c.wlite_hash(db1.ptr, &h1, h1.len);
    const r2 = c.wlite_hash(db2.ptr, &h2, h2.len);

    if (r1 != c.WLITE_OK or r2 != c.WLITE_OK) return false;
    return std.mem.eql(u8, std.span(@as([*:0]const u8, @ptrCast(&h1))), std.span(@as([*:0]const u8, @ptrCast(&h2))));
}
```

## Compiled Models

A compiled model is a pre-parsed schema that can be loaded faster than parsing
raw text. This is useful for large schemas or for embedding the schema in a
binary.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

// Compile a model from a file
var compiled: ?*c.wlite_model = null;
const result = c.wlite_model_compile("schema.wlite", &compiled);
if (result != c.WLITE_OK) {
    const msg = c.wlite_strerror(result);
    std.debug.print("compile failed: {s}\n", .{msg});
    return;
}
defer if (compiled) |m| c.wlite_model_free(m);

// Use the compiled model for migration
var db = try wlite.Database.open("app.db");
defer db.deinit();

try db.migrate(.{ .ptr = compiled.? });
```

### Compiled model from bytes

You can also compile a model from an in-memory buffer.

```zig
const schema_bytes = @embedFile("schema.wlite");
var compiled: ?*c.wlite_model = null;
const result = c.wlite_model_compile_memory(schema_bytes.ptr, schema_bytes.len, &compiled);
if (result != c.WLITE_OK) {
    return error.WliteError;
}
defer if (compiled) |m| c.wlite_model_free(m);
```

## Complete Migration Workflow

The following example demonstrates a full migration workflow: load the model,
open the database, run migrations, verify the schema, and insert initial data.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    // Load the schema model
    var model = try wlite.Model.load("schema.wlite");
    defer model.deinit();

    // Open the database
    var db = try wlite.Database.open("app.db");
    defer db.deinit();

    // Run migrations (idempotent)
    try db.migrate(model);

    // Verify schema matches model
    const c = @cImport({
        @cInclude("wlite/wlite.h"),
    });

    const check_result = c.wlite_check(db.ptr, model.ptr);
    if (check_result != c.WLITE_OK) {
        std.debug.print("Schema drift detected after migration\n", .{});
        return error.WliteError;
    }

    // Insert seed data
    try db.execute(
        \\INSERT OR IGNORE INTO users (name, email) VALUES ('admin', 'admin@example.com')
    );

    std.debug.print("Migration complete\n", .{});
}
```

## Error Handling in Migration

All migration functions return `Error!void`. Use `try` to propagate errors, or
catch them explicitly to handle specific failure modes.

```zig
fn migrateWithRecovery(model: wlite.Model, db: wlite.Database) !void {
    db.migrate(model) catch |err| {
        switch (err) {
            error.ParseError => {
                std.debug.print("Invalid schema file\n", .{});
                return error.WliteError;
            },
            error.SqliteError => {
                std.debug.print("SQLite error during migration\n", .{});
                return error.WliteError;
            },
            else => return err,
        }
    };
}
```

## Thread Safety for Migration

Models are immutable after loading and can be shared across threads. Each thread
should open its own database connection before calling migrate.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

fn worker(model: wlite.Model, id: u32) void {
    var db = wlite.Database.open("app.db") catch return;
    defer db.deinit();

    db.migrate(model) catch return;

    std.debug.print("Thread {d}: migration complete\n", .{id});
}

pub fn main() !void {
    var model = try wlite.Model.load("schema.wlite");
    defer model.deinit();

    var threads: [4]std.Thread = undefined;
    for (0..4) |i| {
        threads[i] = try std.Thread.spawn(.{}, worker, .{ model, @intCast(i) });
    }
    for (threads) |t| {
        t.join();
    }
}
```

## Next Steps

- [Queries](queries.md) covers prepared statements, bindings, and transactions
- [Errors](errors.md) covers error types, cleanup, and thread safety
