"""Python Model wrapper."""

import ctypes
from wlite._ffi import lib, WliteError

class Model:
    def __init__(self, ptr):
        self._ptr = ptr

    @classmethod
    def load(cls, path):
        ptr = ctypes.c_void_p()
        rc = lib.wlite_model_load_file(str(path).encode("utf-8"), ctypes.byref(ptr))
        if rc != 0: raise WliteError(rc, f"failed to load {path}")
        return cls(ptr)

    @classmethod
    def from_bytes(cls, data):
        ptr = ctypes.c_void_p()
        rc = lib.wlite_model_load_memory(data, len(data), ctypes.byref(ptr))
        if rc != 0: raise WliteError(rc, "failed to load from bytes")
        return cls(ptr)

    def validate(self):
        rc = lib.wlite_model_validate(self._ptr)
        if rc != 0: raise WliteError(rc, "validation failed")

    def table_count(self): return lib.wlite_model_table_count(self._ptr)

    def table(self, name):
        ptr = lib.wlite_model_table(self._ptr, name.encode("utf-8"))
        if not ptr: return None
        return _Table(ptr)

    def __del__(self):
        if self._ptr: lib.wlite_model_free(self._ptr)

class _Table:
    def __init__(self, ptr): self._ptr = ptr
    def name(self): return lib.wlite_table_name(self._ptr).decode()
    def field_count(self): return lib.wlite_table_field_count(self._ptr)
    def field(self, name):
        ptr = lib.wlite_table_field(self._ptr, name.encode("utf-8"))
        return _Field(ptr) if ptr else None
    def __repr__(self): return f"Table({self.name()!r})"

class _Field:
    def __init__(self, ptr): self._ptr = ptr
    def name(self): return lib.wlite_field_name(self._ptr).decode()
    def is_nullable(self): return lib.wlite_field_is_nullable(self._ptr) != 0
    def is_primary_key(self): return lib.wlite_field_is_primary_key(self._ptr) != 0
    def is_unique(self): return lib.wlite_field_is_unique(self._ptr) != 0
    def is_autoincrement(self): return lib.wlite_field_is_autoincrement(self._ptr) != 0
    def __repr__(self): return f"Field({self.name()!r})"
