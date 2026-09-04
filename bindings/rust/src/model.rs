use std::ffi::{CStr, CString};
use std::path::Path;

use crate::error::check;
use crate::ffi;
use crate::Result;

pub struct Model {
    ptr: *mut ffi::WliteModel,
}

unsafe impl Send for Model {}
unsafe impl Sync for Model {}

impl Model {
    pub fn load(path: impl AsRef<Path>) -> Result<Self> {
        let c_path = CString::new(path.as_ref().to_str().unwrap()).unwrap();
        let mut ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_model_load_file(c_path.as_ptr(), &mut ptr) })?;
        Ok(Self { ptr })
    }

    pub fn from_bytes(data: &[u8]) -> Result<Self> {
        let mut ptr = std::ptr::null_mut();
        check(unsafe {
            ffi::wlite_model_load_memory(data.as_ptr() as *const _, data.len(), &mut ptr)
        })?;
        Ok(Self { ptr })
    }

    pub fn validate(&self) -> Result<()> {
        check(unsafe { ffi::wlite_model_validate(self.ptr) })
    }

    pub fn table_count(&self) -> usize {
        unsafe { ffi::wlite_model_table_count(self.ptr) }
    }

    pub fn table(&self, name: &str) -> Option<&TableRef> {
        let c_name = CString::new(name).unwrap();
        let ptr = unsafe { ffi::wlite_model_table(self.ptr, c_name.as_ptr()) };
        if ptr.is_null() {
            None
        } else {
            Some(unsafe { &*(ptr as *const TableRef) })
        }
    }

    pub fn as_ptr(&self) -> *const ffi::WliteModel {
        self.ptr
    }
}

impl Drop for Model {
    fn drop(&mut self) {
        unsafe { ffi::wlite_model_free(self.ptr) };
    }
}

pub struct TableRef {
    _private: [u8; 0],
}

impl TableRef {
    pub fn name(&self) -> &str {
        unsafe {
            let ptr = ffi::wlite_table_name(self as *const Self as *const ffi::WliteTable);
            CStr::from_ptr(ptr).to_str().unwrap_or("")
        }
    }

    pub fn sql_name(&self) -> &str {
        unsafe {
            let ptr = ffi::wlite_table_sql_name(self as *const Self as *const ffi::WliteTable);
            CStr::from_ptr(ptr).to_str().unwrap_or("")
        }
    }

    pub fn field_count(&self) -> usize {
        unsafe { ffi::wlite_table_field_count(self as *const Self as *const ffi::WliteTable) }
    }

    pub fn field(&self, name: &str) -> Option<&FieldRef> {
        let c_name = CString::new(name).unwrap();
        let ptr = unsafe {
            ffi::wlite_table_field(self as *const Self as *const ffi::WliteTable, c_name.as_ptr())
        };
        if ptr.is_null() {
            None
        } else {
            Some(unsafe { &*(ptr as *const FieldRef) })
        }
    }
}

pub struct FieldRef {
    _private: [u8; 0],
}

impl FieldRef {
    pub fn name(&self) -> &str {
        unsafe {
            let ptr = ffi::wlite_field_name(self as *const Self as *const ffi::WliteField);
            CStr::from_ptr(ptr).to_str().unwrap_or("")
        }
    }

    pub fn is_nullable(&self) -> bool {
        unsafe { ffi::wlite_field_is_nullable(self as *const Self as *const ffi::WliteField) != 0 }
    }

    pub fn is_primary_key(&self) -> bool {
        unsafe { ffi::wlite_field_is_primary_key(self as *const Self as *const ffi::WliteField) != 0 }
    }

    pub fn is_unique(&self) -> bool {
        unsafe { ffi::wlite_field_is_unique(self as *const Self as *const ffi::WliteField) != 0 }
    }

    pub fn is_autoincrement(&self) -> bool {
        unsafe { ffi::wlite_field_is_autoincrement(self as *const Self as *const ffi::WliteField) != 0 }
    }
}
