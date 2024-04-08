const std = @import("std");

const MicrokitBoard = enum {
    qemu_arm_virt,

    fn platform(board: MicrokitBoard) Platform {
        for (supported_platforms) |p| {
            if (board == p.board) {
                return p;
            }
        }

        std.log.err("Platform '{}' is not supported\n", .{ board });
        std.posix.exit(1);
    }
};

const Platform = struct {
    /// Microkit name of board we are targetting
    board: MicrokitBoard,
    /// What directory/manufacturer the block driver is under
    blk_driver_dir: []const u8,
    target: std.zig.CrossTarget,
};

const supported_platforms = [_]Platform {
    .{
        .board = MicrokitBoard.qemu_arm_virt,
        .blk_driver_dir = "virtio",
        .target = std.zig.CrossTarget{
            .cpu_arch = .aarch64,
            .cpu_model = .{ .explicit = &std.Target.arm.cpu.cortex_a53 },
            .os_tag = .freestanding,
            .abi = .none,
        },
    },
};

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});

    // Getting the path to the Microkit SDK before doing anything else
    const microkit_sdk_arg = b.option([]const u8, "sdk", "Path to Microkit SDK");
    if (microkit_sdk_arg == null) {
        std.log.err("Missing -Dsdk=/path/to/sdk argument being passed\n", .{});
        std.posix.exit(1);
    }
    const microkit_sdk = microkit_sdk_arg.?;

    const microkit_config = b.option([]const u8, "config", "Microkit config to build for") orelse "debug";

    // Get the Microkit SDK board we want to target
    const microkit_board_option = b.option(MicrokitBoard, "board", "Microkit board to target")
                                  orelse MicrokitBoard.qemu_arm_virt;
    const platform = microkit_board_option.platform();
    const target = b.resolveTargetQuery(platform.target);
    const microkit_board = @tagName(platform.board);

    // Since we are relying on Zig to produce the final ELF, it needs to do the
    // linking step as well.
    const microkit_board_dir = b.fmt("{s}/board/{s}/{s}", .{ microkit_sdk, microkit_board, microkit_config });
    const microkit_tool = b.fmt("{s}/bin/microkit", .{ microkit_sdk });
    const libmicrokit = b.fmt("{s}/lib/libmicrokit.a", .{ microkit_board_dir });
    const libmicrokit_linker_script = b.fmt("{s}/lib/microkit.ld", .{ microkit_board_dir });
    const libmicrokit_include: std.Build.LazyPath = .{ .path = b.fmt("{s}/include", .{ microkit_board_dir }) };

    const sddf_include = std.Build.LazyPath.relative("../../include");
    const driver_include = std.Build.LazyPath.relative(b.fmt("../../drivers/blk/{s}/", .{ platform.blk_driver_dir }));

    const printf = b.addStaticLibrary(.{
        .name = "printf",
        .target = target,
        .optimize = optimize,
    });
    printf.addCSourceFiles(.{
        .files = &.{ "../../util/printf.c", "../../util/putchar_debug.c" },
    });
    printf.addIncludePath(sddf_include);
    printf.addIncludePath(libmicrokit_include);

    const driver = b.addExecutable(.{
        .name = "driver.elf",
        .target = target,
        .optimize = optimize,
        .strip = false,
    });
    // Microkit specific linking and includes
    driver.addIncludePath(libmicrokit_include);
    driver.addObjectFile(.{ .path = libmicrokit });
    driver.setLinkerScriptPath(.{ .path = libmicrokit_linker_script });
    // Project specific linking and includes
    driver.linkLibrary(printf);
    driver.addIncludePath(sddf_include);
    driver.addIncludePath(driver_include);
    driver.addCSourceFiles(.{
        .files = &.{ b.fmt("../../drivers/blk/{s}/block.c", .{ platform.blk_driver_dir }) },
        .flags = &.{
            "-Wall",
            "-Werror",
            "-Wno-unused-function",
            "-mstrict-align",
        }
    });
    b.installArtifact(driver);

    const client = b.addExecutable(.{
        .name = "client.elf",
        .target = target,
        .optimize = optimize,
        .strip = false,
    });
    // Microkit specific linking and includes
    client.addIncludePath(libmicrokit_include);
    client.addObjectFile(.{ .path = libmicrokit });
    client.setLinkerScriptPath(.{ .path = libmicrokit_linker_script });
    // Project specific linking and includes
    client.linkLibrary(printf);
    client.addIncludePath(sddf_include);
    client.addCSourceFiles(.{
        .files = &.{"client.c"},
        .flags = &.{
            "-Wall",
            "-Werror",
            "-Wno-unused-function",
            "-mstrict-align",
        }
    });
    b.installArtifact(client);

    const system_description_path = b.fmt("board/{s}/block.system", .{ microkit_board });
    const final_image_dest = b.getInstallPath(.bin, "./loader.img");
    const microkit_tool_cmd = b.addSystemCommand(&[_][]const u8{
       microkit_tool,
       system_description_path,
       "--search-path",
       b.getInstallPath(.bin, ""),
       "--board",
       microkit_board,
       "--config",
       microkit_config,
       "-o",
       final_image_dest,
       "-r",
       b.getInstallPath(.prefix, "./report.txt")
    });
    microkit_tool_cmd.step.dependOn(b.getInstallStep());
    // Add the "microkit" step, and make it the default step when we execute `zig build`
    const microkit_step = b.step("microkit", "Compile and build the final bootable image");
    microkit_step.dependOn(&microkit_tool_cmd.step);
    b.default_step = microkit_step;

    // This is setting up a `qemu` command for running the system using QEMU,
    // which we only want to do when we have a board that we can actually simulate.
    const loader_arg = b.fmt("loader,file={s},addr=0x70000000,cpu-num=0", .{ final_image_dest });
    if (std.mem.eql(u8, microkit_board, "qemu_arm_virt")) {
        const qemu_cmd = b.addSystemCommand(&[_][]const u8{
            "qemu-system-aarch64",
            "-machine", "virt,virtualization=on,highmem=off,secure=off",
            "-cpu", "cortex-a53",
            "-serial", "mon:stdio",
            "-device", loader_arg,
            "-m", "2G",
            "-nographic",
            "-global", "virtio-mmio.force-legacy=false",
            "-d", "guest_errors",
            "-drive", "file=mydisk,if=none,format=raw,id=hd",
            "-device", "virtio-blk-device,drive=hd"
        });
        qemu_cmd.step.dependOn(b.default_step);
        const simulate_step = b.step("qemu", "Simulate the image using QEMU");
        simulate_step.dependOn(&qemu_cmd.step);
    }
}
