// Example: Go application using wlite-go
//
// Build:
//   CGO_ENABLED=1 go run main.go

package main

import (
	"fmt"
	"log"

	wlite "github.com/dbwarden/wlite-go"
)

func main() {
	// Load model
	model, err := wlite.LoadModel("app.wlite")
	if err != nil {
		log.Fatal(err)
	}
	defer model.Close()

	if err := model.Validate(); err != nil {
		log.Fatal(err)
	}
	fmt.Printf("Model loaded: %d table(s)\n", model.TableCount())

	// Open database
	db, err := wlite.Open("todo.db")
	if err != nil {
		log.Fatal(err)
	}
	defer db.Close()

	// Migrate
	if err := db.Migrate(model); err != nil {
		log.Fatal(err)
	}
	fmt.Println("Migration complete.")

	// Insert
	db.Execute("INSERT INTO todos (title, completed, created_at) VALUES ('Buy groceries', 0, strftime('%s','now'))")

	// Query
	rows, err := db.Query("SELECT id, title, completed FROM todos")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("\nTodos:")
	for _, row := range rows {
		status := " "
		if row["completed"] != nil && row["completed"].(int64) != 0 {
			status = "x"
		}
		fmt.Printf("  [%s] %v (id=%v)\n", status, row["title"], row["id"])
	}

	fmt.Println("\nDone!")
}
