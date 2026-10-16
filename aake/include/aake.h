#ifndef AAKE_H
#define AAKE_H

#define AAKE_VERSION "0.1.0"
#define AAKE_DEFAULT_BACKEND "Ninja"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CMD_HELP,
    CMD_VERSION,
    CMD_DOCTOR,
    CMD_BUILD,
    CMD_TEST,
    CMD_CLEAN,
    CMD_UNKNOWN
} aake_cmd_t;

typedef enum {
    BOOT_NONE,
    BOOT_UEFI,
    BOOT_BIOS
} aake_boot_mode_t;

typedef struct {
    aake_cmd_t cmd;
    char *target;
    char *config;          /* "release" (default) or "debug" */
    char *repo_root;       /* absolute path to arkos root */
    char *out_dir;         /* "out" by default */
    bool verbose;
    bool color;
    aake_boot_mode_t boot_mode;
    int argc;
    char **argv;
} aake_options_t;

int aake_cmd_help(const aake_options_t *opts);
int aake_cmd_version(const aake_options_t *opts);
int aake_cmd_doctor(const aake_options_t *opts);
int aake_cmd_build(const aake_options_t *opts);
int aake_cmd_test(const aake_options_t *opts);
int aake_cmd_clean(const aake_options_t *opts);

#endif /* AAKE_H */
