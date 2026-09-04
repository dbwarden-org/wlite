---
title: Zig Binding Overview
description: Getting started with the wlite Zig binding, installation, types, and patterns.
---

# Zig Binding

The wlite Zig binding provides a thin, idiomatic wrapper around the libwlite C ABI.
It does not reimplement wlite semantics. Instead, it exposes the raw C functions through
Zig's `@cImport` facility and wraps them in Zig-native types with comptime error
checking, optional pointer semantics, and `defer`-based resource management.

## Feature Summary

- Direct C interop via `@cImport` and `@cInclude`
- Zig error unions mapped from `wlite_result` codes
- Optional pointer types for nullable handles
- `defer` and `errdefer` for deterministic cleanup
- Zero-cost abstraction over the C API
- Compatible with Zig's allocator and standard library

## Prerequisites

You need:

- A Zig toolchain (0.11 or later recommended)
- libwlite built and installed (static or shared)
- SQLite development headers available to the linker

If libwlite is installed system-wide, the Zig build system can find it via
`linkSystemLibrary`. Otherwise, point the build at the libwlite source tree.

## Installation

### Using the binding module

Add the `bindings/zig/src/wlite.zig` file to your project, or reference it as a
module. The binding file is self-contained and depends only on the C headers being
available at compile time.

### build.zig integration

The recommended approach is to add libwlite as a system library dependency in your
`build.zig` file. The following example shows a minimal build script that links
against libwlite and SQLite.

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

### Linking from source

If libwlite is not installed system-wide, you can compile it from source and link
the resulting static library into your Zig project.

```zig
const wlite_dep = b.addStaticLibrary("wlite", null);
wlite_dep.addCSourceFile("path/to/libwlite/wlite/schema.c", &.{"-std=c11"});
lib.linkLibrary(wlite_dep);
```

### Adding include paths

If the wlite headers are not in a standard location, add the include directory
explicitly.

```zig
exe.addIncludePath(.{ .path = "path/to/libwlite/include" });
```

### Using @cImport

The binding file uses `@cImport` to pull in the C declarations at comptime. You
do not need to write `@cImport` calls yourself if you use the provided
`wlite.zig` module. However, if you prefer a direct approach, you can call
`@cImport` in your own code.

```zig
const std = @import("std");
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});
```

This imports all C types and functions from the wlite header into the `wlite`
namespace. You can then call `wlite.wlite_open`, `wlite.wlite_prepare`, and so on.

## Quick Start

The following program opens an in-memory database, creates a table, inserts a
row, and reads it back. It demonstrates the core pattern used throughout the
binding.

```zig
const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    var db = try wlite.Database.openMemory();
    defer db.deinit();

    try db.execute("CREATE TABLE greeting (msg TEXT)");
    try db.execute("INSERT INTO greeting VALUES ('hello')");

    var stmt = try db.prepare("SELECT msg FROM greeting");
    defer stmt.deinit();

    while (try stmt.step()) {
        const msg = stmt.columnText(0);
        std.debug.print("{s}\n", .{msg});
    }
}
```

The `defer db.deinit()` call ensures the database is closed when the function
returns, regardless of whether it returns normally or with an error.

## Types

### Optional Pointers

wlite handles are modeled as optional pointers. In the C API, a pointer parameter
may be null on failure. The Zig binding wraps this pattern using Zig's optional
pointer types.

A type like `?*wlite.wlite_db` is an optional pointer to a database handle. After
a successful call, the pointer is guaranteed non-null. The binding converts this
into Zig error unions so you never need to check for null manually.

```zig
// C style: you must check for null
var db: ?*wlite.wlite_db = null;
if (wlite.wlite_open("app.db", &db) == wlite.WLITE_OK) {
    if (db) |d| {
        // use d
    }
}

// Zig binding style: error union handles the check
var db = try wlite.Database.open("app.db");
defer db.deinit();
// db.ptr is guaranteed non-null here
```

### Model

The `Model` type wraps `*wlite_model`. It represents a parsed schema definition.
Models are immutable after loading and can be shared across threads.

```zig
var model = try wlite.Model.load("app.wlite");
defer model.deinit();

std.debug.print("Tables: {d}\n", .{model.tableCount()});
```

### Database

The `Database` type wraps `*wlite_db`. It represents an open SQLite connection
managed by libwlite.

```zig
var db = try wlite.Database.open("app.db");
defer db.deinit();
```

### Statement

The `Statement` type wraps `*wlite_stmt`. It represents a prepared SQL statement.

