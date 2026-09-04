---
title: Zig Binding
description: Zig C interop binding for wlite.
---

# Zig Binding

The Zig binding uses `@cImport` to directly interface with libwlite's C ABI. It provides Zig-idiomatic error handling and memory management.

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
