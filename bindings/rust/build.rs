use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let libwlite_dir = manifest_dir.join("..").join("libwlite");

    // Build libwlite from source if not installed
    let src_dir = libwlite_dir.join("wlite");
    let sources: Vec<PathBuf> = [
        "schema.c", "parser.c", "introspect.c", "diff.c", "planner.c",
        "migrate.c", "serialize.c", "query.c", "record.c", "tx.c", "schema_inspect.c", "compile.c",
    ].iter().map(|f| src_dir.join(f)).collect();

    cc::Build::new()
        .files(&sources)
        .include(libwlite_dir.join("include"))
        .include(libwlite_dir.join("wlite"))
        .flag("-std=c11")
        .compile("wlite");

    // Link SQLite
    if pkg_config::probe_library("sqlite3").is_err() {
        println!("cargo:rustc-link-lib=sqlite3");
    }

    println!("cargo:rustc-link-lib=static=wlite");
    println!("cargo:include={}", libwlite_dir.join("include").display());
}
