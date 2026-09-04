use std::ffi::CStr;

use crate::error::{check, WliteError};
use crate::ffi;
use crate::Result;

pub struct Statement {
    ptr: *mut ffi::WliteStmt,
}

unsafe impl Send for Statement {}

impl Statement {
    pub(crate) fn from_raw(ptr: *mut ffi::WliteStmt) -> Self {
        Self { ptr }
    }

    pub fn bind_int64(&self, index: i32, value: i64) -> Result<()> {
        check(unsafe { ffi::wlite_bind_int64(self.ptr, index, value) })
    }

    pub fn bind_double(&self, index: i32, value: f64) -> Result<()> {
        check(unsafe { ffi::wlite_bind_double(self.ptr, index, value) })
    }

    pub fn bind_text(&self, index: i32, value: &str) -> Result<()> {
        let c_value = std::ffi::CString::new(value).unwrap();
        check(unsafe { ffi::wlite_bind_text(self.ptr, index, c_value.as_ptr()) })
    }

    pub fn bind_null(&self, index: i32) -> Result<()> {
        check(unsafe { ffi::wlite_bind_null(self.ptr, index) })
    }

    pub fn step(&self) -> Result<bool> {
        let result = unsafe { ffi::wlite_step(self.ptr) };
        if result == ffi::WLITE_OK {
            Ok(true) // row available
        } else if result == ffi::WLITE_NOT_FOUND {
            Ok(false) // done
        } else {
            Err(WliteError::from_result(result))
        }
    }

    pub fn reset(&self) -> Result<()> {
        check(unsafe { ffi::wlite_step(self.ptr) })
    }

    pub fn column_count(&self) -> i32 {
        unsafe { ffi::wlite_column_count(self.ptr) }
    }

    pub fn column_name(&self, index: i32) -> &str {
        unsafe {
            let ptr = ffi::wlite_column_name(self.ptr, index);
            CStr::from_ptr(ptr).to_str().unwrap_or("")
        }
    }

    pub fn column_int64(&self, index: i32) -> i64 {
        unsafe { ffi::wlite_column_int64(self.ptr, index) }
    }

    pub fn column_double(&self, index: i32) -> f64 {
        unsafe { ffi::wlite_column_double(self.ptr, index) }
    }

    pub fn column_text(&self, index: i32) -> &str {
        unsafe {
            let ptr = ffi::wlite_column_text(self.ptr, index);
            if ptr.is_null() { "" } else { CStr::from_ptr(ptr).to_str().unwrap_or("") }
        }
    }
}

impl Drop for Statement {
    fn drop(&mut self) {
        unsafe { ffi::wlite_stmt_finalize(self.ptr) };
    }
}
