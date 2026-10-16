#define _POSIX_C_SOURCE 200809L
#include "aake.h"
#include "ui.h"
#include "utils.h"
#include "manifest.h"
#include "ninja_gen.h"
#include "ninja_runner.h"
#include "test_runner.h"
#include "doctor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int aake_cmd_version(const aake_options_t *opts) {
    (void)opts;
    printf("ARK-OS Aake v%s (Backed by %s)\n", AAKE_VERSION, AAKE_DEFAULT_BACKEND);
    printf("Copyright (c) 2026 ARK-OS Project Contributors.\n");
    return 0;
}

int aake_cmd_help(const aake_options_t *opts) {
    (void)opts;
    printf("Usage: aake <command> [options] [target]\n\n");
    printf("Aake is the official build orchestrator for ARK-OS,\n");
    printf("backed by Ninja for fast, reliable parallel and incremental builds.\n\n");
    printf("Commands:\n");
    printf("  help                  Show this help overview\n");
    printf("  version               Display Aake version and build backend\n");
    printf("  doctor                Inspect environment, toolchain, and dependencies\n");
    printf("  build [target]        Orchestrate and compile targets via Ninja\n");
    printf("  test [target]         Execute host unit tests or target verification\n");
    printf("  test --uefi           Launch QEMU with UEFI firmware and ESP image\n");
    printf("  test --bios           Inspect legacy BIOS boot support\n");
    printf("  clean [target]        Clean generated artifacts and build outputs\n\n");
    printf("Targets:\n");
    printf("  all                   Build runtime, services, bootloader, and assemble ESP (default)\n");
    printf("  all-target            Build static musl runtime, services, and static initramfs ESP\n");
    printf("  runtime / arkrt       Build PID 1 starter and Swift main runtime\n");
    printf("  runtime-target        Build static musl PID 1 starter and Swift runtime\n");
    printf("  services / arkdiag    Build system diagnostics daemon\n");
    printf("  services-target       Build static musl diagnostics daemon\n");
    printf("  boot                  Build UEFI bootloader (ARKBOOT.EFI)\n");
    printf("  initramfs             Assemble initramfs with host glibc libraries\n");
    printf("  initramfs-target      Assemble static musl initramfs with mimalloc\n");
    printf("  esp                   Assemble ESP partition with host initramfs\n");
    printf("  esp-target            Assemble ESP partition with static musl initramfs\n\n");
    printf("Options:\n");
    printf("  -v, --verbose         Print raw commands and detailed compiler output\n");
    printf("  --debug               Select debug configuration (default: release)\n");
    printf("  --release             Select release configuration\n");
    printf("  --out-dir <path>      Specify build output directory (default: out)\n\n");
    printf("Examples:\n");
    printf("  aake doctor\n");
    printf("  aake build\n");
    printf("  aake build runtime-target\n");
    printf("  aake test\n");
    printf("  aake test --uefi\n");
    printf("  aake clean\n");
    return 0;
}

int aake_cmd_doctor(const aake_options_t *opts) {
    return aake_doctor_check_all(opts);
}

int aake_cmd_build(const aake_options_t *opts) {
    /* 1. Load manifests */
    aake_manifest_set_t manifest;
    if (aake_manifest_load(opts->repo_root, &manifest) != 0) {
        aake_ui_error("Failed to load manifests from %s/manifests", opts->repo_root);
        return 1;
    }

    /* 2. Ensure out directory exists */
    if (aake_mkdir_p(opts->out_dir) != 0) {
        aake_ui_error("Failed to create output directory: %s", opts->out_dir);
        return 1;
    }

    /* 3. Generate Ninja build file */
    char *ninja_file = aake_path_join(opts->out_dir, "build.ninja");
    if (aake_ninja_generate(opts, &manifest, ninja_file) != 0) {
        aake_ui_error("Failed to generate Ninja build file: %s", ninja_file);
        free(ninja_file);
        return 1;
    }

    /* 4. Execute Ninja with Soong-inspired UI */
    double elapsed = 0.0;
    int ret = aake_ninja_run(opts, ninja_file, opts->target, &elapsed);
    free(ninja_file);

    return ret;
}

int aake_cmd_test(const aake_options_t *opts) {
    return aake_run_tests(opts);
}

