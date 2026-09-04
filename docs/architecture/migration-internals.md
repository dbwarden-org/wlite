---
title: "Migration Internals: How wlite Converts Diffs into SQL"
description: "Deep dive into the diff-to-SQL pipeline, classification rules, rebuild expansion, and ordering constraints"
---

# Migration Internals

This document explains the internal mechanics of how wlite transforms a schema diff into a sequence of SQL statements that SQLite can execute safely. It covers every stage of the pipeline from classification to execution, including edge cases and deliberate omissions.

## The SQLite ALTER TABLE Limitation

SQLite supports only a narrow subset of `ALTER TABLE` operations. Specifically, you can:

- Rename a table (`ALTER TABLE ... RENAME TO`)
- Add a column (`ALTER TABLE ... ADD COLUMN`)
- Rename a column (`ALTER TABLE ... RENAME COLUMN` as of 3.25.0)
- Set a `DEFAULT` value for an existing column (via `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT` as of 3.92.0)

That is all. You cannot:

- Drop a column
- Change a column type
- Add or remove a `NOT NULL` constraint
- Add or remove a `UNIQUE` constraint
- Add or remove a `PRIMARY KEY` constraint
- Add or remove a `CHECK` constraint
- Modify `ON CONFLICT` behavior
- Change a column's collation
- Create a `VIEW`
- Create a `TRIGGER`
- Alter the definition of an `INDEX`

This limitation means that most schema changes that other databases handle with a single `ALTER TABLE` statement require a full table rebuild in SQLite. wlite's migration engine exists to automate this rebuild process correctly.

## Classification of Differences

When wlite compares two schema states, it produces a set of differences. Each difference is classified into one of four categories. This classification determines the SQL generation strategy.

### Additive Changes

Additive changes introduce new objects or new properties to existing objects without modifying or removing existing definitions. These are the safest operations.

| Change Type | Example | Classification |
|---|---|
| New table | Table did not exist, now it does | Additive |
| New column | Column did not exist in table, now it does | Additive |
| New index | Index did not exist, now it does | Additive |
| New `DEFAULT` on existing column | Column gains a `DEFAULT` it did not have | Additive |
| New `CHECK` constraint (appended) | Table gains a new `CHECK` expression | Additive |

Additive changes map directly to `ALTER TABLE ... ADD COLUMN` or `CREATE INDEX` statements. No rebuild is required.

### Subtractive Changes

Subtractive changes remove objects or properties. Since SQLite cannot drop columns or remove constraints from existing tables, subtractive changes on columns require a rebuild.

| Change Type | Example | Classification |
|---|---|
| Drop table | Table exists, should not | Subtractive |
| Drop column | Column exists, should not | Subtractive |
| Drop index | Index exists, should not | Subtractive |
| Remove `NOT NULL` | Column has `NOT NULL`, should not | Subtractive |
| Remove `CHECK` | Table has `CHECK`, should not | Subtractive |

Drop table operations map to `DROP TABLE`. Drop index operations map to `DROP INDEX`. Column-level subtractive changes require a rebuild.

### Alternative Changes

Alternative changes modify a property of an existing object without adding or removing the object itself. The result is a different definition for the same object.

| Change Type | Example | Classification |
|---|---|---|
| Type change | `TEXT` to `INTEGER` | Alternative |
| `NOT NULL` added | Column was nullable, now `NOT NULL` | Alternative |
| `UNIQUE` added | Column was not unique, now it is | Alternative |
| `DEFAULT` changed | `DEFAULT 0` to `DEFAULT 1` | Alternative |
| `CHECK` modified | Expression changed | Alternative |
| Collation changed | `BINARY` to `NOCASE` | Alternative |

Almost all alternative changes require a rebuild because SQLite cannot alter column properties in place.

### Rebuild Required

A rebuild is the catch-all classification for any change that SQLite cannot express with a direct DDL statement. The rebuild operation is a specific sequence of SQL statements that recreates the table with the new definition while preserving data.

The decision tree is:

