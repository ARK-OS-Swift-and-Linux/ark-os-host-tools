#define _POSIX_C_SOURCE 200809L
#include "manifest.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_str(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dst_sz) len = dst_sz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void parse_key_value(const char *line, char *key, size_t key_sz, char *val, size_t val_sz) {
    key[0] = '\0';
    val[0] = '\0';
    const char *eq = strchr(line, '=');
    if (!eq) return;

    size_t klen = eq - line;
    if (klen >= key_sz) klen = key_sz - 1;
    memcpy(key, line, klen);
    key[klen] = '\0';
    aake_trim(key);

    const char *vstart = eq + 1;
    while (*vstart == ' ' || *vstart == '\t') vstart++;
    if (*vstart == '"') {
        vstart++;
        const char *vend = strrchr(vstart, '"');
        if (vend) {
            size_t vlen = vend - vstart;
            if (vlen >= val_sz) vlen = val_sz - 1;
            memcpy(val, vstart, vlen);
            val[vlen] = '\0';
            return;
        }
    }
    copy_str(val, val_sz, vstart);
    aake_trim(val);
}

int aake_manifest_load(const char *repo_root, aake_manifest_set_t *out_set) {
    if (!repo_root || !out_set) return -1;
    memset(out_set, 0, sizeof(*out_set));

    /* 1. Parse manifests/components.toml */
    char *comp_path = aake_path_join(repo_root, "manifests/components.toml");
    size_t comp_sz = 0;
    char *comp_content = aake_read_file(comp_path, &comp_sz);
    free(comp_path);

    if (comp_content) {
        char *line = strtok(comp_content, "\r\n");
        aake_component_t *curr = NULL;

        while (line) {
            char *trimmed = aake_trim(line);
            if (*trimmed == '#' || *trimmed == '\0') {
                line = strtok(NULL, "\r\n");
                continue;
            }

            if (*trimmed == '[' && trimmed[strlen(trimmed) - 1] == ']') {
                char header[128];
                size_t hlen = strlen(trimmed) - 2;
                if (hlen >= sizeof(header)) hlen = sizeof(header) - 1;
                memcpy(header, trimmed + 1, hlen);
                header[hlen] = '\0';

                if (strncmp(header, "components.", 11) == 0) {
                    if (out_set->component_count < 16) {
                        curr = &out_set->components[out_set->component_count++];
                        copy_str(curr->name, sizeof(curr->name), header + 11);
                    } else {
                        curr = NULL;
                    }
                } else {
                    curr = NULL;
                }
            } else if (curr) {
                char key[64], val[256];
                parse_key_value(trimmed, key, sizeof(key), val, sizeof(val));
                if (strcmp(key, "type") == 0) {
                    copy_str(curr->type, sizeof(curr->type), val);
                } else if (strcmp(key, "path") == 0) {
                    copy_str(curr->path, sizeof(curr->path), val);
                } else if (strcmp(key, "language") == 0) {
                    copy_str(curr->language, sizeof(curr->language), val);
                } else if (strcmp(key, "binary_name") == 0) {
                    copy_str(curr->binary_name, sizeof(curr->binary_name), val);
                } else if (strcmp(key, "install_path") == 0) {
                    copy_str(curr->install_path, sizeof(curr->install_path), val);
                } else if (strcmp(key, "description") == 0) {
                    copy_str(curr->description, sizeof(curr->description), val);
                }
            }
            line = strtok(NULL, "\r\n");
        }
        free(comp_content);
    }

    /* Check if arkdiag is present in filesystem, and add it if not explicitly in components.toml */
    bool has_arkdiag = false;
    for (int i = 0; i < out_set->component_count; i++) {
        if (strcmp(out_set->components[i].name, "arkdiag") == 0) {
            has_arkdiag = true;
            break;
        }
    }
    if (!has_arkdiag && out_set->component_count < 16) {
        char *diag_path = aake_path_join(repo_root, "components/services/arkdiag");
        if (aake_dir_exists(diag_path)) {
            aake_component_t *c = &out_set->components[out_set->component_count++];
            copy_str(c->name, sizeof(c->name), "arkdiag");
            copy_str(c->type, sizeof(c->type), "service");
            copy_str(c->path, sizeof(c->path), "components/services/arkdiag");
            copy_str(c->language, sizeof(c->language), "swift");
            copy_str(c->binary_name, sizeof(c->binary_name), "arkdiag");
            copy_str(c->install_path, sizeof(c->install_path), "/sbin/arkdiag");
            copy_str(c->description, sizeof(c->description), "System diagnostic monitor daemon");
        }
        free(diag_path);
    }

    /* 2. Parse manifests/targets/x86_64.toml */
    char *tgt_path = aake_path_join(repo_root, "manifests/targets/x86_64.toml");
    size_t tgt_sz = 0;
    char *tgt_content = aake_read_file(tgt_path, &tgt_sz);
    free(tgt_path);

    /* Defaults */
    copy_str(out_set->target_info.architecture, sizeof(out_set->target_info.architecture), "x86_64");
    copy_str(out_set->target_info.platform, sizeof(out_set->target_info.platform), "qemu-q35");
    copy_str(out_set->target_info.triple, sizeof(out_set->target_info.triple), "x86_64-unknown-linux-gnu");
    copy_str(out_set->target_info.kernel_artifact, sizeof(out_set->target_info.kernel_artifact), "arch/x86/boot/bzImage");
    copy_str(out_set->target_info.initramfs_format, sizeof(out_set->target_info.initramfs_format), "cpio.gz");
    copy_str(out_set->target_info.init_path, sizeof(out_set->target_info.init_path), "/sbin/arkrt");
    copy_str(out_set->target_info.emulator_binary, sizeof(out_set->target_info.emulator_binary), "qemu-system-x86_64");
    copy_str(out_set->target_info.emulator_machine, sizeof(out_set->target_info.emulator_machine), "q35");
    copy_str(out_set->target_info.emulator_cmdline, sizeof(out_set->target_info.emulator_cmdline), "console=ttyS0 earlyprintk=serial,ttyS0 rdinit=/sbin/arkrt panic=-1");

    if (tgt_content) {
        char *line = strtok(tgt_content, "\r\n");
        char section[64] = "";
        while (line) {
            char *trimmed = aake_trim(line);
            if (*trimmed == '#' || *trimmed == '\0') {
                line = strtok(NULL, "\r\n");
                continue;
            }

            if (*trimmed == '[' && trimmed[strlen(trimmed) - 1] == ']') {
                size_t slen = strlen(trimmed) - 2;
                if (slen >= sizeof(section)) slen = sizeof(section) - 1;
                memcpy(section, trimmed + 1, slen);
                section[slen] = '\0';
            } else {
                char key[64], val[256];
                parse_key_value(trimmed, key, sizeof(key), val, sizeof(val));
                if (strcmp(section, "target") == 0) {
                    if (strcmp(key, "architecture") == 0) copy_str(out_set->target_info.architecture, sizeof(out_set->target_info.architecture), val);
                    else if (strcmp(key, "platform") == 0) copy_str(out_set->target_info.platform, sizeof(out_set->target_info.platform), val);
                    else if (strcmp(key, "triple") == 0) copy_str(out_set->target_info.triple, sizeof(out_set->target_info.triple), val);
                } else if (strcmp(section, "kernel") == 0) {
                    if (strcmp(key, "artifact") == 0) copy_str(out_set->target_info.kernel_artifact, sizeof(out_set->target_info.kernel_artifact), val);
                } else if (strcmp(section, "initramfs") == 0) {
                    if (strcmp(key, "format") == 0) copy_str(out_set->target_info.initramfs_format, sizeof(out_set->target_info.initramfs_format), val);
                    else if (strcmp(key, "init_path") == 0) copy_str(out_set->target_info.init_path, sizeof(out_set->target_info.init_path), val);
                } else if (strcmp(section, "emulator") == 0) {
                    if (strcmp(key, "binary") == 0) copy_str(out_set->target_info.emulator_binary, sizeof(out_set->target_info.emulator_binary), val);
                    else if (strcmp(key, "machine") == 0) copy_str(out_set->target_info.emulator_machine, sizeof(out_set->target_info.emulator_machine), val);
                    else if (strcmp(key, "cmdline") == 0) copy_str(out_set->target_info.emulator_cmdline, sizeof(out_set->target_info.emulator_cmdline), val);
                }
            }
            line = strtok(NULL, "\r\n");
        }
        free(tgt_content);
    }

    return 0;
}

const aake_component_t *aake_manifest_find_component(const aake_manifest_set_t *set, const char *name) {
    if (!set || !name) return NULL;
    for (int i = 0; i < set->component_count; i++) {
        if (strcmp(set->components[i].name, name) == 0) {
            return &set->components[i];
        }
    }
    return NULL;
}
