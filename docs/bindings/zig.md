---
title: Zig Binding
description: Zig C interop binding for wlite. Use .wlite schemas from Zig.
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
```

## Error Handling

All libwlite functions return `wlite_result`. Check against `wlite.WLITE_OK`:

```zig
const result = wlite.wlite_open("app.db", &db);
if (result != wlite.WLITE_OK) {
    return error.WliteError;
}
```

## Memory Management

Use `defer` for cleanup:

```zig
defer if (model) |m| wlite.wlite_model_free(m);
defer if (db) |d| wlite.wlite_close(d);
```