```
Is the table new?
  -> Yes: CREATE TABLE (additive)
  -> No: Is the table dropped?
    -> Yes: DROP TABLE (subtractive)
    -> No: Is a column added?
      -> Yes: ALTER TABLE ADD COLUMN (additive)
      -> No: Is a column dropped or modified?
        -> Yes: Rebuild required
        -> No: Is an index changed?
          -> Yes: DROP INDEX + CREATE INDEX
          -> No: Check constraint or other DDL
```

## Rebuild Expansion

When a rebuild is required, wlite expands it into a full SQL sequence. This sequence is the core of the migration engine. The expanded sequence for a rebuild is:

```sql
-- 1. Disable foreign key enforcement
PRAGMA foreign_keys = OFF;

-- 2. Begin a transaction
BEGIN TRANSACTION;

-- 3. Create the new table with the target definition
CREATE TABLE "table_name_new" (
  "col1" TEXT NOT NULL,
  "col2" INTEGER DEFAULT 0,
  "col3" REAL
);

-- 4. Copy data from the old table into the new table
INSERT INTO "table_name_new" ("col1", "col2", "col3")
SELECT "col1", "col2", "col3"
FROM "table_name";

-- 5. Drop the old table
DROP TABLE "table_name";

-- 6. Rename the new table to the original name
ALTER TABLE "table_name_new" RENAME TO "table_name";

-- 7. Recreate all indexes that belong to this table
CREATE INDEX "idx_table_name_col1" ON "table_name" ("col1");

-- 8. Recreate all triggers that reference this table
CREATE TRIGGER "trg_table_name_insert" AFTER INSERT ON "table_name"
BEGIN
  INSERT INTO log (table_name, op) VALUES ('table_name', 'INSERT');
END;

-- 9. Commit the transaction
COMMIT;

-- 10. Re-enable foreign key enforcement
PRAGMA foreign_keys = ON;
```

Each step in this sequence exists for a reason. Disabling foreign keys prevents cascading failures during the drop. The transaction ensures atomicity. Creating the new table first allows a safe `SELECT` from the old table. Renaming at the end is the final step so all references resolve correctly.

### Column Selection During Copy

When copying data from the old table to the new table, wlite must map columns correctly. The rules are:

1. If a column exists in both the old and new table, it is copied by name.
2. If a column exists only in the new table (additive), it receives its `DEFAULT` value or `NULL`.
3. If a column exists only in the old table (subtractive), it is silently dropped during the copy.
4. Column order in the `INSERT INTO ... SELECT` matches the new table's column order, not the old table's.

This means the `INSERT INTO "table_name_new" (...) SELECT ...` statement may list columns in a different order than the original table. wlite always uses the new table's column order.

### SELECT Statement Construction

The `SELECT` portion of the copy statement maps old columns to new columns. For each column in the new table:

```sql
-- If the column exists in the old table:
SELECT "col1", "col2", "col3"
FROM "table_name"

-- If a column is new (not in old table):
SELECT "col1", "col2", NULL AS "col3"
FROM "table_name"
-- or, if the new column has a DEFAULT:
SELECT "col1", "col2", 0 AS "col3"
FROM "table_name"
```

The alias syntax (`NULL AS "col3"` or `0 AS "col3"`) ensures the `SELECT` result set matches the new table's column list exactly.

## Planning Rules and Ordering

After classification, wlite produces a plan. The plan is an ordered list of SQL statements. The ordering follows these rules:

### Rule 1: Drops Before Creates

If a table is being dropped and a new table with the same name is being created, the `DROP TABLE` must come before the `CREATE TABLE`. This prevents name conflicts.

### Rule 2: Additive Before Subtractive

New columns are added before old columns are removed within a rebuild. However, since a rebuild handles both simultaneously, this rule primarily applies when operations are split across multiple statements.

### Rule 3: Rebuilds Are Atomic

A rebuild for a single table is expanded into its full sequence and treated as a single unit in the plan. No other operation on the same table can interleave with a rebuild.

