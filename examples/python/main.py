#!/usr/bin/env python3
"""Example: Python application using wlite."""

import wlite

# Load model
model = wlite.Model.load("app.wlite")
model.validate()
print(f"Model loaded: {model.table_count()} table(s)")

# Open database
db = wlite.Database.open("todo.db")
db.migrate(model)
print("Migration complete.")

# Insert a todo
db.execute(
    "INSERT INTO todos (title, completed, created_at) VALUES (?, 0, strftime('%s','now'))",
    ("Buy groceries",),
)

# Query todos
rows = db.query("SELECT id, title, completed FROM todos")
print("\nTodos:")
for row in rows:
    status = "x" if row["completed"] else " "
    print(f"  [{status}] {row['title']} (id={row['id']})")

# Transaction with context manager
with db.transaction() as tx:
    db.execute(
        "INSERT INTO todos (title, completed, created_at) VALUES (?, 0, strftime('%s','now'))",
        ("Read documentation",),
    )

print("\nDone!")
db.close()
