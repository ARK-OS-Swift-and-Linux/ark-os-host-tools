#define _POSIX_C_SOURCE 200809L
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

char *aake_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

char *aake_path_join(const char *dir, const char *file) {
    if (!dir || !file) return NULL;
    size_t dlen = strlen(dir);
    size_t flen = strlen(file);
    bool need_slash = (dlen > 0 && dir[dlen - 1] != '/' && flen > 0 && file[0] != '/');
    char *res = malloc(dlen + flen + (need_slash ? 2 : 1));
    if (!res) return NULL;
    sprintf(res, "%s%s%s", dir, need_slash ? "/" : "", file);
    return res;
}

char *aake_trim(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool aake_file_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
}

bool aake_dir_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int aake_mkdir_p(const char *path) {
    if (!path || !*path) return 0;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

char *aake_find_repo_root(void) {
    const char *env_root = getenv("ARKOS_ROOT");
    if (env_root && aake_dir_exists(env_root)) {
        char *manifest_test = aake_path_join(env_root, "manifests/components.toml");
        if (aake_file_exists(manifest_test)) {
            free(manifest_test);
            return aake_strdup(env_root);
        }
        free(manifest_test);
    }

    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;

    char curr[1024];
    snprintf(curr, sizeof(curr), "%s", cwd);

    while (1) {
        char *manifest = aake_path_join(curr, "manifests/components.toml");
        char *agents = aake_path_join(curr, "AGENTS.md");
        if (aake_file_exists(manifest) && aake_file_exists(agents)) {
            free(manifest);
            free(agents);
            return aake_strdup(curr);
        }
        free(manifest);
        free(agents);

        char *last_slash = strrchr(curr, '/');
        if (!last_slash || last_slash == curr) {
            break;
        }
        *last_slash = '\0';
    }

    return aake_strdup(cwd);
}

char *aake_read_file(const char *path, size_t *out_size) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);

    if (out_size) *out_size = read_bytes;
    return buf;
}

char *aake_which(const char *binary) {
    if (!binary || !*binary) return NULL;
    if (strchr(binary, '/')) {
        if (access(binary, X_OK) == 0) return aake_strdup(binary);
        return NULL;
    }

    const char *path_env = getenv("PATH");
    if (!path_env) path_env = "/usr/bin:/bin";

    char *path_copy = aake_strdup(path_env);
    char *token = strtok(path_copy, ":");
    while (token) {
        char *full_path = aake_path_join(token, binary);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }
        free(full_path);
        token = strtok(NULL, ":");
    }
    free(path_copy);
    return NULL;
}

int aake_run_cmd(const char *const argv[], bool capture_output, char **out_buf) {
    if (!argv || !argv[0]) return -1;

    int pipefd[2] = {-1, -1};
    if (capture_output) {
        if (pipe(pipefd) != 0) return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (capture_output) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }

    if (pid == 0) {
        /* Child */
        if (capture_output) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    /* Parent */
    if (capture_output) {
        close(pipefd[1]);
        size_t cap = 4096;
        size_t len = 0;
        char *buf = malloc(cap);
        ssize_t n;
        char chunk[1024];
        while ((n = read(pipefd[0], chunk, sizeof(chunk))) > 0) {
            if (len + n + 1 >= cap) {
                cap = (cap * 2) + n;
                buf = realloc(buf, cap);
            }
            memcpy(buf + len, chunk, n);
            len += n;
        }
        buf[len] = '\0';
        close(pipefd[0]);
        if (out_buf) *out_buf = buf;
        else free(buf);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

double aake_get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}
