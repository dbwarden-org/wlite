pub mod ffi;
pub mod error;
pub mod database;
pub mod model;
pub mod statement;

pub use error::WliteError;
pub use database::Database;
pub use model::Model;
pub use statement::Statement;

pub type Result<T> = std::result::Result<T, WliteError>;

pub fn version() -> &'static str {
    unsafe { std::ffi::CStr::from_ptr(ffi::wlite_version()).to_str().unwrap_or("unknown") }
}

pub fn abi_version() -> i32 {
    unsafe { ffi::wlite_abi_version() }
}
