#ifndef AAKE_MANIFEST_H
#define AAKE_MANIFEST_H

#include <stdbool.h>

typedef struct {
    char name[64];
    char type[32];         /* "kernel", "boot", "runtime", "service", "library" */
    char path[256];
    char language[32];     /* "c", "swift", etc. */
    char binary_name[64];
    char install_path[128];
    char description[256];
} aake_component_t;

typedef struct {
    char architecture[32];
    char platform[64];
    char triple[64];
    char kernel_artifact[128];
    char initramfs_format[32];
    char init_path[64];
    char emulator_binary[64];
    char emulator_machine[32];
    char emulator_cmdline[256];
} aake_target_info_t;

typedef struct {
    aake_component_t components[16];
    int component_count;
    aake_target_info_t target_info;
} aake_manifest_set_t;

int aake_manifest_load(const char *repo_root, aake_manifest_set_t *out_set);
const aake_component_t *aake_manifest_find_component(const aake_manifest_set_t *set, const char *name);

#endif /* AAKE_MANIFEST_H */