### Rule 4: Independent Tables Can Be Parallelized

Rebuilds on different tables have no dependencies and can theoretically be executed in parallel. In practice, wlite serializes them within a single transaction.

### Rule 5: Index Operations Come After Table Operations

Index creation and drop operations are ordered after all table rebuilds. This ensures the table exists before an index is created on it.

### Rule 6: Foreign Key Ordering

When table A has a foreign key referencing table B, and both tables are being rebuilt, wlite orders the rebuild of table B (the referenced table) before table A (the referencing table). This prevents foreign key violations during the rebuild.

### Rule 7: View and Trigger Recreations Last

Views and triggers that reference rebuilt tables are dropped at the start of the rebuild sequence and recreated at the end.

### Example Plan

Given the following diffs:

- `users`: add column `email_verified` (boolean, default false)
- `posts`: change column `title` from `TEXT` to `TEXT NOT NULL`
- `idx_posts_title`: new index on `posts.title`

The plan would be:

```sql
-- users: additive, direct ALTER TABLE
ALTER TABLE "users" ADD COLUMN "email_verified" INTEGER NOT NULL DEFAULT 0;

-- posts: rebuild required (NOT NULL change)
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "posts_new" (
  "id" INTEGER PRIMARY KEY,
  "title" TEXT NOT NULL,
  "body" TEXT,
  "created_at" TEXT
);
INSERT INTO "posts_new" ("id", "title", "body", "created_at")
SELECT "id", "title", "body", "created_at"
FROM "posts";
DROP TABLE "posts";
ALTER TABLE "posts_new" RENAME TO "posts";
COMMIT;
PRAGMA foreign_keys = ON;

-- idx_posts_title: new index
CREATE INDEX "idx_posts_title" ON "posts" ("title");
```

## Collapse Logic

When multiple changes target the same table, wlite collapses them into a single rebuild rather than executing multiple rebuilds. This is called collapse logic.

### Why Collapse?

Multiple rebuilds on the same table are wasteful. Each rebuild copies the entire table, drops it, and recreates it. Collapsing multiple changes into one rebuild means the table is only rebuilt once.

### How Collapse Works

wlite collects all changes for a given table and determines whether they can be expressed as a single rebuild. The rules are:

1. **One rebuild per table.** If any change on a table requires a rebuild, all other changes on that table are folded into the same rebuild.
2. **Additive and subtractive changes merge.** A column add and a column drop on the same table become a single rebuild with the final column list.
3. **Alternative changes merge.** A type change and a NOT NULL addition on the same column become a single rebuild with the final column definition.
4. **Index changes remain separate.** Index drops and creations are not folded into the table rebuild. They are separate statements.

### Collapse Example

Given these diffs on the `products` table:

- Add column `sku` (TEXT)
- Change column `price` from `REAL` to `INTEGER`
- Add `NOT NULL` to column `name`
- Drop column `temp_field`

Without collapse, this would be four separate rebuilds. With collapse, it becomes one:

```sql
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "products_new" (
  "id" INTEGER PRIMARY KEY,
  "name" TEXT NOT NULL,
  "price" INTEGER,
  "sku" TEXT
);
INSERT INTO "products_new" ("id", "name", "price", "sku")
SELECT "id", "name", CAST("price" AS INTEGER), NULL
FROM "products";
DROP TABLE "products";
ALTER TABLE "products_new" RENAME TO "products";
COMMIT;
PRAGMA foreign_keys = ON;
```

Note how `temp_field` is absent from the `CREATE TABLE` and how `sku` is absent from the `SELECT` (receiving `NULL` instead). The `CAST` on `price` handles the type conversion. The `NOT NULL` on `name` is applied in the new table definition.

### Collapse and Defaults

When a new column has a `DEFAULT` value, the `INSERT INTO ... SELECT` uses that default for existing rows:

```sql
-- New column: sku TEXT DEFAULT 'UNKNOWN'
INSERT INTO "products_new" ("id", "name", "price", "sku")
SELECT "id", "name", CAST("price" AS INTEGER), 'UNKNOWN'
FROM "products";
```

