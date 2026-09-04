/*
 * main.c — wlite CLI entry point
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wlite/wlite.h>

static int stdout_write(wlite_writer *wr, const char *data, size_t len) {
    (void)wr; return (int)fwrite(data, 1, len, stdout);
}
static wlite_writer stdout_writer = { .ctx = NULL, .write = stdout_write };

static void print_diff_summary(WlDiff *diff) {
    if (!diff) return;
    const char *op_names[] = {
        "ADD TABLE", "DROP TABLE", "RENAME TABLE",
        "ADD COLUMN", "DROP COLUMN", "RENAME COLUMN", "ALTER COLUMN",
        "ADD INDEX", "DROP INDEX", "ALTER INDEX",
        "ADD CHECK", "DROP CHECK", "ADD UNIQUE", "DROP UNIQUE",
        "ADD FK", "DROP FK", "ALTER TABLE OPTIONS", "ALTER VIEW",
        "ALTER TRIGGER", "REBUILD TABLE"
    };
    const char *sn[] = {"SAFE", "REBUILD", "DESTRUCTIVE", "CONDITIONAL", "IRREVERSIBLE"};
    for (size_t i = 0; i < diff->entry_count; i++) {
        WlDiffEntry *e = &diff->entries[i];
        printf("  %-20s %s", op_names[e->op], e->table ? e->table : "");
        if (e->object) printf(".%s", e->object);
        printf("  [%s]", sn[e->safety]);
        if (e->detail) printf(" — %s", e->detail);
        printf("\n");
    }
    printf("\n%zu change(s)\n", diff->entry_count);
}

static wlite_db *open_db(const char *path) {
    wlite_db *db = NULL;
    if (wlite_open(path, &db) != WLITE_OK) return NULL;
    return db;
}

static int cmd_init(void) {
    FILE *f = fopen("schema.wlite", "r");
    if (f) { fclose(f); printf("schema.wlite already exists\n"); return 0; }
    f = fopen("schema.wlite", "w");
    if (!f) { perror("create schema.wlite"); return 1; }
    fprintf(f, "# wlite schema\n\ndatabase {\n}\n");
    fclose(f); printf("Created schema.wlite\n");
    mkdir("migrations", 0755); printf("Created migrations/\n");
    return 0;
}

static int cmd_inspect(const char *dbpath, int json_mode) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_error *err = NULL;
    WlSchema *schema = wl_schema_inspect(db, &err);
    wlite_close(db);
    if (!schema) { fprintf(stderr, "Introspection failed: %s\n", err ? err->message : "?");
        wlite_error_free(err); return 1; }
    if (json_mode) wl_schema_write_json(schema, &stdout_writer, NULL);
    else wl_schema_write_dsl(schema, &stdout_writer, NULL);
    wl_schema_free(schema); return 0;
}

static int cmd_diff(const char *dbpath, const char *schemapath, int json_mode) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_error *err = NULL;
    WlSchema *db_schema = wl_schema_inspect(db, &err);
    wlite_close(db);
    if (!db_schema) { fprintf(stderr, "Introspection failed\n"); wlite_error_free(err); return 1; }
    WlSchema *wl_schema = wl_schema_load(schemapath, &err);
    if (!wl_schema) { fprintf(stderr, "Schema parse failed: %s\n", err ? err->message : "?");
        wlite_error_free(err); wl_schema_free(db_schema); return 1; }
    WlDiff *diff = wl_schema_diff(db_schema, wl_schema, &err);
    wl_schema_free(db_schema); wl_schema_free(wl_schema);
    if (!diff) { fprintf(stderr, "Diff failed\n"); wlite_error_free(err); return 1; }
    if (json_mode) {
        const char *op_names[] = {
            "ADD_TABLE", "DROP_TABLE", "RENAME_TABLE",
            "ADD_COLUMN", "DROP_COLUMN", "RENAME_COLUMN", "ALTER_COLUMN",
            "ADD_INDEX", "DROP_INDEX", "ALTER_INDEX",
            "ADD_CHECK", "DROP_CHECK", "ADD_UNIQUE", "DROP_UNIQUE",
            "ADD_FKEY", "DROP_FKEY", "ALTER_TABLE_OPTIONS", "ALTER_VIEW",
            "ALTER_TRIGGER", "REBUILD_TABLE"
        };
        const char *sn[] = {"SAFE", "REBUILD", "DESTRUCTIVE", "CONDITIONAL", "IRREVERSIBLE"};
        printf("{\"change_count\": %zu, \"changes\": [", diff->entry_count);
        for (size_t i = 0; i < diff->entry_count; i++) {
            WlDiffEntry *e = &diff->entries[i];
            if (i > 0) printf(",");
            printf("{\"op\": \"%s\", \"table\": \"%s\"", op_names[e->op], e->table ? e->table : "");
            if (e->object) printf(", \"object\": \"%s\"", e->object);
            printf(", \"safety\": \"%s\"", sn[e->safety]);
            if (e->detail) printf(", \"detail\": \"%s\"", e->detail);
            printf("}");
        }
        printf("]}\n");
    } else if (diff->entry_count == 0) printf("Schemas are identical.\n");
    else print_diff_summary(diff);
    int rc = (diff->entry_count > 0) ? 1 : 0;
    wl_diff_free(diff); return rc;
}

static int cmd_plan(const char *dbpath, const char *schemapath, int json_mode) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_error *err = NULL;
    WlSchema *db_schema = wl_schema_inspect(db, &err);
    wlite_close(db);
    if (!db_schema) { fprintf(stderr, "Introspection failed\n"); wlite_error_free(err); return 1; }
    WlSchema *wl_schema = wl_schema_load(schemapath, &err);
    if (!wl_schema) { fprintf(stderr, "Schema parse failed: %s\n", err ? err->message : "?");
        wlite_error_free(err); wl_schema_free(db_schema); return 1; }
    WlPlan *plan = wl_plan_migration(db_schema, wl_schema, &err);
    wl_schema_free(db_schema); wl_schema_free(wl_schema);
    if (!plan) { fprintf(stderr, "Plan failed: %s\n", err ? err->message : "?"); wlite_error_free(err); return 1; }
    if (json_mode) {
        const char *op_names[] = {"CREATE_TABLE","DROP_TABLE","RENAME_TABLE","ADD_COLUMN","DROP_COLUMN",
            "RENAME_COLUMN","ALTER_COLUMN","REBUILD_TABLE","CREATE_INDEX","DROP_INDEX",
            "ADD_CHECK","DROP_CHECK","ADD_UNIQUE","DROP_UNIQUE","ADD_FKEY","DROP_FKEY","CUSTOM"};
        const char *sn[] = {"SAFE", "REBUILD", "DESTRUCTIVE", "CONDITIONAL", "IRREVERSIBLE"};
        printf("{\"step_count\": %zu, \"steps\": [", plan->step_count);
        for (size_t i = 0; i < plan->step_count; i++) {
            WlPlanStep *s = &plan->steps[i];
            if (i > 0) printf(",");
            printf("{\"type\": \"%s\"", op_names[s->op]);
            if (s->table) printf(", \"table\": \"%s\"", s->table);
            printf(", \"safety\": \"%s\"", sn[s->safety]);
            if (s->sql) printf(", \"sql\": \"%s\"", s->sql);
            if (s->detail) printf(", \"detail\": \"%s\"", s->detail);
            if (s->is_non_atomic) printf(", \"non_atomic\": true");
            printf("}");
        }
        printf("]}\n");
    } else {
        printf("PLAN (%zu steps)\n\n", plan->step_count);
        const char *op_names[] = {"CREATE TABLE","DROP TABLE","RENAME TABLE","ADD COLUMN","DROP COLUMN",
            "RENAME COLUMN","ALTER COLUMN","REBUILD TABLE","CREATE INDEX","DROP INDEX",
            "ADD CHECK","DROP CHECK","ADD UNIQUE","DROP UNIQUE","ADD FK","DROP FK","CUSTOM"};
        for (size_t i = 0; i < plan->step_count; i++) {
            WlPlanStep *s = &plan->steps[i];
            printf("  %zu. %s %s", i+1, op_names[s->op], s->table ? s->table : "");
            if (s->detail) printf(" — %s", s->detail);
            if (s->safety) printf(" [%s]", s->safety == WL_SAFETY_REQUIRES_REBUILD ? "REBUILD" : "DESTRUCTIVE");
            if (s->is_non_atomic) printf(" [NON-ATOMIC]");
            printf("\n");
        }
    }
    wl_plan_free(plan); return 0;
}

static int cmd_generate(const char *dbpath, const char *schemapath, const char *name, int yes) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_error *err = NULL;
    WlSchema *db_schema = wl_schema_inspect(db, &err); wlite_close(db);
    if (!db_schema) { fprintf(stderr, "Introspection failed\n"); return 1; }
    WlSchema *wl_schema = wl_schema_load(schemapath, &err);
    if (!wl_schema) { fprintf(stderr, "Schema parse failed\n"); wl_schema_free(db_schema); return 1; }
    WlPlan *plan = wl_plan_migration(db_schema, wl_schema, &err);
    wl_schema_free(db_schema); wl_schema_free(wl_schema);
    if (!plan) { fprintf(stderr, "Plan failed\n"); return 1; }
    if (plan->step_count == 0) { printf("No changes.\n"); wl_plan_free(plan); return 0; }
    printf("Migration plan (%zu steps):\n", plan->step_count);
    if (!yes) { printf("Create? [y/N] "); if (getchar() != 'y') { wl_plan_free(plan); return 0; } }
    char slug[128]; int si = 0;
    if (!name) name = "migration";
    for (const char *p = name; *p && si < 127; p++) {
        if (*p == ' ' || *p == '-') slug[si++] = '_';
        else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) slug[si++] = *p;
    }
    slug[si] = '\0';
    char filename[256]; snprintf(filename, sizeof(filename), "migrations/0001_%s.sql", slug);
    FILE *f = fopen(filename, "w");
    if (!f) { perror("create migration"); wl_plan_free(plan); return 1; }
    fprintf(f, "-- wlite migration\n-- name: %s\n\n-- upgrade\n\n", name);
    for (size_t i = 0; i < plan->step_count; i++)
        if (plan->steps[i].sql) fprintf(f, "%s\n\n", plan->steps[i].sql);
    fprintf(f, "-- rollback\n\n");
    for (size_t i = plan->step_count; i > 0; i--)
        if (plan->steps[i-1].rollback_sql) fprintf(f, "%s\n\n", plan->steps[i-1].rollback_sql);
    fclose(f); printf("Created %s\n", filename);
    wl_plan_free(plan); return 0;
}

static int cmd_migrate(const char *dbpath, const char *schemapath, int force) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_model *model = NULL;
    wlite_result rc = wlite_model_load_file(schemapath, &model);
    if (rc != WLITE_OK) { fprintf(stderr, "Schema parse failed: %s\n", wlite_strerror(rc)); wlite_close(db); return 1; }

    /* Check for destructive operations before applying */
    WlPlan *plan = NULL;
    rc = wlite_diff(db, model, &plan);
    if (rc == WLITE_OK && plan) {
        int has_destructive = 0;
        int has_rebuild = 0;
        for (size_t i = 0; i < plan->step_count; i++) {
            if (plan->steps[i].safety == WL_SAFETY_DESTRUCTIVE) has_destructive = 1;
            if (plan->steps[i].safety == WL_SAFETY_REQUIRES_REBUILD) has_rebuild = 1;
        }
        if ((has_destructive || has_rebuild) && !force) {
            printf("Migration contains ");
            if (has_destructive) printf("DESTRUCTIVE");
            else printf("REBUILD");
            printf(" operations:\n");
            for (size_t i = 0; i < plan->step_count; i++) {
                WlPlanStep *s = &plan->steps[i];
                if (s->safety == WL_SAFETY_DESTRUCTIVE || s->safety == WL_SAFETY_REQUIRES_REBUILD) {
                    printf("  %s %s", s->table ? s->table : "", s->detail ? s->detail : "");
                    printf(" [%s]\n", s->safety == WL_SAFETY_DESTRUCTIVE ? "DESTRUCTIVE" : "REBUILD");
                }
            }
            printf("\nApply anyway? [y/N] ");
            int ch = getchar();
            if (ch != 'y' && ch != 'Y') {
                printf("Aborted.\n");
                wl_plan_free(plan);
                wlite_model_free(model);
                wlite_close(db);
                return 0;
            }
        }
        wl_plan_free(plan);
    }

    rc = wlite_migrate(db, model);
    wlite_model_free(model);
    if (rc != WLITE_OK) { fprintf(stderr, "Migration failed: %s\n", wlite_strerror(rc)); wlite_close(db); return 1; }
    printf("Migration applied successfully.\n");
    wlite_close(db); return 0;
}