```zig
var stmt = try db.prepare("SELECT * FROM users");
defer stmt.deinit();
```

### Transaction

The `Transaction` type wraps `*wlite_tx`. It represents an active database
transaction.

```zig
var tx = try wlite.Transaction.begin(db);
defer tx.deinit();
```

## The defer Pattern

Zig's `defer` statement runs an expression when the enclosing scope exits. This
is the primary mechanism for resource cleanup in the Zig binding. Every handle
type has a `deinit` method that frees the underlying C resource.

```zig
{
    var db = try wlite.Database.open("app.db");
    defer db.deinit(); // runs when this block exits

    var stmt = try db.prepare("SELECT 1");
    defer stmt.deinit(); // runs when this block exits

    _ = try stmt.step();
}
// stmt.deinit() runs first, then db.deinit()
```

The order of `defer` statements is reverse order of declaration, matching stack
unwinding semantics. This means statements are finalized before the database is
closed.

### errdefer

Use `errdefer` to run cleanup only when the function returns with an error. This
is useful when you want to release resources on failure but keep them alive on
success.

```zig
fn setup() !wlite.Database {
    var db = try wlite.Database.open("app.db");
    errdefer db.deinit();

    try db.execute("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY)");
    // If execute fails, db is closed. If we reach the return, db stays open.
    return db;
}
```

### Nested Cleanup

When multiple resources are allocated in sequence, each `defer` adds to the
cleanup stack. The binding relies on this ordering to prevent use-after-free.

```zig
fn fullWorkflow() !void {
    var model = try wlite.Model.load("schema.wlite");
    defer model.deinit();

    var db = try wlite.Database.open("data.db");
    defer db.deinit();

    try db.migrate(model);

    var stmt = try db.prepare("SELECT COUNT(*) FROM users");
    defer stmt.deinit();

    if (try stmt.step()) {
        const count = stmt.columnInt64(0);
        std.debug.print("Users: {d}\n", .{count});
    }
}
```

## Raw C API Access

You can bypass the Zig wrapper and call the C functions directly when needed. This
is useful for functions not yet wrapped by the binding.

```zig
const c = @cImport({
    @cInclude("wlite/wlite.h"),
});

const result = c.wlite_wal_checkpoint(db.ptr, "main");
if (result != c.WLITE_OK) {
    std.debug.print("checkpoint failed\n", .{});
}
```

## Compile-Time Checks

The `@cImport` call runs at comptime. If the wlite headers are missing or
malformed, you get a compile error rather than a runtime crash. This catches
linking problems early.

```zig
// This will fail at compile time if wlite.h is not found
const wlite = @cImport({
    @cInclude("wlite/wlite.h");
});
```

## Memory Ownership Rules

The binding follows libwlite's ownership conventions:

- Caller owns `wlite_db`, `wlite_model`, `wlite_stmt`, `wlite_tx`
- Library owns `wlite_table`, `wlite_field` (freed when model is freed)
- Text pointers returned by `wlite_column_text` are valid until the next
  `wlite_step` or `wlite_stmt_finalize`

Always copy text values if you need them to outlive the step call.

```zig
var name_buf: [128]u8 = undefined;
const name_ptr = stmt.columnText(0);
const name = std.mem.span(name_ptr);
const owned = std.mem.copy(u8, &name_buf, name);
```

## Thread Safety

Models are immutable after loading and can be shared across threads. Database
connections are not thread-safe. Use one connection per thread, or serialize
access with a mutex.

```zig
const model = try wlite.Model.load("app.wlite");
defer model.deinit();

// Each thread gets its own database connection
const thread1 = try std.Thread.spawn(.{}, worker, .{model});
const thread2 = try std.Thread.spawn(.{}, worker, .{model});

thread1.join();
thread2.join();

fn worker(model: wlite.Model) void {
    var db = wlite.Database.open("app.db") catch return;
    defer db.deinit();
    db.migrate(model) catch return;
}
```

## Error Handling Summary

Every function that can fail returns `Error!T`. Use `try` to propagate errors, or
catch them explicitly with `if` or `switch`.

```zig
var db = wlite.Database.open("app.db") catch |err| {
    std.debug.print("open failed: {}\n", .{err});
    return;
};
defer db.deinit();
```

See the dedicated [errors](errors.md) page for complete coverage of error codes
and handling patterns.

## Next Steps

- [Migration](migration.md) covers model loading, schema diffing, and migration
- [Queries](queries.md) covers prepared statements, bindings, and transactions
- [Errors](errors.md) covers error types, cleanup, and thread safety
