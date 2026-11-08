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
    char target_sysroot[1024] = "";
    char target_res_dir[1024] = "";

    const char *env_sysroot = getenv("TARGET_SYSROOT");
    if (env_sysroot && aake_dir_exists(env_sysroot)) {
        snprintf(target_sysroot, sizeof(target_sysroot), "%s", env_sysroot);
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(target_sysroot, sizeof(target_sysroot),
                     "%s/.swiftpm/swift-sdks/swift-6.4.0-RELEASE_static-linux-0.1.0.artifactbundle/swift-6.4.0-RELEASE_static-linux-0.1.0/swift-linux-musl/musl-1.2.5.sdk/x86_64",
                     home);
        }
    }
    snprintf(target_res_dir, sizeof(target_res_dir), "%s/usr/lib/swift_static", target_sysroot);

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
    fprintf(f, "swiftflags = -O -static-stdlib -I $repo_root/components/runtime/arkrt/include\n");
    fprintf(f, "sash_swiftflags = -O -static-stdlib\n\n");

    /* Rules */
    fprintf(f, "# Rules: Host Compilation\n");
    fprintf(f, "rule cc_host\n");
    fprintf(f, "  command = gcc $cflags -c $in -o $out\n");
    fprintf(f, "  description = Compiling C (host) $out\n\n");

    fprintf(f, "rule starter_host\n");
    fprintf(f, "  command = gcc $cflags -static $in -o $out\n");
    fprintf(f, "  description = Compiling PID 1 starter (host) $out\n\n");

    fprintf(f, "rule swiftc_host\n");
    fprintf(f, "  command = swiftc $swiftflags $in -o $out\n");
    fprintf(f, "  description = Compiling Swift (host) $out\n\n");

    fprintf(f, "rule swiftc_sash_host\n");
    fprintf(f, "  command = swiftc $sash_swiftflags $in -o $out\n");
    fprintf(f, "  description = Compiling Shell sash (host) $out\n\n");

    fprintf(f, "# Rules: Target Static Musl Compilation\n");
    fprintf(f, "rule cc_target\n");
    fprintf(f, "  command = clang -target x86_64-swift-linux-musl --sysroot=$target_sysroot -O2 -Wall -Wextra -Werror -I $repo_root/components/runtime/arkrt/include -c $in -o $out\n");
    fprintf(f, "  description = Compiling C (target musl) $out\n\n");

    fprintf(f, "rule starter_target\n");
    fprintf(f, "  command = clang -target x86_64-swift-linux-musl --sysroot=$target_sysroot -static -O2 -Wall -Wextra -Werror -I $repo_root/components/runtime/arkrt/include $in -o $out\n");
    fprintf(f, "  description = Compiling PID 1 starter (target musl) $out\n\n");

    fprintf(f, "rule swiftc_target\n");
    fprintf(f, "  command = swiftc -target x86_64-swift-linux-musl -sdk $target_sysroot -resource-dir $target_res_dir -Xfrontend -use-static-resource-dir -static-stdlib -Xlinker -lswift_RegexParser -O -I $repo_root/components/runtime/arkrt/include $in $mimalloc_obj -o $out\n");
    fprintf(f, "  description = Compiling Swift (target musl) $out\n\n");

    fprintf(f, "rule swiftc_sash_target\n");
    fprintf(f, "  command = swiftc -target x86_64-swift-linux-musl -sdk $target_sysroot -resource-dir $target_res_dir -Xfrontend -use-static-resource-dir -static-stdlib -Xlinker -lswift_RegexParser -O $in $mimalloc_obj -o $out\n");
    fprintf(f, "  description = Compiling Shell sash (target musl) $out\n\n");

    fprintf(f, "# Rules: Bootloader, Initramfs & Packaging\n");
    fprintf(f, "rule boot_make\n");
    fprintf(f, "  command = make -C $repo_root/components/boot $boot_action\n");
    fprintf(f, "  description = Building Bootloader $out\n\n");

    fprintf(f, "rule assemble_initramfs_host\n");
    fprintf(f, "  command = rm -rf $out_dir/initramfs_staging && mkdir -p $out_dir/initramfs_staging/dev $out_dir/initramfs_staging/proc $out_dir/initramfs_staging/sys $out_dir/initramfs_staging/run $out_dir/initramfs_staging/sbin $out_dir/initramfs_staging/bin $out_dir/initramfs_staging/lib64 $out_dir/initramfs_staging/usr/lib $out_dir/initramfs_staging/etc/ark/services.d && ln -sf usr/lib $out_dir/initramfs_staging/lib && cp $repo_root/components/runtime/arkrt/starter $out_dir/initramfs_staging/sbin/arkrt && chmod 755 $out_dir/initramfs_staging/sbin/arkrt && ln -sf sbin/arkrt $out_dir/initramfs_staging/init && cp $repo_root/components/runtime/arkrt/arkrt-swift $out_dir/initramfs_staging/sbin/arkrt-swift && chmod 755 $out_dir/initramfs_staging/sbin/arkrt-swift && cp $repo_root/components/services/arkdiag/arkdiag $out_dir/initramfs_staging/sbin/arkdiag && chmod 755 $out_dir/initramfs_staging/sbin/arkdiag && cp $repo_root/components/services/arkdiag/arkdiag.serve $out_dir/initramfs_staging/etc/ark/services.d/arkdiag.serve && chmod 644 $out_dir/initramfs_staging/etc/ark/services.d/arkdiag.serve && (if [ -f $repo_root/components/shell/sash/sash ]; then cp $repo_root/components/shell/sash/sash $out_dir/initramfs_staging/bin/sash && chmod 755 $out_dir/initramfs_staging/bin/sash && ln -sf sash $out_dir/initramfs_staging/bin/sh; fi) && (if [ -f $repo_root/components/shell/sash/sash.serve ]; then cp $repo_root/components/shell/sash/sash.serve $out_dir/initramfs_staging/etc/ark/services.d/sash.serve && chmod 644 $out_dir/initramfs_staging/etc/ark/services.d/sash.serve; fi) && cp /lib64/ld-linux-x86-64.so.2 $out_dir/initramfs_staging/lib64/ && cp /usr/lib/libc.so.6 /usr/lib/libm.so.6 /usr/lib/libgcc_s.so.1 $out_dir/initramfs_staging/usr/lib/ && cp -d /usr/lib/libstdc++.so.6* $out_dir/initramfs_staging/usr/lib/ && (cd $out_dir/initramfs_staging && find . | cpio -H newc -o > ../initramfs.cpio) && mkdir -p $repo_root/components/boot/build && cp $out $repo_root/components/boot/build/initramfs.cpio\n");
    fprintf(f, "  description = Assembling host initramfs archive $out\n\n");

    fprintf(f, "rule assemble_initramfs_target\n");
    fprintf(f, "  command = rm -rf $out_dir/initramfs_target_staging && mkdir -p $out_dir/initramfs_target_staging/dev $out_dir/initramfs_target_staging/proc $out_dir/initramfs_target_staging/sys $out_dir/initramfs_target_staging/run $out_dir/initramfs_target_staging/sbin $out_dir/initramfs_target_staging/bin $out_dir/initramfs_target_staging/etc/ark/services.d && cp $repo_root/components/runtime/arkrt/starter.target $out_dir/initramfs_target_staging/sbin/arkrt && chmod 755 $out_dir/initramfs_target_staging/sbin/arkrt && ln -sf sbin/arkrt $out_dir/initramfs_target_staging/init && cp $repo_root/components/runtime/arkrt/arkrt-swift-target $out_dir/initramfs_target_staging/sbin/arkrt-swift && chmod 755 $out_dir/initramfs_target_staging/sbin/arkrt-swift && cp $repo_root/components/services/arkdiag/arkdiag.target $out_dir/initramfs_target_staging/sbin/arkdiag && chmod 755 $out_dir/initramfs_target_staging/sbin/arkdiag && cp $repo_root/components/services/arkdiag/arkdiag.serve $out_dir/initramfs_target_staging/etc/ark/services.d/arkdiag.serve && chmod 644 $out_dir/initramfs_target_staging/etc/ark/services.d/arkdiag.serve && (if [ -f $repo_root/components/shell/sash/sash.target ]; then cp $repo_root/components/shell/sash/sash.target $out_dir/initramfs_target_staging/bin/sash && chmod 755 $out_dir/initramfs_target_staging/bin/sash && ln -sf sash $out_dir/initramfs_target_staging/bin/sh; fi) && (if [ -f $repo_root/components/shell/sash/sash.serve ]; then cp $repo_root/components/shell/sash/sash.serve $out_dir/initramfs_target_staging/etc/ark/services.d/sash.serve && chmod 644 $out_dir/initramfs_target_staging/etc/ark/services.d/sash.serve; fi) && (cd $out_dir/initramfs_target_staging && find . | cpio -H newc -o > ../initramfs-target.cpio) && mkdir -p $repo_root/components/boot/build && cp $out $repo_root/components/boot/build/initramfs-target.cpio\n");
    fprintf(f, "  description = Assembling static musl initramfs archive $out\n\n");

    fprintf(f, "rule assemble_esp\n");
    fprintf(f, "  command = mkdir -p $out_dir/esp/EFI/BOOT && cp $repo_root/components/boot/ARKBOOT.EFI $out_dir/esp/EFI/BOOT/BOOTX64.EFI && cp $repo_root/components/boot/build/kernel/boot/bzImage $out_dir/esp/EFI/BOOT/bzImage && cp $repo_root/components/boot/build/kernel/boot/bzImage $out_dir/esp/bzImage && cp $in_initramfs $out_dir/esp/EFI/BOOT/initramfs.cpio && cp $in_initramfs $out_dir/esp/initramfs.cpio && mkdir -p $repo_root/components/boot/build/esp/EFI/BOOT && cp -r $out_dir/esp/* $repo_root/components/boot/build/esp/ && touch $out\n");
    fprintf(f, "  description = Assembling EFI System Partition (ESP) $out\n\n");

    /* Build Statements: Host Components */
    fprintf(f, "# Host Components: Runtime (arkrt)\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/signals.o: cc_host $repo_root/components/runtime/arkrt/signals.c | $repo_root/components/runtime/arkrt/include/signals.h\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/starter: starter_host $repo_root/components/runtime/arkrt/starter.c\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/arkrt-swift: swiftc_host $repo_root/components/runtime/arkrt/main.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.o | $repo_root/components/runtime/arkrt/include/module.modulemap\n\n");

    fprintf(f, "# Host Components: Services (arkdiag)\n");
    fprintf(f, "build $repo_root/components/services/arkdiag/arkdiag: swiftc_host $repo_root/components/services/arkdiag/main.swift $repo_root/components/runtime/arkrt/signals.o\n\n");

    fprintf(f, "# Host Components: Shell (sash)\n");
    fprintf(f, "build $repo_root/components/shell/sash/sash: swiftc_sash_host $repo_root/components/shell/sash/Sources/sash/sash.swift $repo_root/components/shell/sash/Sources/sash/CommandDispatcher.swift $repo_root/components/shell/sash/Sources/sash/Builtins.swift $repo_root/components/shell/sash/Sources/sash/JobControl.swift\n\n");

    /* Build Statements: Target Static Musl Components */
    fprintf(f, "# Target Static Musl Components\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/signals.target.o: cc_target $repo_root/components/runtime/arkrt/signals.c | $repo_root/components/runtime/arkrt/include/signals.h\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/starter.target: starter_target $repo_root/components/runtime/arkrt/starter.c\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/arkrt-swift-target: swiftc_target $repo_root/components/runtime/arkrt/main.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.target.o | $repo_root/components/runtime/arkrt/include/module.modulemap\n");
    fprintf(f, "build $repo_root/components/services/arkdiag/arkdiag.target: swiftc_target $repo_root/components/services/arkdiag/main.swift $repo_root/components/runtime/arkrt/signals.target.o\n");
    fprintf(f, "build $repo_root/components/shell/sash/sash.target: swiftc_sash_target $repo_root/components/shell/sash/Sources/sash/sash.swift $repo_root/components/shell/sash/Sources/sash/CommandDispatcher.swift $repo_root/components/shell/sash/Sources/sash/Builtins.swift $repo_root/components/shell/sash/Sources/sash/JobControl.swift\n\n");

    /* Build Statements: Bootloader & Initramfs Archives */
    fprintf(f, "# Bootloader & Packaging\n");
    fprintf(f, "build $repo_root/components/boot/ARKBOOT.EFI: boot_make\n");
    fprintf(f, "  boot_action = ARKBOOT.EFI\n\n");

    fprintf(f, "build $out_dir/initramfs.cpio: assemble_initramfs_host | $repo_root/components/runtime/arkrt/starter $repo_root/components/runtime/arkrt/arkrt-swift $repo_root/components/services/arkdiag/arkdiag $repo_root/components/services/arkdiag/arkdiag.serve $repo_root/components/shell/sash/sash $repo_root/components/shell/sash/sash.serve\n\n");

    fprintf(f, "build $out_dir/initramfs-target.cpio: assemble_initramfs_target | $repo_root/components/runtime/arkrt/starter.target $repo_root/components/runtime/arkrt/arkrt-swift-target $repo_root/components/services/arkdiag/arkdiag.target $repo_root/components/services/arkdiag/arkdiag.serve $repo_root/components/shell/sash/sash.target $repo_root/components/shell/sash/sash.serve\n\n");

    fprintf(f, "build $out_dir/esp.stamp: assemble_esp | $repo_root/components/boot/ARKBOOT.EFI $out_dir/initramfs.cpio\n");
    fprintf(f, "  in_initramfs = $out_dir/initramfs.cpio\n\n");

    fprintf(f, "build $out_dir/esp-target.stamp: assemble_esp | $repo_root/components/boot/ARKBOOT.EFI $out_dir/initramfs-target.cpio\n");
    fprintf(f, "  in_initramfs = $out_dir/initramfs-target.cpio\n\n");

    /* Tests */
    fprintf(f, "# Tests\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/test_supervisor: swiftc_host $repo_root/components/runtime/arkrt/test_supervisor.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.o\n");
    fprintf(f, "build $repo_root/components/runtime/arkrt/test_supervisor.target: swiftc_target $repo_root/components/runtime/arkrt/test_supervisor.swift $repo_root/components/runtime/arkrt/Service.swift $repo_root/components/runtime/arkrt/Supervisor.swift $repo_root/components/runtime/arkrt/signals.target.o\n\n");

    /* Phonys / Target Aliases */
    fprintf(f, "# Target Aliases\n");
    fprintf(f, "build runtime: phony $repo_root/components/runtime/arkrt/starter $repo_root/components/runtime/arkrt/arkrt-swift\n");
    fprintf(f, "build runtime-target: phony $repo_root/components/runtime/arkrt/starter.target $repo_root/components/runtime/arkrt/arkrt-swift-target\n");
    fprintf(f, "build services: phony $repo_root/components/services/arkdiag/arkdiag\n");
    fprintf(f, "build services-target: phony $repo_root/components/services/arkdiag/arkdiag.target\n");
    fprintf(f, "build shell: phony $repo_root/components/shell/sash/sash\n");
    fprintf(f, "build shell-target: phony $repo_root/components/shell/sash/sash.target\n");
    fprintf(f, "build sash: phony shell\n");
    fprintf(f, "build sash-target: phony shell-target\n");
    fprintf(f, "build arkrt: phony runtime\n");
    fprintf(f, "build arkrt-target: phony runtime-target\n");
    fprintf(f, "build arkdiag: phony services\n");
    fprintf(f, "build arkdiag-target: phony services-target\n");
    fprintf(f, "build target: phony runtime-target services-target shell-target\n");
    fprintf(f, "build boot: phony $repo_root/components/boot/ARKBOOT.EFI\n");
    fprintf(f, "build initramfs: phony $out_dir/initramfs.cpio\n");
    fprintf(f, "build initramfs-target: phony $out_dir/initramfs-target.cpio\n");
    fprintf(f, "build esp: phony $out_dir/esp.stamp\n");
    fprintf(f, "build esp-target: phony $out_dir/esp-target.stamp\n");
    fprintf(f, "build test-supervisor: phony $repo_root/components/runtime/arkrt/test_supervisor\n");
    fprintf(f, "build test-supervisor-target: phony $repo_root/components/runtime/arkrt/test_supervisor.target\n");
    fprintf(f, "build all: phony runtime services shell boot initramfs esp\n");
    fprintf(f, "build all-target: phony runtime-target services-target shell-target boot initramfs-target esp-target\n");
    fprintf(f, "default all\n");

    fclose(f);
    return 0;
}
