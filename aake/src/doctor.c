#define _POSIX_C_SOURCE 200809L
#include "doctor.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static void print_check(const char *name, bool ok, const char *details, const char *remedy) {
    if (ok) {
        printf("  [OK]   %-24s : %s\n", name, details ? details : "Available");
    } else {
        printf("  [FAIL] %-24s : %s\n", name, details ? details : "Missing or incompatible");
        if (remedy && *remedy) {
            printf("         -> Action: %s\n", remedy);
        }
    }
}

int aake_doctor_check_all(const aake_options_t *opts) {
    printf("========================================\n");
    printf("ARK-OS Environment Diagnostic (Doctor)\n");
    printf("========================================\n\n");

    int failures = 0;

    /* 1. Host OS and Architecture */
    struct utsname un;
    if (uname(&un) == 0) {
        char os_info[512];
        snprintf(os_info, sizeof(os_info), "%s %s (%s)", un.sysname, un.release, un.machine);
        bool os_ok = (strcmp(un.sysname, "Linux") == 0);
        bool arch_ok = (strcmp(un.machine, "x86_64") == 0);
        print_check("Host Platform", os_ok && arch_ok, os_info, "ARK-OS development requires Linux x86_64");
        if (!os_ok || !arch_ok) failures++;
    } else {
        print_check("Host Platform", false, "uname failed", "Verify POSIX environment");
        failures++;
    }

    /* 2. C Compiler */
    char *cc = aake_which("gcc");
    if (!cc) cc = aake_which("clang");
    if (cc) {
        print_check("Host C Compiler", true, cc, NULL);
        free(cc);
    } else {
        print_check("Host C Compiler", false, "Not found", "Install gcc or clang via package manager");
        failures++;
    }

    /* 3. Ninja */
    char *ninja = aake_which("ninja");
    if (ninja) {
        print_check("Ninja Build System", true, ninja, NULL);
        free(ninja);
    } else {
        print_check("Ninja Build System", false, "Not found", "Install ninja (e.g., sudo pacman -S ninja or apt install ninja-build)");
        failures++;
    }

    /* 4. Swift Toolchain */
    char *swiftc = aake_which("swiftc");
    if (swiftc) {
        print_check("Swift Toolchain", true, swiftc, NULL);
        free(swiftc);
    } else {
        print_check("Swift Toolchain", false, "Not found", "Install Swift toolchain and add swiftc to PATH");
        failures++;
    }

    /* 5. Swift Static Linux SDK */
    const char *home = getenv("HOME");
    char sdk_path[1024] = "";
    bool sdk_ok = false;
    if (home) {
        snprintf(sdk_path, sizeof(sdk_path),
                 "%s/.swiftpm/swift-sdks/swift-6.4.0-RELEASE_static-linux-0.1.0.artifactbundle/swift-6.4.0-RELEASE_static-linux-0.1.0/swift-linux-musl/musl-1.2.5.sdk/x86_64",
                 home);
        sdk_ok = aake_dir_exists(sdk_path);
    }
    if (sdk_ok) {
        print_check("Swift Static SDK", true, sdk_path, NULL);
    } else {
        print_check("Swift Static SDK", false, "musl-1.2.5.sdk not found",
                    "Install Swift Static Linux SDK via `swift sdk install <url>`");
        /* Note: core build can still use host-glibc if SDK missing, so we flag as warning/fail depending on target */
    }

    /* 6. mimalloc Object */
    char *mimalloc_path = aake_path_join(opts->repo_root, "components/boot/build/sysroot/lib/mimalloc.o");
    bool mimalloc_ok = aake_file_exists(mimalloc_path);
    if (!mimalloc_ok) {
        /* Check scratch fallback from previous build if present */
        const char *scratch = getenv("MIMALLOC_OBJ");
        if (scratch && aake_file_exists(scratch)) {
            mimalloc_ok = true;
            free(mimalloc_path);
            mimalloc_path = aake_strdup(scratch);
        }
    }
    if (mimalloc_ok) {
        print_check("mimalloc Object", true, mimalloc_path, NULL);
    } else {
        print_check("mimalloc Object", false, "build/sysroot/lib/mimalloc.o missing",
                    "Run sysbuilts sync or specify MIMALLOC_OBJ=<path>");
    }
    free(mimalloc_path);

    /* 7. QEMU Emulator (Optional for build, required for boot-testing) */
    char *qemu = aake_which("qemu-system-x86_64");
    if (qemu) {
        print_check("QEMU Emulator", true, qemu, NULL);
        free(qemu);
    } else {
        print_check("QEMU Emulator", false, "qemu-system-x86_64 not found (needed for aake test --uefi)",
                    "Install qemu-system-x86_64 via package manager");
    }

    /* 8. UEFI Firmware (OVMF) */
    bool ovmf_ok = aake_file_exists("/usr/share/edk2/x64/OVMF.4m.fd") ||
                   aake_file_exists("/usr/share/OVMF/OVMF_CODE.fd") ||
                   aake_file_exists("/usr/share/ovmf/OVMF.fd");
    if (ovmf_ok) {
        print_check("UEFI Firmware (OVMF)", true, "Available", NULL);
    } else {
        print_check("UEFI Firmware (OVMF)", false, "OVMF.4m.fd not found in standard paths",
                    "Install edk2-ovmf via package manager");
    }

    /* 9. Manifests */
    char *comp_toml = aake_path_join(opts->repo_root, "manifests/components.toml");
    char *tgt_toml = aake_path_join(opts->repo_root, "manifests/targets/x86_64.toml");
    bool comp_ok = aake_file_exists(comp_toml);
    bool tgt_ok = aake_file_exists(tgt_toml);
    print_check("Component Manifest", comp_ok, comp_toml, "Verify manifests/components.toml exists");
    print_check("Target Manifest", tgt_ok, tgt_toml, "Verify manifests/targets/x86_64.toml exists");
    free(comp_toml);
    free(tgt_toml);
    if (!comp_ok || !tgt_ok) failures++;

    printf("\n========================================\n");
    if (failures == 0) {
        printf("Doctor Summary: All core environment checks passed.\n");
        printf("Ready for building ARK-OS.\n");
    } else {
        printf("Doctor Summary: %d core environment check(s) failed.\n", failures);
        printf("Resolve failed items before proceeding with build.\n");
    }
    printf("========================================\n");

    return (failures == 0) ? 0 : 1;
}
