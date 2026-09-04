// Package wlite provides Go bindings for libwlite.
//
// It wraps the C ABI of libwlite, providing an idiomatic Go API for
// SQLite schema management, migrations, and queries.
package wlite

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../. -lwlite -lsqlite3
#include <stdlib.h>
#include "wlite.h"
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// Error represents a wlite error.
type Error struct {
	Code    int
	Message string
}

func (e *Error) Error() string {
	return fmt.Sprintf("wlite error %d: %s", e.Code, e.Message)
}

func checkResult(r C.wlite_result) error {
	if r == C.WLITE_OK {
		return nil
	}
	return &Error{Code: int(r), Message: C.GoString(C.wlite_strerror(r))}
}

// Version returns the libwlite version string.
func Version() string {
	return C.GoString(C.wlite_version())
}

// ABIVersion returns the libwlite ABI version.
func ABIVersion() int {
	return int(C.wlite_abi_version())
}

// Database represents a wlite database connection.
type Database struct {
	ptr *C.wlite_db
}

// Open opens a SQLite database.
func Open(path string) (*Database, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	var db *C.wlite_db
	if err := checkResult(C.wlite_open(cpath, &db)); err != nil {
		return nil, err
	}
	return &Database{ptr: db}, nil
}

// OpenMemory opens an in-memory database.
func OpenMemory() (*Database, error) {
	var db *C.wlite_db
	if err := checkResult(C.wlite_open(C.CString(":memory:"), &db)); err != nil {
		return nil, err
	}
	return &Database{ptr: db}, nil
}

// Close closes the database.
func (db *Database) Close() {
	if db.ptr != nil {
		C.wlite_close(db.ptr)
		db.ptr = nil
	}
}

// Execute runs SQL that doesn't return rows.
func (db *Database) Execute(sql string) error {
	csql := C.CString(sql)
	defer C.free(unsafe.Pointer(csql))
	return checkResult(C.wlite_execute(db.ptr, csql, nil))
}

// Prepare prepares a SQL statement.
func (db *Database) Prepare(sql string) (*Statement, error) {
	csql := C.CString(sql)
	defer C.free(unsafe.Pointer(csql))
	var stmt *C.wlite_stmt
	if err := checkResult(C.wlite_prepare(db.ptr, csql, &stmt)); err != nil {
		return nil, err
	}
	return &Statement{stmt: stmt}, nil
}

// Query executes a query and returns rows as maps.
func (db *Database) Query(sql string, args ...interface{}) ([]map[string]interface{}, error) {
	stmt, err := db.Prepare(sql)
	if err != nil {
		return nil, err
	}
	defer stmt.Close()

	for i, arg := range args {
		switch v := arg.(type) {
		case int64:
			stmt.BindInt64(i+1, v)
		case int:
			stmt.BindInt64(i+1, int64(v))
		case float64:
			stmt.BindDouble(i+1, v)
		case string:
			stmt.BindText(i+1, v)
		case nil:
			stmt.BindNull(i+1)
		default:
			stmt.BindText(i+1, fmt.Sprintf("%v", v))
		}
	}

	var rows []map[string]interface{}
	for stmt.Step() {
		row := make(map[string]interface{})
		for i := 0; i < stmt.ColumnCount(); i++ {
			name := stmt.ColumnName(i)
			vt := stmt.ColumnType(i)
			if vt == 0 { // WLITE_VALUE_NULL
				row[name] = nil
			} else if vt == 1 { // WLITE_VALUE_INTEGER
				row[name] = stmt.ColumnInt64(i)
			} else if vt == 2 { // WLITE_VALUE_REAL
				row[name] = stmt.ColumnDouble(i)
			} else if vt == 3 { // WLITE_VALUE_TEXT
				row[name] = stmt.ColumnText(i)
			} else {
				row[name] = nil
			}
		}
		rows = append(rows, row)
	}
	return rows, nil
}

// Migrate applies a model schema to the database.
func (db *Database) Migrate(model *Model) error {
	return checkResult(C.wlite_diff(db.ptr, model.ptr, nil))
}

// Diff computes a migration plan.
func (db *Database) Diff(model *Model) error {
	var plan *C.WlPlan
	if err := checkResult(C.wlite_diff(db.ptr, model.ptr, &plan)); err != nil {
		return err
	}
	if plan != nil {
		C.wl_plan_free(plan)
	}
	return nil
}

