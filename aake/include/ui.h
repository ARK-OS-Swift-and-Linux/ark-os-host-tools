#ifndef AAKE_UI_H
#define AAKE_UI_H

#include "aake.h"
#include <stdbool.h>

void aake_ui_print_header(const char *target, const char *backend, const char *config);
void aake_ui_print_action(int completed, int total, const char *action, const char *target_name);
void aake_ui_print_action_done(double duration_sec, const char *target_name, bool success);
void aake_ui_print_summary(bool success, int total_targets, int total_actions, int failed_actions, double elapsed_seconds);
void aake_ui_error(const char *fmt, ...);
void aake_ui_warn(const char *fmt, ...);
void aake_ui_info(const char *fmt, ...);
bool aake_ui_is_terminal(void);

#endif /* AAKE_UI_H */