When a column is removed, it simply does not appear in the `INSERT INTO` column list, and its data is discarded.

## Foreign Key Handling During Rebuilds

Foreign keys introduce ordering constraints. wlite handles them as follows.

### Disabling Foreign Keys

At the start of every rebuild, wlite emits `PRAGMA foreign_keys = OFF`. This is necessary because:

1. Dropping a table that is referenced by another table would fail with a foreign key violation.
2. Inserting data into a new table that has foreign key constraints referencing other tables that may not exist yet would fail.

The pragma is session-scoped and does not persist after the connection closes.

### Re-enabling Foreign Keys

At the end of the rebuild, after all tables have been rebuilt and all foreign keys are valid, wlite emits `PRAGMA foreign_keys = ON`. This restores normal enforcement.

### Ordering by Dependency

When rebuilding multiple tables with foreign key relationships, wlite determines the dependency graph:

```
orders -> customers (orders.customer_id references customers.id)
order_items -> orders (order_items.order_id references orders.id)
order_items -> products (order_items.product_id references products.id)
```

The rebuild order is:

1. `customers` (no dependencies)
2. `products` (no dependencies)
3. `orders` (depends on `customers`)
4. `order_items` (depends on `orders` and `products`)

This topological ordering ensures that when a table is rebuilt and its foreign key constraints are re-established, the referenced table already exists in its final form.

### Self-Referencing Tables

Tables that reference themselves (e.g., `categories` with `parent_id REFERENCES categories(id)`) require special handling. During the rebuild:

1. Foreign keys are disabled, so the self-reference does not block the rebuild.
2. The new table is created with the self-referencing foreign key in its definition.
3. Data is copied from the old table.
4. The old table is dropped and the new table is renamed.
5. Foreign keys are re-enabled, and the self-reference becomes active.

Self-referencing tables do not cause ordering issues because they are rebuilt in a single pass with keys disabled.

### Circular References

Circular references between two tables (e.g., `users` references `roles`, `roles` references `users`) are not possible in SQLite because foreign keys are checked at insert time, not at table creation time. However, wlite handles this by disabling foreign keys for the entire rebuild sequence covering both tables, then re-enabling them at the end.

## Index Handling

Indexes are not part of a table rebuild. They are handled separately.

### Index Drops

If an index definition changes (different columns, different expression, different uniqueness), wlite drops the old index and creates the new one:

```sql
DROP INDEX IF EXISTS "idx_users_email";
CREATE UNIQUE INDEX "idx_users_email" ON "users" ("email");
```

### Index Recreations After Rebuild

When a table is rebuilt, all indexes on that table must be recreated. This is because:

1. The old table is dropped, which destroys its indexes.
2. The new table starts with no indexes.

wlite tracks which indexes belong to which table and recreates them after the rename step:

```sql
-- After the rebuild of "users"
CREATE INDEX "idx_users_name" ON "users" ("name");
CREATE UNIQUE INDEX "idx_users_email" ON "users" ("email");
```

### Index Dependencies on Columns

If an index references a column that is being dropped, the index is also dropped. If an index references a column that is being added, the index is created after the rebuild.

### Implicit Indexes

SQLite creates an implicit index for `PRIMARY KEY` and `UNIQUE` constraints. wlite does not emit `CREATE INDEX` statements for these. The implicit index is created automatically when the table is created.

## Checksums

wlite uses checksums to verify that the migration was applied correctly.

### Schema Checksum

After generating the migration SQL, wlite computes a checksum of the target schema state. This checksum is stored in the `schema_version` metadata table. On subsequent runs, wlite recomputes the checksum and compares it to the stored value. If they differ, a new migration is needed.

### Data Checksum

During a rebuild, wlite can optionally compute a checksum of the data in the old table and compare it to the data in the new table. This verifies that the migration did not corrupt or lose data. The checksum is computed over all rows and all columns.

### Checksum Algorithm