static int cmd_rollback(const char *dbpath, int steps) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    for (int i = 0; i < steps; i++) {
        wlite_error *err = NULL;
        wlite_result rc = wl_rollback_last(db, &err);
        if (rc != WLITE_OK) { fprintf(stderr, "Rollback failed: %s\n", err ? err->message : "?");
            wlite_error_free(err); wlite_close(db); return 1; }
    }
    wlite_close(db); printf("Rolled back %d migration(s).\n", steps); return 0;
}

static int cmd_status(const char *dbpath) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    WlSchema *schema = wl_schema_inspect(db, NULL);
    char *hash = schema ? wl_schema_hash(schema) : NULL;
    printf("Database: %s\n", dbpath);
    if (hash) { printf("Schema hash: %s\n", hash); free(hash); }
    if (schema) wl_schema_free(schema);
    wlite_close(db); return 0;
}

static int cmd_check(const char *dbpath, const char *schemapath) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_error *err = NULL;
    WlSchema *expected = wl_schema_load(schemapath, &err);
    if (!expected) { fprintf(stderr, "Schema parse failed\n"); wlite_close(db); return 1; }
    WlDiff *diff = NULL;
    wlite_result rc = wl_schema_verify(db, expected, &diff, &err);
    wl_schema_free(expected); wlite_close(db);
    if (rc == WLITE_OK) { printf("Schema check: OK\n"); return 0; }
    if (rc == WLITE_NOT_FOUND) { printf("Schema check: FAILED\n\n"); print_diff_summary(diff); wl_diff_free(diff); return 1; }
    fprintf(stderr, "Verification failed: %s\n", err ? err->message : "?"); wlite_error_free(err); return 2;
}

