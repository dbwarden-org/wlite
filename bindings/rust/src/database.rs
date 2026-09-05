use std::ffi::CString;
use std::path::Path;

use crate::error::check;
use crate::ffi;
use crate::model::Model;
use crate::statement::Statement;
use crate::Result;

pub struct Database {
    ptr: *mut ffi::WliteDb,
}

unsafe impl Send for Database {}
unsafe impl Sync for Database {}

impl Database {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let c_path = CString::new(path.as_ref().to_str().unwrap()).unwrap();
        let mut ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_open(c_path.as_ptr(), &mut ptr) })?;
        Ok(Self { ptr })
    }

    pub fn open_memory() -> Result<Self> {
        let c_path = CString::new(":memory:").unwrap();
        let mut ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_open(c_path.as_ptr(), &mut ptr) })?;
        Ok(Self { ptr })
    }

    pub fn execute(&self, sql: &str) -> Result<i64> {
        let c_sql = CString::new(sql).unwrap();
        let mut affected: i64 = 0;
        check(unsafe { ffi::wlite_execute(self.ptr, c_sql.as_ptr(), &mut affected) })?;
        Ok(affected)
    }

    pub fn prepare(&self, sql: &str) -> Result<Statement> {
        let c_sql = CString::new(sql).unwrap();
        let mut ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_prepare(self.ptr, c_sql.as_ptr(), &mut ptr) })?;
        Ok(Statement::from_raw(ptr))
    }

    pub fn migrate(&self, model: &Model) -> Result<()> {
        check(unsafe { ffi::wlite_migrate(self.ptr, model.as_ptr()) })
    }

    pub fn diff(&self, model: &Model) -> Result<()> {
        let mut plan_ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_diff(self.ptr, model.as_ptr(), &mut plan_ptr) })?;
        if !plan_ptr.is_null() {
            unsafe { ffi::wl_plan_free(plan_ptr) };
        }
        Ok(())
    }

    pub fn begin(&self) -> Result<Transaction> {
        let mut tx_ptr = std::ptr::null_mut();
        check(unsafe { ffi::wlite_begin(self.ptr, &mut tx_ptr) })?;
        Ok(Transaction { ptr: tx_ptr })
    }

    pub fn as_ptr(&self) -> *mut ffi::WliteDb {
        self.ptr
    }
}

impl Drop for Database {
    fn drop(&mut self) {
        unsafe { ffi::wlite_close(self.ptr) };
    }
}

pub struct Transaction {
    ptr: *mut ffi::WliteTx,
}

impl Transaction {
    pub fn commit(mut self) -> Result<()> {
        let r = check(unsafe { ffi::wlite_commit(self.ptr) });
        self.ptr = std::ptr::null_mut();
        r
    }

    pub fn rollback(mut self) -> Result<()> {
        let r = check(unsafe { ffi::wlite_rollback(self.ptr) });
        self.ptr = std::ptr::null_mut();
        r
    }
}

impl Drop for Transaction {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { ffi::wlite_tx_free(self.ptr) };
        }
    }
}
