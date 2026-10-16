#define _POSIX_C_SOURCE 200809L
#include "ninja_runner.h"
#include "ui.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int aake_ninja_run(const aake_options_t *opts, const char *ninja_file, const char *target, double *out_elapsed) {
    if (!opts || !ninja_file) return -1;

    char *ninja_bin = aake_which("ninja");
    if (!ninja_bin) {
        aake_ui_error("Ninja executable not found on PATH. Install ninja before running builds.");
        return 1;
    }

    const char *tgt = (target && *target) ? target : "all";
    aake_ui_print_header(tgt, "Ninja", opts->config ? opts->config : "release");

    double start_time = aake_get_time_seconds();

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        aake_ui_error("Failed to create communication pipe: %s", strerror(errno));
        free(ninja_bin);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        aake_ui_error("Failed to fork process: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        free(ninja_bin);
        return 1;
    }

    if (pid == 0) {
        /* Child: execute ninja */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        /* Set Ninja progress status format */
        setenv("NINJA_STATUS", "[%f/%t] ", 1);

        const char *args[8];
        int arg_idx = 0;
        args[arg_idx++] = ninja_bin;
        args[arg_idx++] = "-f";
        args[arg_idx++] = ninja_file;
        if (opts->verbose) {
            args[arg_idx++] = "-v";
        }
        args[arg_idx++] = tgt;
        args[arg_idx] = NULL;

        execvp(ninja_bin, (char *const *)args);
        _exit(127);
    }

    /* Parent */
    close(pipefd[1]);
    free(ninja_bin);

    FILE *stream = fdopen(pipefd[0], "r");
    char line[1024];
    int total_actions = 0;
    int completed_actions = 0;
    int failed_actions = 0;

    while (fgets(line, sizeof(line), stream)) {
        char *trimmed = aake_trim(line);
        if (*trimmed == '\0') continue;

        /* Parse Ninja progress format: [f/t] description */
        int cur_comp = 0, cur_tot = 0;
        if (sscanf(trimmed, "[%d/%d]", &cur_comp, &cur_tot) == 2) {
            completed_actions = cur_comp;
            total_actions = cur_tot;

            const char *desc = strchr(trimmed, ']');
            if (desc) {
                desc++;
                while (*desc == ' ') desc++;
                aake_ui_print_action(completed_actions, total_actions, desc, "");
            }
        } else if (strstr(trimmed, "ninja: no work to do.")) {
            printf("[   --   ] Everything is up to date (no work to do).\n");
        } else if (strstr(trimmed, "FAILED:")) {
            failed_actions++;
            fprintf(stderr, "%s\n", trimmed);
        } else {
            /* Diagnostic, compiler output, or warning */
            printf("%s\n", trimmed);
        }
        fflush(stdout);
    }

    fclose(stream);

    int status = 0;
    waitpid(pid, &status, 0);
    double end_time = aake_get_time_seconds();
    double elapsed = end_time - start_time;
    if (out_elapsed) *out_elapsed = elapsed;

    bool success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (!success && failed_actions == 0) {
        failed_actions = 1;
    }

    aake_ui_print_summary(success, (tgt && strcmp(tgt, "all") != 0) ? 1 : 4,
                          total_actions > 0 ? total_actions : completed_actions,
                          failed_actions, elapsed);

    return success ? 0 : (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
}