wlite uses a rolling checksum based on the concatenation of all cell values in the table. The checksum is deterministic: the same data in the same order always produces the same checksum. Row order is significant; wlite sorts rows by primary key before computing the checksum.

## What wlite Does NOT Do

Understanding the boundaries of wlite's capabilities is as important as understanding what it does.

### Does NOT Support Column Renames

SQLite supports `ALTER TABLE ... RENAME COLUMN`, but wlite treats a rename as a drop-and-add. This means data in the renamed column is lost unless the rebuild explicitly copies it. wlite does not attempt to detect renames.

### Does NOT Optimize for Large Tables

The rebuild strategy copies the entire table. For very large tables (millions of rows), this is slow and requires significant temporary disk space. wlite does not implement incremental migration or online schema change techniques.

### Does NOT Handle Data Migration

wlite handles schema changes only. If a column change requires data transformation (e.g., splitting a single `name` column into `first_name` and `last_name`), wlite does not generate the necessary `INSERT` or `UPDATE` statements. Data migration must be handled separately.

### Does NOT Validate Foreign Key Integrity After Rebuild

After rebuilding a table and re-enabling foreign keys, wlite does not run a `PRAGMA foreign_key_check` to verify that all foreign key constraints are satisfied. If the data violates a newly added foreign key constraint, the error will surface at runtime, not during migration.

### Does NOT Handle Triggers During Rebuild

wlite recreates triggers that reference rebuilt tables, but it does not temporarily disable triggers during the rebuild. If a trigger fires during the data copy and modifies another table, this could cause unexpected behavior.

### Does NOT Generate Down Migrations

wlite generates the forward migration (from schema A to schema B). It does not generate a reverse migration (from schema B back to schema A). Rollback must be handled by restoring a backup.

### Does NOT Support Partial Indexes

SQLite supports partial indexes (indexes with a `WHERE` clause), but wlite does not generate or manage them. If a partial index is defined in the schema, wlite will include it in the rebuild, but it does not validate or transform the `WHERE` clause.

### Does NOT Handle Virtual Tables

SQLite supports virtual tables (e.g., FTS5 full-text search tables). wlite does not rebuild virtual tables. If a virtual table definition changes, wlite emits a warning and skips the change.

### Does NOT Preserve Table Statistics

SQLite maintains internal statistics about table contents that influence query planning. After a rebuild, these statistics are invalidated. wlite does not run `ANALYZE` after migration.

## Concrete Examples

### Example 1: Adding a Column

**Schema A:**
```sql
CREATE TABLE "users" (
  "id" INTEGER PRIMARY KEY,
  "name" TEXT NOT NULL
);
```

**Schema B:**
```sql
CREATE TABLE "users" (
  "id" INTEGER PRIMARY KEY,
  "name" TEXT NOT NULL,
  "email" TEXT
);
```

**Diff:** Additive change. Column `email` is new.

**Generated SQL:**
```sql
ALTER TABLE "users" ADD COLUMN "email" TEXT;
```

No rebuild is required. The operation is O(1) in SQLite (metadata only).

### Example 2: Changing a Column Type

**Schema A:**
```sql
CREATE TABLE "products" (
  "id" INTEGER PRIMARY KEY,
  "price" REAL
);
```

**Schema B:**
```sql
CREATE TABLE "products" (
  "id" INTEGER PRIMARY KEY,
  "price" INTEGER
);
```

**Diff:** Alternative change. Column `price` type changes from `REAL` to `INTEGER`.

**Generated SQL:**
```sql
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "products_new" (
  "id" INTEGER PRIMARY KEY,
  "price" INTEGER
);
INSERT INTO "products_new" ("id", "price")
SELECT "id", CAST("price" AS INTEGER)
FROM "products";
DROP TABLE "products";
ALTER TABLE "products_new" RENAME TO "products";
COMMIT;
PRAGMA foreign_keys = ON;
```

The `CAST` ensures that floating-point values are truncated to integers during the copy.

### Example 3: Adding NOT NULL

