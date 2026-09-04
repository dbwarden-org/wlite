"""Low-level FFI bindings to libwlite using ctypes."""

import ctypes
import os
import sys

# Find and load libwlite
_lib_name = {
    "linux": "libwlite.so",
    "darwin": "libwlite.dylib",
    "windows": "wlite.dll",
}.get(sys.platform, "libwlite.so")

_search_paths = [
    os.path.join(os.path.dirname(__file__), "..", "..", "..", "libwlite"),
    os.path.join(os.path.dirname(__file__), "..", ".."),
    "/usr/local/lib",
    "/usr/lib",
]

lib = None
for path in _search_paths:
    candidate = os.path.join(path, _lib_name)
    if os.path.exists(candidate):
        try:
            lib = ctypes.CDLL(candidate)
            break
        except OSError:
            continue

if lib is None:
    try:
        lib = ctypes.CDLL("libwlite.so" if sys.platform != "darwin" else "libwlite.dylib")
    except OSError:
        try:
            lib = ctypes.CDLL("libwlite")
        except OSError:
            raise ImportError("Cannot find libwlite. Build libwlite first.")

# Result codes
WLITE_OK = 0
WLITE_ERROR = 1
WLITE_INVALID_ARGUMENT = 2
WLITE_OUT_OF_MEMORY = 3
WLITE_IO_ERROR = 4
WLITE_PARSE_ERROR = 5
WLITE_MODEL_ERROR = 6
WLITE_SQLITE_ERROR = 7
WLITE_CONSTRAINT_ERROR = 8
WLITE_NOT_FOUND = 9
WLITE_BUSY = 10
WLITE_TRANSACTION_ERROR = 11

WLITE_VALUE_NULL = 0
WLITE_VALUE_INTEGER = 1
WLITE_VALUE_REAL = 2
WLITE_VALUE_TEXT = 3
WLITE_VALUE_BLOB = 4


class WliteError(Exception):
    def __init__(self, code, message=""):
        self.code = code
        self.message = message
        super().__init__(f"wlite error {code}: {message}")


def check_result(result):
    if result != WLITE_OK:
        try:
            msg_ptr = lib.wlite_strerror(result)
            msg = ctypes.string_at(msg_ptr).decode() if msg_ptr else ""
        except Exception:
            msg = ""
        raise WliteError(result, msg)


# Function signatures
lib.wlite_abi_version.restype = ctypes.c_int
lib.wlite_version.restype = ctypes.c_char_p
lib.wlite_strerror.argtypes = [ctypes.c_int]
lib.wlite_strerror.restype = ctypes.c_char_p
lib.wlite_error_free.argtypes = [ctypes.c_void_p]
lib.wlite_free.argtypes = [ctypes.c_void_p]
lib.wlite_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
lib.wlite_open.restype = ctypes.c_int
lib.wlite_close.argtypes = [ctypes.c_void_p]
lib.wlite_execute.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int64)]
lib.wlite_execute.restype = ctypes.c_int
lib.wlite_model_load_file.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
lib.wlite_model_load_file.restype = ctypes.c_int
lib.wlite_model_free.argtypes = [ctypes.c_void_p]
lib.wlite_model_validate.argtypes = [ctypes.c_void_p]
lib.wlite_model_validate.restype = ctypes.c_int
lib.wlite_model_table_count.argtypes = [ctypes.c_void_p]
lib.wlite_model_table_count.restype = ctypes.c_size_t
lib.wlite_model_table.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.wlite_model_table.restype = ctypes.c_void_p
lib.wlite_table_name.argtypes = [ctypes.c_void_p]
lib.wlite_table_name.restype = ctypes.c_char_p
lib.wlite_table_field_count.argtypes = [ctypes.c_void_p]
lib.wlite_table_field_count.restype = ctypes.c_size_t
lib.wlite_table_field.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.wlite_table_field.restype = ctypes.c_void_p
lib.wlite_field_name.argtypes = [ctypes.c_void_p]
lib.wlite_field_name.restype = ctypes.c_char_p
lib.wlite_field_is_nullable.argtypes = [ctypes.c_void_p]
lib.wlite_field_is_nullable.restype = ctypes.c_int
lib.wlite_field_is_primary_key.argtypes = [ctypes.c_void_p]
lib.wlite_field_is_primary_key.restype = ctypes.c_int
lib.wlite_field_is_unique.argtypes = [ctypes.c_void_p]
lib.wlite_field_is_unique.restype = ctypes.c_int
lib.wlite_field_is_autoincrement.argtypes = [ctypes.c_void_p]
lib.wlite_field_is_autoincrement.restype = ctypes.c_int
lib.wlite_prepare.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
lib.wlite_prepare.restype = ctypes.c_int
lib.wlite_bind_int64.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int64]
lib.wlite_bind_int64.restype = ctypes.c_int
lib.wlite_bind_double.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_double]
lib.wlite_bind_double.restype = ctypes.c_int
lib.wlite_bind_text.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
lib.wlite_bind_text.restype = ctypes.c_int
lib.wlite_bind_null.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_bind_null.restype = ctypes.c_int
lib.wlite_step.argtypes = [ctypes.c_void_p]
lib.wlite_step.restype = ctypes.c_int
lib.wlite_stmt_finalize.argtypes = [ctypes.c_void_p]
lib.wlite_column_count.argtypes = [ctypes.c_void_p]
lib.wlite_column_count.restype = ctypes.c_int
lib.wlite_column_name.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_column_name.restype = ctypes.c_char_p
lib.wlite_column_type.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_column_type.restype = ctypes.c_int
lib.wlite_column_int64.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_column_int64.restype = ctypes.c_int64
lib.wlite_column_double.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_column_double.restype = ctypes.c_double
lib.wlite_column_text.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.wlite_column_text.restype = ctypes.c_char_p
lib.wlite_diff.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
lib.wlite_diff.restype = ctypes.c_int
lib.wlite_plan_count.argtypes = [ctypes.c_void_p]
lib.wlite_plan_count.restype = ctypes.c_size_t
lib.wl_plan_free.argtypes = [ctypes.c_void_p]