static int cmd_snapshot(const char *dbpath, int json_mode) { return cmd_inspect(dbpath, json_mode); }

static int cmd_hash(const char *dbpath) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    WlSchema *schema = wl_schema_inspect(db, NULL); wlite_close(db);
    if (!schema) { fprintf(stderr, "Introspection failed\n"); return 1; }
    char *hash = wl_schema_hash(schema); printf("%s\n", hash ? hash : "?");
    free(hash); wl_schema_free(schema); return 0;
}

static int cmd_format(const char *schemapath) {
    wlite_error *err = NULL;
    WlSchema *schema = wl_schema_load(schemapath, &err);
    if (!schema) { fprintf(stderr, "Parse failed: %s\n", err ? err->message : "?"); wlite_error_free(err); return 1; }
    wl_schema_write_dsl(schema, &stdout_writer, NULL); wl_schema_free(schema); return 0;
}

static int cmd_compile(const char *schemapath, const char *outpath) {
    wlite_error *err = NULL;
    WlSchema *schema = wl_schema_load(schemapath, &err);
    if (!schema) { fprintf(stderr, "Parse failed: %s\n", err ? err->message : "?"); wlite_error_free(err); return 1; }
    int rc = wl_model_compile(schema, outpath);
    wl_schema_free(schema);
    if (rc != 0) { fprintf(stderr, "Compile failed\n"); return 1; }
    printf("Compiled %s -> %s\n", schemapath, outpath);
    return 0;
}