**Schema A:**
```sql
CREATE TABLE "orders" (
  "id" INTEGER PRIMARY KEY,
  "customer_id" INTEGER,
  "total" REAL DEFAULT 0
);
```

**Schema B:**
```sql
CREATE TABLE "orders" (
  "id" INTEGER PRIMARY KEY,
  "customer_id" INTEGER NOT NULL,
  "total" REAL DEFAULT 0
);
```

**Diff:** Alternative change. Column `customer_id` gains `NOT NULL`.

**Generated SQL:**
```sql
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "orders_new" (
  "id" INTEGER PRIMARY KEY,
  "customer_id" INTEGER NOT NULL,
  "total" REAL DEFAULT 0
);
INSERT INTO "orders_new" ("id", "customer_id", "total")
SELECT "id", "customer_id", "total"
FROM "orders";
DROP TABLE "orders";
ALTER TABLE "orders_new" RENAME TO "orders";
COMMIT;
PRAGMA foreign_keys = ON;
```

**Warning:** If existing rows have `NULL` values in `customer_id`, the `INSERT` will fail because the new table has a `NOT NULL` constraint. wlite does not validate data before migration. The user must ensure data integrity before applying the migration.

### Example 4: Dropping a Table

**Schema A:**
```sql
CREATE TABLE "temp_data" (
  "id" INTEGER PRIMARY KEY,
  "value" TEXT
);
```

**Schema B:**
```sql
-- (no table)
```

**Diff:** Subtractive change. Table `temp_data` is dropped.

**Generated SQL:**
```sql
DROP TABLE IF EXISTS "temp_data";
```

### Example 5: Adding an Index

**Schema A:**
```sql
CREATE TABLE "users" (
  "id" INTEGER PRIMARY KEY,
  "email" TEXT NOT NULL
);
```

**Schema B:**
```sql
CREATE TABLE "users" (
  "id" INTEGER PRIMARY KEY,
  "email" TEXT NOT NULL
);
CREATE UNIQUE INDEX "idx_users_email" ON "users" ("email");
```

**Diff:** Additive change. Index `idx_users_email` is new.

**Generated SQL:**
```sql
CREATE UNIQUE INDEX "idx_users_email" ON "users" ("email");
```

### Example 6: Modifying an Index

**Schema A:**
```sql
CREATE INDEX "idx_products_name" ON "products" ("name");
```

**Schema B:**
```sql
CREATE UNIQUE INDEX "idx_products_name" ON "products" ("name");
```

**Diff:** Alternative change. Index gains `UNIQUE`.

**Generated SQL:**
```sql
DROP INDEX IF EXISTS "idx_products_name";
CREATE UNIQUE INDEX "idx_products_name" ON "products" ("name");
```

### Example 7: Adding a CHECK Constraint

**Schema A:**
```sql
CREATE TABLE "inventory" (
  "id" INTEGER PRIMARY KEY,
  "quantity" INTEGER NOT NULL
);
```

**Schema B:**
```sql
CREATE TABLE "inventory" (
  "id" INTEGER PRIMARY KEY,
  "quantity" INTEGER NOT NULL,
  CONSTRAINT "chk_quantity_positive" CHECK ("quantity" >= 0)
);
```

**Diff:** Additive change. New `CHECK` constraint on table.

**Generated SQL:**
```sql
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "inventory_new" (
  "id" INTEGER PRIMARY KEY,
  "quantity" INTEGER NOT NULL,
  CONSTRAINT "chk_quantity_positive" CHECK ("quantity" >= 0)
);
INSERT INTO "inventory_new" ("id", "quantity")
SELECT "id", "quantity"
FROM "inventory";
DROP TABLE "inventory";
ALTER TABLE "inventory_new" RENAME TO "inventory";
COMMIT;
PRAGMA foreign_keys = ON;
```

Note: Even though this is technically an additive change to the table, it requires a rebuild because SQLite does not support `ALTER TABLE ... ADD CONSTRAINT`.

### Example 8: Multiple Changes on One Table

