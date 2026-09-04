/*
 * Example: C++ application using wlite.hpp
 *
 * Build:
 *   g++ -std=c++17 -I../../include -I../../bindings/cpp/include -o todo main.cpp -L../../. -lwlite -lsqlite3
 *   LD_LIBRARY_PATH=../../. ./todo
 */

#include <iostream>
#include <wlite/wlite.hpp>

int main() {
    try {
        auto model = wlite::Model::load("app.wlite");
        model.validate();
        std::cout << "Model loaded: " << model.table_count() << " table(s)\n";

        auto db = wlite::Database::open("todo.db");
        db.migrate(model);
        std::cout << "Migration complete.\n";

        db.execute("INSERT INTO todos (title, completed, created_at) VALUES ('Buy groceries', 0, strftime('%s','now'))");

        auto stmt = db.prepare("SELECT id, title, completed FROM todos");
        std::cout << "\nTodos:\n";
        while (stmt.step()) {
            std::cout << "  [" << (stmt.column_int64(2) ? "x" : " ") << "] "
                      << stmt.column_text(1) << " (id=" << stmt.column_int64(0) << ")\n";
        }

        // Transaction with savepoint
        {
            auto tx = wlite::Database::Transaction(db);
            tx.savepoint("sp1");
            db.execute("INSERT INTO todos (title, completed, created_at) VALUES ('Test', 0, strftime('%s','now'))");
            tx.rollback_to("sp1");
            tx.release("sp1");
            tx.commit();
        }

        std::cout << "\nDone!\n";
    } catch (const wlite::Error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
