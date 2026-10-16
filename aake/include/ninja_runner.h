#ifndef AAKE_NINJA_RUNNER_H
#define AAKE_NINJA_RUNNER_H

#include "aake.h"

int aake_ninja_run(const aake_options_t *opts, const char *ninja_file, const char *target, double *out_elapsed);

#endif /* AAKE_NINJA_RUNNER_H */