static int cmd_query(const char *dbpath, const char *sql, const char *format) {
    wlite_db *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "Cannot open %s\n", dbpath); return 1; }
    wlite_stmt *stmt = NULL;
    wlite_result rc = wlite_prepare(db, sql, &stmt);
    if (rc != WLITE_OK) { fprintf(stderr, "Query failed: %s\n", wlite_strerror(rc)); wlite_close(db); return 1; }
    int cols = wlite_column_count(stmt);
    /* Print header */
    if (strcmp(format, "table") == 0 || strcmp(format, "") == 0) {
        for (int i = 0; i < cols; i++) {
            if (i > 0) printf("\t");
            printf("%s", wlite_column_name(stmt, i));
        }
        printf("\n");
    }
    /* Print rows */
    while (wlite_step(stmt) == WLITE_OK) {
        if (strcmp(format, "json") == 0) {
            printf("{");
            for (int i = 0; i < cols; i++) {
                if (i > 0) printf(", ");
                printf("\"%s\": ", wlite_column_name(stmt, i));
                wlite_value_type vt = wlite_column_type(stmt, i);
                if (vt == WLITE_TYPE_NULL) printf("null");
                else if (vt == WLITE_TYPE_INTEGER) printf("%lld", (long long)wlite_column_int64(stmt, i));
                else if (vt == WLITE_TYPE_REAL) printf("%g", wlite_column_double(stmt, i));
                else if (vt == WLITE_TYPE_TEXT) printf("\"%s\"", wlite_column_text(stmt, i));
                else printf("\"blob\"");
            }
            printf("}\n");
        } else {
            for (int i = 0; i < cols; i++) {
                if (i > 0) printf("\t");
                wlite_value_type vt = wlite_column_type(stmt, i);
                if (vt == WLITE_TYPE_NULL) printf("NULL");
                else if (vt == WLITE_TYPE_INTEGER) printf("%lld", (long long)wlite_column_int64(stmt, i));
                else if (vt == WLITE_TYPE_REAL) printf("%g", wlite_column_double(stmt, i));
                else if (vt == WLITE_TYPE_TEXT) printf("%s", wlite_column_text(stmt, i));
                else printf("[blob]");
            }
            printf("\n");
        }
    }
    wlite_stmt_finalize(stmt);
    wlite_close(db);
    return 0;
}

