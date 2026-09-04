using System;
using System.Runtime.InteropServices;

namespace Wlite
{
    internal static class Native
    {
        private const string LibName = "wlite";

        // Database
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_open([MarshalAs(UnmanagedType.LPStr)] string path, out IntPtr db);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void wlite_close(IntPtr db);

        // SQL Execution
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_execute(IntPtr db, [MarshalAs(UnmanagedType.LPStr)] string sql, out long rowsAffected);

        // Prepared Statements
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_prepare(IntPtr db, [MarshalAs(UnmanagedType.LPStr)] string sql, out IntPtr stmt);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_bind_int64(IntPtr stmt, int index, long value);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_bind_double(IntPtr stmt, int index, double value);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_bind_text(IntPtr stmt, int index, [MarshalAs(UnmanagedType.LPStr)] string value);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_bind_null(IntPtr stmt, int index);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_step(IntPtr stmt);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void wlite_stmt_finalize(IntPtr stmt);

        // Column Access
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_column_count(IntPtr stmt);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr wlite_column_name(IntPtr stmt, int column);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_column_type(IntPtr stmt, int column);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern long wlite_column_int64(IntPtr stmt, int column);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern double wlite_column_double(IntPtr stmt, int column);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr wlite_column_text(IntPtr stmt, int column);

        // Model
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_model_load_file([MarshalAs(UnmanagedType.LPStr)] string path, out IntPtr model);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void wlite_model_free(IntPtr model);

        // Migration
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_migrate(IntPtr db, IntPtr model);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_diff(IntPtr db, IntPtr model, out IntPtr plan);

        // Transactions
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_begin(IntPtr db, out IntPtr tx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_commit(IntPtr tx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int wlite_rollback(IntPtr tx);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void wlite_tx_free(IntPtr tx);

        // Error
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr wlite_strerror(int result);

        // Version
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr wlite_version();
    }

    public enum WliteResult : int
    {
        OK = 0,
        Error = -1,
        NotFound = -2,
        Memory = -3,
        Io = -4,
        Corrupt = -5,
        Range = -6,
    }

    public enum ValueType : int
    {
        Integer = 1,
        Float = 2,
        Text = 3,
        Blob = 4,
        Null = 5,
    }

    public class WliteException : Exception
    {
        public WliteResult Result { get; }

        public WliteException(WliteResult result)
            : base(GetMessage(result))
        {
            Result = result;
        }

        private static string GetMessage(WliteResult result)
        {
            IntPtr ptr = Native.wlite_strerror((int)result);
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) ?? result.ToString() : result.ToString();
        }
    }

    public class Database : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;

        private Database(IntPtr handle)
        {
            _handle = handle;
        }

        public static Database Open(string path)
        {
            int result = Native.wlite_open(path, out IntPtr handle);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            return new Database(handle);
        }

        public void Execute(string sql)
        {
            ThrowIfDisposed();
            int result = Native.wlite_execute(_handle, sql, out _);
            if (result != 0)
                throw new WliteException((WliteResult)result);
        }

        public Statement Prepare(string sql)
        {
            ThrowIfDisposed();
            int result = Native.wlite_prepare(_handle, sql, out IntPtr stmt);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            return new Statement(stmt);
        }

        public void Migrate(Model model)
        {
            ThrowIfDisposed();
            int result = Native.wlite_migrate(_handle, model.Handle);
            if (result != 0)
                throw new WliteException((WliteResult)result);
        }

        public Transaction Begin()
        {
            ThrowIfDisposed();
            int result = Native.wlite_begin(_handle, out IntPtr tx);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            return new Transaction(tx);
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                if (_handle != IntPtr.Zero)
                {
                    Native.wlite_close(_handle);
                    _handle = IntPtr.Zero;
                }
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Database));
        }

        ~Database() => Dispose();
    }

    public class Model : IDisposable
    {
        internal IntPtr Handle { get; }
        private bool _disposed;

        private Model(IntPtr handle)
        {
            Handle = handle;
        }

        public static Model Load(string path)
        {
            int result = Native.wlite_model_load_file(path, out IntPtr handle);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            return new Model(handle);
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                if (Handle != IntPtr.Zero)
                {
                    Native.wlite_model_free(Handle);
                }
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        ~Model() => Dispose();
    }

    public class Statement : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;

        internal Statement(IntPtr handle)
        {
            _handle = handle;
        }

        public void Bind(int index, long value) => Check(Native.wlite_bind_int64(_handle, index, value));
        public void Bind(int index, double value) => Check(Native.wlite_bind_double(_handle, index, value));
        public void Bind(int index, string value) => Check(Native.wlite_bind_text(_handle, index, value));
        public void Bind(int index) => Check(Native.wlite_bind_null(_handle, index));

        public bool Step()
        {
            ThrowIfDisposed();
            int result = Native.wlite_step(_handle);
            return result == 0;
        }

        public int ColumnCount() => Native.wlite_column_count(_handle);

        public string? ColumnName(int column)
        {
            IntPtr ptr = Native.wlite_column_name(_handle, column);
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) : null;
        }

        public ValueType ColumnType(int column) => (ValueType)Native.wlite_column_type(_handle, column);

        public long ColumnInt64(int column) => Native.wlite_column_int64(_handle, column);
        public double ColumnDouble(int column) => Native.wlite_column_double(_handle, column);

        public string? ColumnText(int column)
        {
            IntPtr ptr = Native.wlite_column_text(_handle, column);
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) : null;
        }

        public void Finalize()
        {
            if (!_disposed)
            {
                Native.wlite_stmt_finalize(_handle);
                _handle = IntPtr.Zero;
                _disposed = true;
            }
        }

        public void Dispose()
        {
            Finalize();
            GC.SuppressFinalize(this);
        }

        private void Check(int result)
        {
            if (result != 0)
                throw new WliteException((WliteResult)result);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Statement));
        }

        ~Statement() => Dispose();
    }

    public class Transaction : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;

        internal Transaction(IntPtr handle)
        {
            _handle = handle;
        }

        public void Commit()
        {
            ThrowIfDisposed();
            int result = Native.wlite_commit(_handle);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            _disposed = true;
        }

        public void Rollback()
        {
            ThrowIfDisposed();
            int result = Native.wlite_rollback(_handle);
            if (result != 0)
                throw new WliteException((WliteResult)result);
            _disposed = true;
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                Native.wlite_tx_free(_handle);
                _handle = IntPtr.Zero;
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(Transaction));
        }

        ~Transaction() => Dispose();
    }

    public static class WliteVersion
    {
        public static string Get()
        {
            IntPtr ptr = Native.wlite_version();
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) ?? "unknown" : "unknown";
        }
    }
}
