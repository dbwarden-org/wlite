const std = @import("std");
const wlite = @import("src/wlite.zig");

pub fn main() !void {
    var db = try wlite.Database.openMemory();
    defer db.deinit();
    try db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)");
    try db.execute("INSERT INTO t VALUES (1, 'hello')");
    var stmt = try db.prepare("SELECT * FROM t");
    defer stmt.deinit();
    while (try stmt.step()) {
        std.debug.print("row: {d} {s}\n", .{ stmt.columnInt64(0), stmt.columnText(0) });
    }
    std.debug.print("Zig binding: OK\n", .{});
}
