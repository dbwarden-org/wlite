"""Python Database wrapper."""

import ctypes
from wlite._ffi import lib, check_result, WliteError
from wlite._model import Model
from wlite._statement import Statement

class Database:
    def __init__(self, ptr):
        self._ptr = ptr

    @classmethod
    def open(cls, path):
        ptr = ctypes.c_void_p()
        rc = lib.wlite_open(str(path).encode("utf-8"), ctypes.byref(ptr))
        if rc != 0: raise WliteError(rc, f"failed to open {path}")
        return cls(ptr)

    @classmethod
    def memory(cls):
        ptr = ctypes.c_void_p()
        rc = lib.wlite_open(b":memory:", ctypes.byref(ptr))
        if rc != 0: raise WliteError(rc, "failed to open in-memory")
        return cls(ptr)

    def execute(self, sql):
        affected = ctypes.c_int64(0)
        rc = lib.wlite_execute(self._ptr, sql.encode("utf-8"), ctypes.byref(affected))
        if rc != 0: raise WliteError(rc, f"execute failed: {sql}")
        return affected.value

    def prepare(self, sql):
        ptr = ctypes.c_void_p()
        rc = lib.wlite_prepare(self._ptr, sql.encode("utf-8"), ctypes.byref(ptr))
        if rc != 0: raise WliteError(rc, f"prepare failed: {sql}")
        return Statement(ptr)

    def query(self, sql, params=()):
        stmt = self.prepare(sql)
        for i, p in enumerate(params):
            if p is None: stmt.bind_null(i + 1)
            elif isinstance(p, int): stmt.bind_int64(i + 1, p)
            elif isinstance(p, float): stmt.bind_double(i + 1, p)
            else: stmt.bind_text(i + 1, str(p))
        rows = []
        while stmt.step():
            row = {}
            for i in range(stmt.column_count()):
                row[stmt.column_name(i)] = stmt.column_value(i)
            rows.append(row)
        return rows

    def migrate(self, model):
        check_result(lib.wlite_migrate(self._ptr, model._ptr))

    def close(self):
        if self._ptr: lib.wlite_close(self._ptr); self._ptr = None

    def __enter__(self): return self
    def __exit__(self, *a): self.close()
    def __del__(self): self.close()
