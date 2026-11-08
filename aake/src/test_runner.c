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
        } else if (aake_file_exists("/usr/share/OVMF/x64/OVMF.4m.fd")) {
            ovmf = "/usr/share/OVMF/x64/OVMF.4m.fd";
        } else if (aake_file_exists("/usr/share/OVMF/OVMF_CODE.fd")) {
            ovmf = "/usr/share/OVMF/OVMF_CODE.fd";
        }

        if (!ovmf) {
            aake_ui_error("UEFI OVMF firmware not found in standard paths (/usr/share/edk2/x64/OVMF.4m.fd).");
            return 1;
        }

        aake_mkdir_p(opts->out_dir);
        char *serial_log = aake_path_join(opts->out_dir, "serial.log");

        /* Check for assembled ESP partition */
        char *out_esp = aake_path_join(opts->out_dir, "esp");
        char *out_bootx64 = aake_path_join(out_esp, "EFI/BOOT/BOOTX64.EFI");
        char *boot_esp = aake_path_join(opts->repo_root, "components/boot/build/esp");
        char *boot_bootx64 = aake_path_join(boot_esp, "EFI/BOOT/BOOTX64.EFI");

        char *esp_dir = NULL;
        if (aake_file_exists(out_bootx64)) {
            esp_dir = aake_strdup(out_esp);
        } else if (aake_file_exists(boot_bootx64)) {
            esp_dir = aake_strdup(boot_esp);
        } else {
            aake_ui_info("ESP boot image not found. Assembling via Ninja...");
            ensure_ninja_built(opts, "esp-target");
            if (aake_file_exists(out_bootx64)) {
                esp_dir = aake_strdup(out_esp);
            } else if (aake_file_exists(boot_bootx64)) {
                esp_dir = aake_strdup(boot_esp);
            }
        }
        free(out_esp);
        free(out_bootx64);
        free(boot_esp);
        free(boot_bootx64);

        if (!esp_dir) {
            aake_ui_error("ESP boot partition not found. Run `aake build esp` or `aake build esp-target` first.");
            free(serial_log);
            return 1;
        }

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
            free(serial_log);
            return 1;
        }

        char drive_arg[1024];
        snprintf(drive_arg, sizeof(drive_arg), "file=fat:rw:%s,format=raw", esp_dir);

        char chardev_arg[1024];
        snprintf(chardev_arg, sizeof(chardev_arg), "stdio,id=char0,logfile=%s,signal=off", serial_log);

        const char *qemu_argv[32];
        int q_i = 0;
        qemu_argv[q_i++] = "qemu-system-x86_64";
        qemu_argv[q_i++] = "-bios";
        qemu_argv[q_i++] = ovmf;
        qemu_argv[q_i++] = "-drive";
        qemu_argv[q_i++] = drive_arg;
        qemu_argv[q_i++] = "-m";
        qemu_argv[q_i++] = "2048M";
        qemu_argv[q_i++] = "-net";
        qemu_argv[q_i++] = "none";
        qemu_argv[q_i++] = "-chardev";
        qemu_argv[q_i++] = chardev_arg;
        qemu_argv[q_i++] = "-serial";
        qemu_argv[q_i++] = "chardev:char0";
        qemu_argv[q_i++] = "-vga";
        qemu_argv[q_i++] = "std";
        if (opts->headless || getenv("AAKE_HEADLESS") || getenv("HEADLESS") || !getenv("DISPLAY")) {
            qemu_argv[q_i++] = "-display";
            qemu_argv[q_i++] = "none";
        }
        qemu_argv[q_i++] = NULL;

        aake_ui_info("Launching QEMU with UEFI OVMF firmware...");
        aake_ui_info("Firmware: %s", ovmf);
        aake_ui_info("ESP:      %s", esp_dir);
        aake_ui_info("Serial:   %s", serial_log);

        /* Ensure temporary backing files for virtual FAT drive go to /tmp */
        if (!getenv("TMPDIR")) {
            setenv("TMPDIR", "/tmp", 1);
        }

        /* Prevent GTK/GIO from blocking on GVFS D-Bus timeouts */
        if (!getenv("GIO_USE_VFS")) {
            setenv("GIO_USE_VFS", "local", 1);
        }
        if (!getenv("GVFS_DISABLE_FUSE")) {
            setenv("GVFS_DISABLE_FUSE", "1", 1);
        }

        int res = aake_run_cmd(qemu_argv, false, NULL);

        aake_ui_info("QEMU session terminated. Serial output captured in: %s", serial_log);

        free(esp_dir);
        free(bootx64);
        free(initramfs);
        free(bzImage);
        free(serial_log);
        return res;
    }

    /* Host unit tests */
    printf("======================================== ARK-OS Aake Test\n");
    printf("Test Target: %s\n", (opts->target && *opts->target) ? opts->target : "runtime (test_supervisor)");
    printf("Backend:     Host Process Execution\n");
    printf("========================================\n\n");

    if (opts->target && (strcmp(opts->target, "shell") == 0 || strcmp(opts->target, "sash") == 0)) {
        char *sash_dir = aake_path_join(opts->repo_root, "components/shell/sash");
        aake_ui_info("Executing Swift test suite for sash: %s", sash_dir);
        double t_start = aake_get_time_seconds();
        const char *const argv[] = { "swift", "test", "--package-path", sash_dir, NULL };
        int ret = aake_run_cmd(argv, false, NULL);
        double t_end = aake_get_time_seconds();
        free(sash_dir);
        printf("\n========================================\n");
        if (ret == 0) {
            printf("TEST SUITE SUCCESSFUL\n");
            printf("All sash shell unit tests passed.\n");
        } else {
            printf("TEST SUITE FAILED (exit code %d)\n", ret);
        }
        printf("Elapsed: %.2fs\n", t_end - t_start);
        printf("========================================\n");
        return ret;
    }

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
