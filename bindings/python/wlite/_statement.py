"""Python Statement wrapper."""

from wlite._ffi import lib, check_result, WliteError, WLITE_OK, WLITE_NOT_FOUND

class Statement:
    def __init__(self, ptr): self._ptr = ptr
    def bind_int64(self, i, v): check_result(lib.wlite_bind_int64(self._ptr, i, v))
    def bind_double(self, i, v): check_result(lib.wlite_bind_double(self._ptr, i, v))
    def bind_text(self, i, v): check_result(lib.wlite_bind_text(self._ptr, i, v.encode("utf-8")))
    def bind_null(self, i): check_result(lib.wlite_bind_null(self._ptr, i))
    def step(self):
        rc = lib.wlite_step(self._ptr)
        if rc == WLITE_OK: return True
        if rc == WLITE_NOT_FOUND: return False
        raise WliteError(rc)
    def column_count(self): return lib.wlite_column_count(self._ptr)
    def column_name(self, i): return lib.wlite_column_name(self._ptr, i).decode()
    def column_type(self, i): return lib.wlite_column_type(self._ptr, i)
    def column_int64(self, i): return lib.wlite_column_int64(self._ptr, i)
    def column_double(self, i): return lib.wlite_column_double(self._ptr, i)
    def column_text(self, i):
        p = lib.wlite_column_text(self._ptr, i)
        return p.decode() if p else ""
    def column_value(self, i):
        t = self.column_type(i)
        if t == 0: return None
        elif t == 1: return self.column_int64(i)
        elif t == 2: return self.column_double(i)
        elif t == 3: return self.column_text(i)
        return None
    def __iter__(self): return self
    def __next__(self):
        if self.step(): return [self.column_value(i) for i in range(self.column_count())]
        raise StopIteration
    def __del__(self):
        if self._ptr: lib.wlite_stmt_finalize(self._ptr)