// Transaction begins a transaction. Call Commit or Rollback.
func (db *Database) Transaction() (*Transaction, error) {
	var tx *C.wlite_tx
	if err := checkResult(C.wlite_begin(db.ptr, &tx)); err != nil {
		return nil, err
	}
	return &Transaction{ptr: tx}, nil
}

// Statement represents a prepared SQL statement.
type Statement struct {
	stmt *C.wlite_stmt
}

func (s *Statement) BindInt64(index int, value int64) error {
	return checkResult(C.wlite_bind_int64(s.stmt, C.int(index), C.int64_t(value)))
}

func (s *Statement) BindDouble(index int, value float64) error {
	return checkResult(C.wlite_bind_double(s.stmt, C.int(index), C.double(value)))
}

func (s *Statement) BindText(index int, value string) error {
	cv := C.CString(value)
	defer C.free(unsafe.Pointer(cv))
	return checkResult(C.wlite_bind_text(s.stmt, C.int(index), cv))
}

func (s *Statement) BindNull(index int) error {
	return checkResult(C.wlite_bind_null(s.stmt, C.int(index)))
}

func (s *Statement) Step() bool {
	r := C.wlite_step(s.stmt)
	if r == C.WLITE_NOT_FOUND {
		return false
	}
	if r != C.WLITE_OK {
		panic(&Error{Code: int(r), Message: C.GoString(C.wlite_strerror(r))})
	}
	return true
}

func (s *Statement) ColumnCount() int {
	return int(C.wlite_column_count(s.stmt))
}

func (s *Statement) ColumnName(index int) string {
	return C.GoString(C.wlite_column_name(s.stmt, C.int(index)))
}

func (s *Statement) ColumnType(index int) int {
	return int(C.wlite_column_type(s.stmt, C.int(index)))
}

func (s *Statement) ColumnInt64(index int) int64 {
	return int64(C.wlite_column_int64(s.stmt, C.int(index)))
}

func (s *Statement) ColumnDouble(index int) float64 {
	return float64(C.wlite_column_double(s.stmt, C.int(index)))
}

func (s *Statement) ColumnText(index int) string {
	p := C.wlite_column_text(s.stmt, C.int(index))
	if p == nil {
		return ""
	}
	return C.GoString(p)
}

func (s *Statement) Close() {
	if s.stmt != nil {
		C.wlite_stmt_finalize(s.stmt)
		s.stmt = nil
	}
}

// Transaction represents a database transaction.
type Transaction struct {
	ptr *C.wlite_tx
}

func (tx *Transaction) Commit() error {
	err := checkResult(C.wlite_commit(tx.ptr))
	if err == nil {
		tx.ptr = nil
	}
	return err
}

func (tx *Transaction) Rollback() error {
	err := checkResult(C.wlite_rollback(tx.ptr))
	tx.ptr = nil
	return err
}

func (tx *Transaction) Savepoint(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return checkResult(C.wlite_savepoint(tx.ptr, cname))
}

func (tx *Transaction) Release(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return checkResult(C.wlite_release(tx.ptr, cname))
}

func (tx *Transaction) RollbackTo(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return checkResult(C.wlite_rollback_to(tx.ptr, cname))
}

func (tx *Transaction) Close() {
	if tx.ptr != nil {
		C.wlite_tx_free(tx.ptr)
		tx.ptr = nil
	}
}

// Model represents a loaded .wlite model.
type Model struct {
	ptr *C.wlite_model
}

// LoadModel loads a model from a .wlite file.
func LoadModel(path string) (*Model, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	var m *C.wlite_model
	if err := checkResult(C.wlite_model_load_file(cpath, &m)); err != nil {
		return nil, err
	}
	return &Model{ptr: m}, nil
}

// ModelFromBytes loads a model from bytes.
func ModelFromBytes(data []byte) (*Model, error) {
	var m *C.wlite_model
	if err := checkResult(C.wlite_model_load_memory(
		unsafe.Pointer(&data[0]), C.size_t(len(data)), &m)); err != nil {
		return nil, err
	}
	return &Model{ptr: m}, nil
}

func (m *Model) Close() {
	if m.ptr != nil {
		C.wlite_model_free(m.ptr)
		m.ptr = nil
	}
}

func (m *Model) Validate() error {
	return checkResult(C.wlite_model_validate(m.ptr))
}

func (m *Model) TableCount() int {
	return int(C.wlite_model_table_count(m.ptr))
}
