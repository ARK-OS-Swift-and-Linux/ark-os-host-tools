#ifndef AAKE_UTILS_H
#define AAKE_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* String & path helpers */
char *aake_strdup(const char *s);
char *aake_path_join(const char *dir, const char *file);
char *aake_trim(char *str);
bool aake_file_exists(const char *path);
bool aake_dir_exists(const char *path);
int aake_mkdir_p(const char *path);
char *aake_find_repo_root(void);
char *aake_read_file(const char *path, size_t *out_size);
char *aake_which(const char *binary);

/* Process execution */
int aake_run_cmd(const char *const argv[], bool capture_output, char **out_buf);
double aake_get_time_seconds(void);

#endif /* AAKE_UTILS_H */
