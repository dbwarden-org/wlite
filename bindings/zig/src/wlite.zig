/// wlite — Zig binding for libwlite
///
/// This module provides a thin, idiomatic Zig wrapper around the libwlite C ABI.
/// It does not reimplement any WLite semantics.
const c = @cImport({
    @cInclude("wlite.h"),
});

// ── Error ──────────────────────────────────────────────────────────────

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

// ── Version ────────────────────────────────────────────────────────────

pub fn version() []const u8 {
    return std.mem.span(c.wlite_version());
}

pub fn abiVersion() c_int {
    return c.wlite_abi_version();
}

// ── Model ──────────────────────────────────────────────────────────────

pub const Model = struct {
    ptr: *c.wlite_model,

    pub fn load(path: [*:0]const u8) Error!Model {
        var ptr: ?*c.wlite_model = null;
        try checkResult(c.wlite_model_load_file(path, &ptr));
        return .{ .ptr = ptr.? };
    }

    pub fn fromBytes(data: []const u8) Error!Model {
        var ptr: ?*c.wlite_model = null;
        try checkResult(c.wlite_model_load_memory(data.ptr, data.len, &ptr));
        return .{ .ptr = ptr.? };
    }

    pub fn validate(self: Model) Error!void {
        try checkResult(c.wlite_model_validate(self.ptr));
    }

    pub fn tableCount(self: Model) usize {
        return c.wlite_model_table_count(self.ptr);
    }

    pub fn deinit(self: *Model) void {
        c.wlite_model_free(self.ptr);
    }
};

// ── Database ───────────────────────────────────────────────────────────

pub const Database = struct {
    ptr: *c.wlite_db,

    pub fn open(path: [*:0]const u8) Error!Database {
        var ptr: ?*c.wlite_db = null;
        try checkResult(c.wlite_open(path, &ptr));
        return .{ .ptr = ptr.? };
    }

    pub fn openMemory() Error!Database {
        var ptr: ?*c.wlite_db = null;
        try checkResult(c.wlite_open(":memory:", &ptr));
        return .{ .ptr = ptr.? };
    }

    pub fn deinit(self: *Database) void {
        c.wlite_close(self.ptr);
    }

    pub fn execute(self: Database, sql: [*:0]const u8) Error!void {
        try checkResult(c.wlite_execute(self.ptr, sql, null));
    }

    pub fn prepare(self: Database, sql: [*:0]const u8) Error!Statement {
        var ptr: ?*c.wlite_stmt = null;
        try checkResult(c.wlite_prepare(self.ptr, sql, &ptr));
        return .{ .stmt = ptr.? };
    }

    pub fn migrate(self: Database, model: Model) Error!void {
        try checkResult(c.wlite_diff(self.ptr, model.ptr, null));
    }
};

// ── Statement ──────────────────────────────────────────────────────────

pub const Statement = struct {
    stmt: *c.wlite_stmt,

    pub fn bindInt64(self: Statement, index: c_int, value: i64) Error!void {
        try checkResult(c.wlite_bind_int64(self.stmt, index, value));
    }

    pub fn bindDouble(self: Statement, index: c_int, value: f64) Error!void {
        try checkResult(c.wlite_bind_double(self.stmt, index, value));
    }

    pub fn bindText(self: Statement, index: c_int, value: [*:0]const u8) Error!void {
        try checkResult(c.wlite_bind_text(self.stmt, index, value));
    }

    pub fn bindNull(self: Statement, index: c_int) Error!void {
        try checkResult(c.wlite_bind_null(self.stmt, index));
    }

    pub fn step(self: Statement) Error!bool {
        const r = c.wlite_step(self.stmt);
        if (r == c.WLITE_NOT_FOUND) return false;
        try checkResult(r);
        return true;
    }

    pub fn columnCount(self: Statement) c_int {
        return c.wlite_column_count(self.stmt);
    }

    pub fn columnName(self: Statement, index: c_int) [*:0]const u8 {
        return c.wlite_column_name(self.stmt, index);
    }

    pub fn columnInt64(self: Statement, index: c_int) i64 {
        return c.wlite_column_int64(self.stmt, index);
    }

    pub fn columnDouble(self: Statement, index: c_int) f64 {
        return c.wlite_column_double(self.stmt, index);
    }

    pub fn columnText(self: Statement, index: c_int) [*:0]const u8 {
        return c.wlite_column_text(self.stmt, index);
    }

    pub fn deinit(self: *Statement) void {
        c.wlite_stmt_finalize(self.stmt);
    }
};

// ── Transaction ────────────────────────────────────────────────────────

pub const Transaction = struct {
    tx: *c.wlite_tx,

    pub fn begin(db: Database) Error!Transaction {
        var ptr: ?*c.wlite_tx = null;
        try checkResult(c.wlite_begin(db.ptr, &ptr));
        return .{ .tx = ptr.? };
    }

    pub fn commit(self: *Transaction) Error!void {
        try checkResult(c.wlite_commit(self.tx));
    }

    pub fn rollback(self: *Transaction) Error!void {
        try checkResult(c.wlite_rollback(self.tx));
    }

    pub fn savepoint(self: Transaction, name: [*:0]const u8) Error!void {
        try checkResult(c.wlite_savepoint(self.tx, name));
    }

    pub fn release(self: Transaction, name: [*:0]const u8) Error!void {
        try checkResult(c.wlite_release(self.tx, name));
    }

    pub fn rollbackTo(self: Transaction, name: [*:0]const u8) Error!void {
        try checkResult(c.wlite_rollback_to(self.tx, name));
    }

    pub fn deinit(self: *Transaction) void {
        c.wlite_tx_free(self.tx);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "database open and execute" {
    var db = try Database.openMemory();
    defer db.deinit();

    try db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)");
    try db.execute("INSERT INTO t VALUES (1, 'hello')");

    var stmt = try db.prepare("SELECT * FROM t");
    defer stmt.deinit();

    if (try stmt.step()) {
        try std.testing.expectEqual(@as(i64, 1), stmt.columnInt64(0));
        const val = stmt.columnText(0);
        _ = val;
    }
}

test "transaction with savepoint" {
    var db = try Database.openMemory();
    defer db.deinit();

    try db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)");

    var tx = try Transaction.begin(db);
    try tx.savepoint("sp1");
    try db.execute("INSERT INTO t VALUES (1)");
    try tx.rollbackTo("sp1");
    try tx.release("sp1");
    try tx.commit();
}