int aake_cmd_clean(const aake_options_t *opts) {
    printf("========================================\n");
    printf("ARK-OS Aake Clean\n");
    printf("========================================\n");

    const char *tgt = opts->target;
    if (!tgt || strcmp(tgt, "all") == 0 || *tgt == '\0') {
        char *ninja_file = aake_path_join(opts->out_dir, "build.ninja");
        if (aake_file_exists(ninja_file)) {
            char *ninja_bin = aake_which("ninja");
            if (ninja_bin) {
                const char *const args[] = { ninja_bin, "-f", ninja_file, "-t", "clean", NULL };
                aake_run_cmd(args, false, NULL);
                free(ninja_bin);
            }
        }
        free(ninja_file);

        /* Clean out/ directory safely */
        aake_ui_info("Removing output directory %s...", opts->out_dir);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", opts->out_dir);
        system(cmd);

        /* Clean component build artifacts */
        snprintf(cmd, sizeof(cmd), "make -C %s/components/runtime/arkrt clean >/dev/null 2>&1", opts->repo_root);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "make -C %s/components/services/arkdiag clean >/dev/null 2>&1", opts->repo_root);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "make -C %s/components/boot clean >/dev/null 2>&1", opts->repo_root);
        system(cmd);

        aake_ui_info("Clean completed.");
    } else {
        aake_ui_info("Cleaning target: %s", tgt);
        char *ninja_file = aake_path_join(opts->out_dir, "build.ninja");
        if (aake_file_exists(ninja_file)) {
            char *ninja_bin = aake_which("ninja");
            if (ninja_bin) {
                const char *const args[] = { ninja_bin, "-f", ninja_file, "-t", "clean", tgt, NULL };
                aake_run_cmd(args, false, NULL);
                free(ninja_bin);
            }
        }
        free(ninja_file);
        aake_ui_info("Target clean completed.");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    aake_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.cmd = CMD_BUILD;
    opts.config = "release";
    opts.boot_mode = BOOT_NONE;
    opts.argc = argc;
    opts.argv = argv;

    /* Detect repository root */
    opts.repo_root = aake_find_repo_root();
    if (!opts.repo_root) {
        opts.repo_root = aake_strdup(".");
    }
    opts.out_dir = aake_path_join(opts.repo_root, "out");

    /* Parse arguments */
    int arg_i = 1;
    if (arg_i < argc) {
        if (strcmp(argv[arg_i], "help") == 0 || strcmp(argv[arg_i], "-h") == 0 || strcmp(argv[arg_i], "--help") == 0) {
            opts.cmd = CMD_HELP;
            arg_i++;
        } else if (strcmp(argv[arg_i], "version") == 0 || strcmp(argv[arg_i], "--version") == 0) {
            opts.cmd = CMD_VERSION;
            arg_i++;
        } else if (strcmp(argv[arg_i], "doctor") == 0) {
            opts.cmd = CMD_DOCTOR;
            arg_i++;
        } else if (strcmp(argv[arg_i], "build") == 0) {
            opts.cmd = CMD_BUILD;
            arg_i++;
        } else if (strcmp(argv[arg_i], "test") == 0) {
            opts.cmd = CMD_TEST;
            arg_i++;
        } else if (strcmp(argv[arg_i], "clean") == 0) {
            opts.cmd = CMD_CLEAN;
            arg_i++;
        }
    }

    for (; arg_i < argc; arg_i++) {
        const char *arg = argv[arg_i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            opts.verbose = true;
        } else if (strcmp(arg, "--debug") == 0) {
            opts.config = "debug";
        } else if (strcmp(arg, "--release") == 0) {
            opts.config = "release";
        } else if (strcmp(arg, "--uefi") == 0) {
            opts.boot_mode = BOOT_UEFI;
        } else if (strcmp(arg, "--bios") == 0) {
            opts.boot_mode = BOOT_BIOS;
        } else if (strcmp(arg, "--out-dir") == 0 && arg_i + 1 < argc) {
            free(opts.out_dir);
            opts.out_dir = aake_strdup(argv[++arg_i]);
        } else if (arg[0] != '-') {
            if (!opts.target) {
                opts.target = aake_strdup(arg);
            }
        }
    }

    int result = 0;
    switch (opts.cmd) {
        case CMD_HELP:
            result = aake_cmd_help(&opts);
            break;
        case CMD_VERSION:
            result = aake_cmd_version(&opts);
            break;
        case CMD_DOCTOR:
            result = aake_cmd_doctor(&opts);
            break;
        case CMD_BUILD:
            result = aake_cmd_build(&opts);
            break;
        case CMD_TEST:
            result = aake_cmd_test(&opts);
            break;
        case CMD_CLEAN:
            result = aake_cmd_clean(&opts);
            break;
        default:
            result = aake_cmd_help(&opts);
            break;
    }

    free(opts.repo_root);
    free(opts.out_dir);
    free(opts.target);

    return result;
}