**Schema A:**
```sql
CREATE TABLE "sessions" (
  "id" INTEGER PRIMARY KEY,
  "user_id" INTEGER,
  "token" TEXT,
  "expires_at" TEXT,
  "ip_address" TEXT
);
```

**Schema B:**
```sql
CREATE TABLE "sessions" (
  "id" INTEGER PRIMARY KEY,
  "user_id" INTEGER NOT NULL,
  "token" TEXT NOT NULL,
  "created_at" TEXT NOT NULL,
  "expires_at" TEXT
);
```

**Diff:**
- `user_id`: add `NOT NULL` (alternative)
- `token`: add `NOT NULL` (alternative)
- `expires_at`: no change
- `ip_address`: drop (subtractive)
- `created_at`: add with `NOT NULL` (additive + alternative)

**Collapse:** All changes collapse into a single rebuild.

**Generated SQL:**
```sql
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "sessions_new" (
  "id" INTEGER PRIMARY KEY,
  "user_id" INTEGER NOT NULL,
  "token" TEXT NOT NULL,
  "created_at" TEXT NOT NULL,
  "expires_at" TEXT
);
INSERT INTO "sessions_new" ("id", "user_id", "token", "created_at", "expires_at")
SELECT "id", "user_id", "token", NULL, "expires_at"
FROM "sessions";
DROP TABLE "sessions";
ALTER TABLE "sessions_new" RENAME TO "sessions";
COMMIT;
PRAGMA foreign_keys = ON;
```

### Example 9: Rebuild with Foreign Keys

**Schema A:**
```sql
CREATE TABLE "authors" (
  "id" INTEGER PRIMARY KEY,
  "name" TEXT NOT NULL
);
CREATE TABLE "books" (
  "id" INTEGER PRIMARY KEY,
  "author_id" INTEGER,
  "title" TEXT NOT NULL
);
```

**Schema B:**
```sql
CREATE TABLE "authors" (
  "id" INTEGER PRIMARY KEY,
  "name" TEXT NOT NULL,
  "bio" TEXT
);
CREATE TABLE "books" (
  "id" INTEGER PRIMARY KEY,
  "author_id" INTEGER NOT NULL,
  "title" TEXT NOT NULL
);
```

**Diff:**
- `authors`: add column `bio` (additive, but also has FK dependency)
- `books`: add `NOT NULL` to `author_id` (requires rebuild)

**Generated SQL:**
```sql
-- Step 1: authors (additive, but rebuild not strictly needed)
ALTER TABLE "authors" ADD COLUMN "bio" TEXT;

-- Step 2: books (rebuild required)
PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;
CREATE TABLE "books_new" (
  "id" INTEGER PRIMARY KEY,
  "author_id" INTEGER NOT NULL,
  "title" TEXT NOT NULL
);
INSERT INTO "books_new" ("id", "author_id", "title")
SELECT "id", "author_id", "title"
FROM "books";
DROP TABLE "books";
ALTER TABLE "books_new" RENAME TO "books";
CREATE INDEX "idx_books_author_id" ON "books" ("author_id");
COMMIT;
PRAGMA foreign_keys = ON;
```

Because `authors` only gained a new column (additive), it does not require a rebuild. The `books` table is rebuilt independently. Foreign keys are disabled during the `books` rebuild to avoid any intermediate constraint violations.

## Summary

The migration pipeline in wlite follows this flow:

1. **Diff generation** produces a set of classified changes.
2. **Classification** assigns each change to additive, subtractive, alternative, or rebuild.
3. **Collapse logic** merges changes on the same table into a single rebuild.
4. **Planning** orders the operations respecting foreign key dependencies and SQLite constraints.
5. **Expansion** converts each rebuild into the full SQL sequence.
6. **Execution** runs the SQL within a transaction with foreign keys disabled.
7. **Checksums** verify the final state matches the target schema.

This approach trades execution speed for correctness. Every migration is safe, atomic, and idempotent. The tradeoff is acceptable because schema migrations are infrequent operations where correctness matters more than speed.