static void usage(void) {
    fprintf(stderr,
        "wlite — SQLite schema and migration toolkit\n\n"
        "Commands:\n"
        "  init                           Create schema.wlite + migrations/\n"
        "  migrate <db> <schema> [--force]  Apply migrations to database\n"
        "  inspect <db> [--json]          Show database schema\n"
        "  diff <db> <schema> [--json]    Compare database against schema\n"
        "  plan <db> <schema> [--json]    Show migration plan\n"
        "  generate <db> <schema>         Generate migration SQL\n"
        "  rollback <db> [--steps N]      Rollback migrations\n"
        "  status <db>                    Show schema hash\n"
        "  check <db> <schema>            Verify schema matches\n"
        "  compile <schema> [-o file]     Compile .wlite to .wlitem binary\n"
        "  query <db> <sql> [--json]      Execute SQL query\n"
        "  snapshot <db> [--json]         Export schema\n"
        "  hash <db>                      Show schema hash\n"
        "  format <schema>                Format schema.wlite\n"
        "  version                        Show version\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }
    const char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) return cmd_init();
    if (strcmp(cmd, "migrate") == 0) { if (argc<4) { usage(); return 1; }
        int force = 0;
        for (int i=4;i<argc;i++) if (strcmp(argv[i],"--force")==0) force=1;
        return cmd_migrate(argv[2], argv[3], force); }
    if (strcmp(cmd, "version") == 0) { printf("wlite %d.%d.%d\n", WLITE_VERSION_MAJOR, WLITE_VERSION_MINOR, WLITE_VERSION_PATCH); return 0; }
    if (strcmp(cmd, "inspect") == 0) { if (argc<3) { usage(); return 1; } return cmd_inspect(argv[2], argc>3 && strcmp(argv[3],"--json")==0); }
    if (strcmp(cmd, "diff") == 0) { if (argc<4) { usage(); return 1; } return cmd_diff(argv[2], argv[3], argc>4 && strcmp(argv[4],"--json")==0); }
    if (strcmp(cmd, "plan") == 0) { if (argc<4) { usage(); return 1; } return cmd_plan(argv[2], argv[3], argc>4 && strcmp(argv[4],"--json")==0); }
    if (strcmp(cmd, "generate") == 0) { if (argc<4) { usage(); return 1; }
        const char *name = NULL; int yes = 0;
        for (int i=4;i<argc;i++) { if (strcmp(argv[i],"--name")==0 && i+1<argc) name=argv[++i]; if (strcmp(argv[i],"--yes")==0) yes=1; }
        return cmd_generate(argv[2], argv[3], name, yes); }
    if (strcmp(cmd, "rollback") == 0) { if (argc<3) { usage(); return 1; }
        int steps=1; for (int i=3;i<argc;i++) if (strcmp(argv[i],"--steps")==0 && i+1<argc) steps=atoi(argv[++i]);
        return cmd_rollback(argv[2], steps); }
    if (strcmp(cmd, "status") == 0) { if (argc<3) { usage(); return 1; } return cmd_status(argv[2]); }
    if (strcmp(cmd, "check") == 0) { if (argc<4) { usage(); return 1; } return cmd_check(argv[2], argv[3]); }
    if (strcmp(cmd, "snapshot") == 0) { if (argc<3) { usage(); return 1; } return cmd_snapshot(argv[2], argc>3 && strcmp(argv[3],"--json")==0); }
    if (strcmp(cmd, "hash") == 0) { if (argc<3) { usage(); return 1; } return cmd_hash(argv[2]); }
    if (strcmp(cmd, "format") == 0) { if (argc<3) { usage(); return 1; } return cmd_format(argv[2]); }
    if (strcmp(cmd, "compile") == 0) { if (argc<3) { usage(); return 1; }
        const char *out = "schema.json";
        for (int i=3;i<argc;i++) if (strcmp(argv[i],"-o")==0 && i+1<argc) out=argv[++i];
        return cmd_compile(argv[2], out); }
    if (strcmp(cmd, "query") == 0) { if (argc<4) { usage(); return 1; }
        const char *fmt = "table";
        for (int i=4;i<argc;i++) if (strcmp(argv[i],"--json")==0) fmt="json";
        return cmd_query(argv[2], argv[3], fmt); }
    fprintf(stderr, "Unknown command: %s\n", cmd); usage(); return 1;
}
