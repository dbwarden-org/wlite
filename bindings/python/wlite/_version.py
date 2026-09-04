"""Version info for wlite Python package."""

__version__ = "0.2.0"

def version():
    from wlite._ffi import lib
    return lib.wlite_version().decode()

def abi_version():
    from wlite._ffi import lib
    return lib.wlite_abi_version()
