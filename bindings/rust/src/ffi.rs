use std::os::raw::{c_char, c_int, c_void};

pub type WliteResult = c_int;

pub const WLITE_OK: WliteResult = 0;
pub const WLITE_ERROR: WliteResult = 1;
pub const WLITE_INVALID_ARGUMENT: WliteResult = 2;
pub const WLITE_OUT_OF_MEMORY: WliteResult = 3;
pub const WLITE_IO_ERROR: WliteResult = 4;
pub const WLITE_PARSE_ERROR: WliteResult = 5;
pub const WLITE_MODEL_ERROR: WliteResult = 6;
pub const WLITE_SQLITE_ERROR: WliteResult = 7;
pub const WLITE_CONSTRAINT_ERROR: WliteResult = 8;
pub const WLITE_NOT_FOUND: WliteResult = 9;
pub const WLITE_BUSY: WliteResult = 10;
pub const WLITE_TRANSACTION_ERROR: WliteResult = 11;

pub const WLITE_VALUE_NULL: i32 = 0;
pub const WLITE_VALUE_INTEGER: i32 = 1;
pub const WLITE_VALUE_REAL: i32 = 2;
pub const WLITE_VALUE_TEXT: i32 = 3;
pub const WLITE_VALUE_BLOB: i32 = 4;

#[repr(C)]
pub struct WliteDb {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteModel {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteStmt {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteTx {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteRecord {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteError {
    pub code: WliteResult,
    pub message: *mut c_char,
    pub subsystem: *mut c_char,
    pub object: *mut c_char,
    pub sqlite_code: c_int,
    pub line: c_int,
}

#[repr(C)]
pub struct WlitePlan {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteDiff {
    _private: [u8; 0],
}

extern "C" {
    pub fn wlite_abi_version() -> c_int;
    pub fn wlite_version() -> *const c_char;
    pub fn wlite_strerror(result: WliteResult) -> *const c_char;
    pub fn wlite_error_free(err: *mut WliteError);

    pub fn wlite_open(path: *const c_char, out: *mut *mut WliteDb) -> WliteResult;
    pub fn wlite_close(db: *mut WliteDb);
    pub fn wlite_execute(db: *mut WliteDb, sql: *const c_char, rows_affected: *mut i64) -> WliteResult;

    pub fn wlite_model_load_file(path: *const c_char, out: *mut *mut WliteModel) -> WliteResult;
    pub fn wlite_model_load_memory(data: *const c_void, size: usize, out: *mut *mut WliteModel) -> WliteResult;
    pub fn wlite_model_free(model: *mut WliteModel);
    pub fn wlite_model_validate(model: *const WliteModel) -> WliteResult;
    pub fn wlite_model_table_count(model: *const WliteModel) -> usize;
    pub fn wlite_model_table(model: *const WliteModel, name: *const c_char) -> *const WliteTable;

    pub fn wlite_prepare(db: *mut WliteDb, sql: *const c_char, out: *mut *mut WliteStmt) -> WliteResult;
    pub fn wlite_bind_int64(stmt: *mut WliteStmt, index: c_int, value: i64) -> WliteResult;
    pub fn wlite_bind_double(stmt: *mut WliteStmt, index: c_int, value: f64) -> WliteResult;
    pub fn wlite_bind_text(stmt: *mut WliteStmt, index: c_int, value: *const c_char) -> WliteResult;
    pub fn wlite_bind_null(stmt: *mut WliteStmt, index: c_int) -> WliteResult;
    pub fn wlite_step(stmt: *mut WliteStmt) -> WliteResult;
    pub fn wlite_stmt_finalize(stmt: *mut WliteStmt);
    pub fn wlite_column_count(stmt: *mut WliteStmt) -> c_int;
    pub fn wlite_column_name(stmt: *mut WliteStmt, column: c_int) -> *const c_char;
    pub fn wlite_column_type(stmt: *mut WliteStmt, column: c_int) -> c_int;
    pub fn wlite_column_int64(stmt: *mut WliteStmt, column: c_int) -> i64;
    pub fn wlite_column_double(stmt: *mut WliteStmt, column: c_int) -> f64;
    pub fn wlite_column_text(stmt: *mut WliteStmt, column: c_int) -> *const c_char;
    pub fn wlite_column_bytes(stmt: *mut WliteStmt, column: c_int) -> usize;

    pub fn wlite_begin(db: *mut WliteDb, out: *mut *mut WliteTx) -> WliteResult;
    pub fn wlite_commit(tx: *mut WliteTx) -> WliteResult;
    pub fn wlite_rollback(tx: *mut WliteTx) -> WliteResult;
    pub fn wlite_tx_free(tx: *mut WliteTx);

    pub fn wlite_diff(db: *mut WliteDb, model: *const WliteModel, out: *mut *mut WlitePlan) -> WliteResult;
    pub fn wlite_migrate(db: *mut WliteDb, model: *const WliteModel) -> WliteResult;
    pub fn wlite_plan_count(plan: *const WlitePlan) -> usize;
    pub fn wl_plan_free(plan: *mut WlitePlan);
    pub fn wl_schema_hash(schema: *const WliteSchema) -> *mut c_char;
    pub fn wlite_free(ptr: *mut c_void);
}

#[repr(C)]
pub struct WliteTable {
    _private: [u8; 0],
}

extern "C" {
    pub fn wlite_table_name(table: *const WliteTable) -> *const c_char;
    pub fn wlite_table_sql_name(table: *const WliteTable) -> *const c_char;
    pub fn wlite_table_field_count(table: *const WliteTable) -> usize;
    pub fn wlite_table_field_at(table: *const WliteTable, index: usize) -> *const WliteField;
    pub fn wlite_table_field(table: *const WliteTable, name: *const c_char) -> *const WliteField;
    pub fn wlite_field_name(field: *const WliteField) -> *const c_char;
    pub fn wlite_field_type(field: *const WliteField) -> c_int;
    pub fn wlite_field_is_nullable(field: *const WliteField) -> c_int;
    pub fn wlite_field_is_primary_key(field: *const WliteField) -> c_int;
    pub fn wlite_field_is_unique(field: *const WliteField) -> c_int;
    pub fn wlite_field_is_autoincrement(field: *const WliteField) -> c_int;
}

#[repr(C)]
pub struct WliteField {
    _private: [u8; 0],
}

#[repr(C)]
pub struct WliteSchema {
    _private: [u8; 0],
}
