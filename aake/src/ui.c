#define _POSIX_C_SOURCE 200809L
#include "ui.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

bool aake_ui_is_terminal(void) {
    return isatty(fileno(stdout));
}

void aake_ui_print_header(const char *target, const char *backend, const char *config) {
    printf("======================================== ARK-OS Aake\n");
    printf("Target:        %s\n", target ? target : "x86_64");
    printf("Backend:       %s\n", backend ? backend : "Ninja");
    printf("Configuration: %s\n", config ? config : "release");
    printf("========================================\n\n");
    fflush(stdout);
}

void aake_ui_print_action(int completed, int total, const char *action, const char *target_name) {
    if (total > 0) {
        printf("[%3d / %-3d] %s %s\n", completed, total, action ? action : "Building", target_name ? target_name : "");
    } else {
        printf("[  -- / -- ] %s %s\n", action ? action : "Building", target_name ? target_name : "");
    }
    fflush(stdout);
}

void aake_ui_print_action_done(double duration_sec, const char *target_name, bool success) {
    printf("[ %6.2fs ] %-32s %s\n", duration_sec, target_name ? target_name : "", success ? "OK" : "FAIL");
    fflush(stdout);
}

void aake_ui_print_summary(bool success, int total_targets, int total_actions, int failed_actions, double elapsed_seconds) {
    printf("\n========================================\n");
    if (success) {
        printf("BUILD SUCCESSFUL\n");
    } else {
        printf("BUILD FAILED\n");
    }
    if (total_targets > 0) {
        printf("Targets:    %d\n", total_targets);
    }
    if (total_actions > 0) {
        printf("Actions:    %d\n", total_actions);
    }
    printf("Failed:     %d\n", failed_actions);
    printf("Total Time: %.2fs\n", elapsed_seconds);
    printf("========================================\n");
    fflush(stdout);
}

void aake_ui_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[aake] Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    fflush(stderr);
}

void aake_ui_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[aake] Warning: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    fflush(stderr);
}

void aake_ui_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[aake] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
    fflush(stdout);
}
