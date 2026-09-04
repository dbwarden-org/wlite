const std = @import("std");
const build = std.build;

pub fn build(b: *build.Builder) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "wlite",
        .root_source_file = .{ .path = "src/wlite.zig" },
        .target = target,
        .optimize = optimize,
    });

    // Link against libwlite (pre-built)
    lib.addLibraryPath(.{ .path = "../../." });
    lib.linkSystemLibrary("wlite");
    lib.linkSystemLibrary("sqlite3");
    lib.addIncludePath(.{ .path = "../../include" });

    b.installArtifact(lib);

    const main_tests = b.addTest(.{
        .root_source_file = .{ .path = "src/wlite.zig" },
        .target = target,
        .optimize = optimize,
    });

    const run_tests = b.addRunArtifact(main_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_tests.step);
}
