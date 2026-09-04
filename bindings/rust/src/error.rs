use std::fmt;

use crate::ffi;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum WliteError {
    Error,
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
    Unknown(i32),
}

impl fmt::Display for WliteError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let msg = match self {
            Self::Error => "general error",
            Self::InvalidArgument => "invalid argument",
            Self::OutOfMemory => "out of memory",
            Self::IoError => "I/O error",
            Self::ParseError => "parse error",
            Self::ModelError => "model error",
            Self::SqliteError => "SQLite error",
            Self::ConstraintError => "constraint violation",
            Self::NotFound => "not found",
            Self::Busy => "busy",
            Self::TransactionError => "transaction error",
            Self::Unknown(code) => return write!(f, "unknown error ({})", code),
        };
        write!(f, "{}", msg)
    }
}

impl std::error::Error for WliteError {}

impl WliteError {
    pub fn from_result(result: i32) -> Self {
        match result {
            ffi::WLITE_OK => unreachable!(),
            ffi::WLITE_ERROR => Self::Error,
            ffi::WLITE_INVALID_ARGUMENT => Self::InvalidArgument,
            ffi::WLITE_OUT_OF_MEMORY => Self::OutOfMemory,
            ffi::WLITE_IO_ERROR => Self::IoError,
            ffi::WLITE_PARSE_ERROR => Self::ParseError,
            ffi::WLITE_MODEL_ERROR => Self::ModelError,
            ffi::WLITE_SQLITE_ERROR => Self::SqliteError,
            ffi::WLITE_CONSTRAINT_ERROR => Self::ConstraintError,
            ffi::WLITE_NOT_FOUND => Self::NotFound,
            ffi::WLITE_BUSY => Self::Busy,
            ffi::WLITE_TRANSACTION_ERROR => Self::TransactionError,
            code => Self::Unknown(code),
        }
    }
}

pub fn check(result: i32) -> Result<(), WliteError> {
    if result == ffi::WLITE_OK {
        Ok(())
    } else {
        Err(WliteError::from_result(result))
    }
}
