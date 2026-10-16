#define _POSIX_C_SOURCE 200809L
#include "ninja_gen.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int aake_ninja_generate(const aake_options_t *opts, const aake_manifest_set_t *manifest, const char *out_ninja_file) {
    if (!opts || !out_ninja_file) return -1;
    (void)manifest;

    FILE *f = fopen(out_ninja_file, "w");
    if (!f) return -1;

    const char *repo_root = opts->repo_root ? opts->repo_root : ".";
    const char *out_dir = opts->out_dir ? opts->out_dir : "out";

    /* Find target sysroot */
    const char *home = getenv("HOME");
    char target_sysroot[1024] = "";
    char target_res_dir[1024] = "";
    if (home) {
        snprintf(target_sysroot, sizeof(target_sysroot),
                 "%s/.swiftpm/swift-sdks/swift-6.4.0-RELEASE_static-linux-0.1.0.artifactbundle/swift-6.4.0-RELEASE_static-linux-0.1.0/swift-linux-musl/musl-1.2.5.sdk/x86_64",
                 home);
        snprintf(target_res_dir, sizeof(target_res_dir), "%s/usr/lib/swift_static", target_sysroot);
    }

    /* Find mimalloc object */
    char mimalloc_obj[1024] = "";
    const char *env_mimalloc = getenv("MIMALLOC_OBJ");
    if (env_mimalloc && aake_file_exists(env_mimalloc)) {
        snprintf(mimalloc_obj, sizeof(mimalloc_obj), "%s", env_mimalloc);
    } else {
        snprintf(mimalloc_obj, sizeof(mimalloc_obj), "%s/components/boot/build/sysroot/lib/mimalloc.o", repo_root);
    }

    fprintf(f, "# Generated deterministically by ARK-OS Aake v%s\n", AAKE_VERSION);
    fprintf(f, "ninja_required_version = 1.8\n\n");

    /* Global Variables */
    fprintf(f, "repo_root = %s\n", repo_root);
    fprintf(f, "out_dir = %s\n", out_dir);
    fprintf(f, "target_sysroot = %s\n", target_sysroot);
    fprintf(f, "target_res_dir = %s\n", target_res_dir);
    fprintf(f, "mimalloc_obj = %s\n", mimalloc_obj);
    fprintf(f, "cflags = -O2 -Wall -Wextra -Werror -I $repo_root/components/runtime/arkrt/include\n");
    fprintf(f, "swiftflags = -O -static-stdlib -I $repo_root/components/runtime/arkrt/include\n\n");

    /* Rules */
    fprintf(f, "# Rules\n");
    fprintf(f, "rule cc_host\n");
    fprintf(f, "  command = gcc $cflags -c $in -o $out\n");
    fprintf(f, "  description = Compiling C (host) $out\n\n");

    fprintf(f, "rule starter_host\n");
    fprintf(f, "  command = gcc $cflags -static $in -o $out\n");
    fprintf(f, "  description = Compiling PID 1 starter (host) $out\n\n");

    fprintf(f, "rule swiftc_host\n");
    fprintf(f, "  command = swiftc $swiftflags $in -o $out\n");
    fprintf(f, "  description = Compiling Swift (host) $out\n\n");

    fprintf(f, "rule cc_target\n");
    fprintf(f, "  command = clang -target x86_64-swift-linux-musl --sysroot=$target_sysroot -O2 -Wall -Wextra -Werror -I $repo_root/components/runtime/arkrt/include -c $in -o $out\n");
    fprintf(f, "  description = Compiling C (target musl) $out\n\n");

    fprintf(f, "rule starter_target\n");
    fprintf(f, "  command = clang -target x86_64-swift-linux-musl --sysroot=$target_sysroot -static -O2 -Wall -Wextra -Werror -I $repo_root/components/runtime/arkrt/include $in -o $out\n");
    fprintf(f, "  description = Compiling PID 1 starter (target musl) $out\n\n");

    fprintf(f, "rule swiftc_target\n");
    fprintf(f, "  command = swiftc -target x86_64-swift-linux-musl -sdk $target_sysroot -resource-dir $target_res_dir -Xfrontend -use-static-resource-dir -static-stdlib -Xlinker -lswift_RegexParser -O -I $repo_root/components/runtime/arkrt/include $in $mimalloc_obj -o $out\n");
    fprintf(f, "  description = Compiling Swift (target musl) $out\n\n");

    fprintf(f, "rule boot_make\n");
    fprintf(f, "  command = make -C $repo_root/components/boot $boot_action\n");
    fprintf(f, "  description = Building Boot component $out\n\n");

    fprintf(f, "rule test_exec\n");
    fprintf(f, "  command = $in\n");
    fprintf(f, "  description = Running unit test $in\n\n");

    /* Build Statements */
    fprintf(f, "# Components: Runtime (arkrt)\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/signals.o: cc_host $repo_root/components/runtime/arkrt/signals.c | $repo_root/components/runtime/arkrt/include/signals.h\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/starter: starter_host $repo_root/components/runtime/arkrt/starter.c\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/arkrt-swift: swiftc_host $repo_root/components/runtime/arkrt/main.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.o | $repo_root/components/runtime/arkrt/include/module.modulemap\n\n");

    fprintf(f, "# Components: Services (arkdiag)\n");
    fprintf(f, "build $repo_root/components/services/arkdiag/arkdiag: swiftc_host $repo_root/components/services/arkdiag/main.swift $repo_root/components/runtime/arkrt/signals.o\n\n");

    fprintf(f, "# Components: Target static musl binaries\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/signals.target.o: cc_target $repo_root/components/runtime/arkrt/signals.c | $repo_root/components/runtime/arkrt/include/signals.h\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/starter.target: starter_target $repo_root/components/runtime/arkrt/starter.c\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/arkrt-swift-target: swiftc_target $repo_root/components/runtime/arkrt/main.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.target.o | $repo_root/components/runtime/arkrt/include/module.modulemap\n");
    fprintf(f, "build $repo_root/components/services/arkdiag/arkdiag.target: swiftc_target $repo_root/components/services/arkdiag/main.swift $repo_root/components/runtime/arkrt/signals.target.o\n\n");

    fprintf(f, "# Components: Bootloader & Images\n");
    fprintf(f, "build $repo_root/components/boot/ARKBOOT.EFI: boot_make\n");
    fprintf(f, "  boot_action = ARKBOOT.EFI\n");
    fprintf(f, "build $repo_root/components/boot/build/initramfs.cpio: boot_make | $repo_root/components/runtime/arkrt/starter $repo_root/components/services/arkdiag/arkdiag\n");
    fprintf(f, "  boot_action = initramfs\n");
    fprintf(f, "build $repo_root/components/boot/build/initramfs-target.cpio: boot_make | $repo_root/components/runtime/arkrt/starter.target $repo_root/components/runtime/arkrt/arkrt-swift-target $repo_root/components/services/arkdiag/arkdiag.target\n");
    fprintf(f, "  boot_action = initramfs-target MIMALLOC_OBJ=$mimalloc_obj TARGET_SYSROOT=$target_sysroot\n");
    fprintf(f, "build $repo_root/components/boot/build/esp: boot_make | $repo_root/components/boot/ARKBOOT.EFI $repo_root/components/boot/build/initramfs.cpio\n");
    fprintf(f, "  boot_action = esp\n");
    fprintf(f, "build $repo_root/components/boot/build/esp-target: boot_make | $repo_root/components/boot/ARKBOOT.EFI $repo_root/components/boot/build/initramfs-target.cpio\n");
    fprintf(f, "  boot_action = esp-target MIMALLOC_OBJ=$mimalloc_obj TARGET_SYSROOT=$target_sysroot\n\n");

    fprintf(f, "# Tests\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/test_supervisor: swiftc_host $repo_root/components/runtime/arkrt/test_supervisor.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.o\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/test_supervisor.target: swiftc_target $repo_root/components/runtime/arkrt/test_supervisor.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.target.o\n\n");

    /* Phonys / Target Aliases */
    fprintf(f, "# Target Aliases\n");
    fprintf(f, "build runtime: phony $repo_root/components/runtime/arkrt/starter $repo_root/components/runtime/arkrt/arkrt-swift\n");
    fprintf(f, "build runtime-target: phony $repo_root/components/runtime/arkrt/starter.target $repo_root/components/runtime/arkrt/arkrt-swift-target\n");
    fprintf(f, "build services: phony $repo_root/components/services/arkdiag/arkdiag\n");
    fprintf(f, "build services-target: phony $repo_root/components/services/arkdiag/arkdiag.target\n");
    fprintf(f, "build arkrt: phony runtime\n");
    fprintf(f, "build arkrt-target: phony runtime-target\n");
    fprintf(f, "build arkdiag: phony services\n");
    fprintf(f, "build arkdiag-target: phony services-target\n");
    fprintf(f, "build boot: phony $repo_root/components/boot/ARKBOOT.EFI\n");
    fprintf(f, "build initramfs: phony $repo_root/components/boot/build/initramfs.cpio\n");
    fprintf(f, "build initramfs-target: phony $repo_root/components/boot/build/initramfs-target.cpio\n");
    fprintf(f, "build esp: phony $repo_root/components/boot/build/esp\n");
    fprintf(f, "build esp-target: phony $repo_root/components/boot/build/esp-target\n");
    fprintf(f, "build test-supervisor: phony $repo_root/components/runtime/arkrt/test_supervisor\n");
    fprintf(f, "build test-supervisor-target: phony $repo_root/components/runtime/arkrt/test_supervisor.target\n");
    fprintf(f, "build all: phony runtime services boot initramfs esp\n");
    fprintf(f, "build all-target: phony runtime-target services-target boot initramfs-target esp-target\n");
    fprintf(f, "default all\n");

    fclose(f);
    return 0;
}
