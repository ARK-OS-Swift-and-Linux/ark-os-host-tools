#define _POSIX_C_SOURCE 200809L
#include "test_runner.h"
#include "ninja_runner.h"
#include "ninja_gen.h"
#include "manifest.h"
#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void ensure_ninja_built(const aake_options_t *opts, const char *target) {
    char *ninja_file = aake_path_join(opts->out_dir, "build.ninja");
    if (!aake_file_exists(ninja_file)) {
        aake_manifest_set_t manifests;
        if (aake_manifest_load(opts->repo_root, &manifests) == 0) {
            aake_mkdir_p(opts->out_dir);
            aake_ninja_generate(opts, &manifests, ninja_file);
        }
    }
    double el = 0.0;
    aake_ninja_run(opts, ninja_file, target, &el);
    free(ninja_file);
}

int aake_run_tests(const aake_options_t *opts) {
    if (!opts) return -1;

    /* Handle explicit boot-mode testing */
    if (opts->boot_mode == BOOT_BIOS) {
        printf("======================================== ARK-OS Aake Test\n");
        printf("Test Mode: Legacy BIOS\n");
        printf("========================================\n\n");
        aake_ui_warn("Legacy BIOS bootloader is not currently implemented in ARK-OS.");
        aake_ui_info("ARK-OS currently implements UEFI booting via ARKBOOT.EFI (use `aake test --uefi`).");
        aake_ui_info("Remaining work: An MBR/multiboot stage 1 bootloader and 16-bit real-mode entry are required for BIOS support.");
        printf("\n========================================\n");
        printf("TEST RESULT: UNSUPPORTED (0 run, 1 skipped)\n");
        printf("========================================\n");
        return 1;
    }

    if (opts->boot_mode == BOOT_UEFI) {
        printf("======================================== ARK-OS Aake Test\n");
        printf("Test Mode: UEFI Virtual Machine (QEMU + OVMF)\n");
        printf("========================================\n\n");

        char *qemu = aake_which("qemu-system-x86_64");
        if (!qemu) {
            aake_ui_error("qemu-system-x86_64 not found on PATH. Install QEMU for boot-testing.");
            return 1;
        }
        free(qemu);

        const char *ovmf = NULL;
        if (aake_file_exists("/usr/share/edk2/x64/OVMF.4m.fd")) {
            ovmf = "/usr/share/edk2/x64/OVMF.4m.fd";
        } else if (aake_file_exists("/usr/share/OVMF/OVMF_CODE.fd")) {
            ovmf = "/usr/share/OVMF/OVMF_CODE.fd";
        }

        if (!ovmf) {
            aake_ui_error("UEFI OVMF firmware not found in standard paths (/usr/share/edk2/x64/OVMF.4m.fd).");
            return 1;
        }

        char *esp_dir = aake_path_join(opts->repo_root, "components/boot/build/esp");
        char *bootx64 = aake_path_join(esp_dir, "EFI/BOOT/BOOTX64.EFI");
        char *initramfs = aake_path_join(esp_dir, "EFI/BOOT/initramfs.cpio");
        char *bzImage = aake_path_join(esp_dir, "EFI/BOOT/bzImage");

        if (!aake_file_exists(bootx64) || !aake_file_exists(initramfs) || !aake_file_exists(bzImage)) {
            aake_ui_error("ESP boot image incomplete in %s.", esp_dir);
            aake_ui_info("Missing required boot files. Run `aake build esp` or `aake build esp-target` first.");
            free(esp_dir);
            free(bootx64);
            free(initramfs);
            free(bzImage);
            return 1;
        }

        char drive_arg[1024];
        snprintf(drive_arg, sizeof(drive_arg), "file=fat:rw:%s,format=raw", esp_dir);

        const char *const qemu_argv[] = {
            "qemu-system-x86_64",
            "-bios", ovmf,
            "-drive", drive_arg,
            "-m", "1024M",
            "-net", "none",
            "-serial", "stdio",
            "-vga", "std",
            NULL
        };

        aake_ui_info("Launching QEMU with UEFI OVMF firmware...");
        aake_ui_info("Firmware: %s", ovmf);
        aake_ui_info("ESP:      %s", esp_dir);

        int res = aake_run_cmd(qemu_argv, false, NULL);

        free(esp_dir);
        free(bootx64);
        free(initramfs);
        free(bzImage);
        return res;
    }

    /* Host unit tests */
    printf("======================================== ARK-OS Aake Test\n");
    printf("Test Target: %s\n", (opts->target && *opts->target) ? opts->target : "runtime (test_supervisor)");
    printf("Backend:     Host Process Execution\n");
    printf("========================================\n\n");

    bool test_target_musl = (opts->target && (strcmp(opts->target, "target") == 0 ||
                                              strcmp(opts->target, "runtime-target") == 0 ||
                                              strcmp(opts->target, "arkrt-target") == 0));

    char *test_bin = NULL;
    if (test_target_musl) {
        test_bin = aake_path_join(opts->repo_root, "components/runtime/arkrt/test_supervisor.target");
        if (!aake_file_exists(test_bin)) {
            aake_ui_info("Building test_supervisor.target via Ninja...");
            ensure_ninja_built(opts, "test-supervisor-target");
        }
    } else {
        test_bin = aake_path_join(opts->repo_root, "components/runtime/arkrt/test_supervisor");
        if (!aake_file_exists(test_bin)) {
            aake_ui_info("Building test_supervisor via Ninja...");
            ensure_ninja_built(opts, "test-supervisor");
        }
    }

    if (!aake_file_exists(test_bin)) {
        aake_ui_error("Test executable not found: %s", test_bin);
        free(test_bin);
        return 1;
    }

    aake_ui_info("Executing test harness: %s", test_bin);
    double t_start = aake_get_time_seconds();

    const char *const argv[] = { test_bin, NULL };
    int ret = aake_run_cmd(argv, false, NULL);

    double t_end = aake_get_time_seconds();
    free(test_bin);

    printf("\n========================================\n");
    if (ret == 0) {
        printf("TEST SUITE SUCCESSFUL\n");
        printf("All service supervisor test cases passed.\n");
    } else {
        printf("TEST SUITE FAILED (exit code %d)\n", ret);
    }
    printf("Elapsed: %.2fs\n", t_end - t_start);
    printf("========================================\n");

    return ret;
}
