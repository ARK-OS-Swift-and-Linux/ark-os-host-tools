#ifndef AAKE_NINJA_GEN_H
#define AAKE_NINJA_GEN_H

#include "aake.h"
#include "manifest.h"

int aake_ninja_generate(const aake_options_t *opts, const aake_manifest_set_t *manifest, const char *out_ninja_file);

#endif /* AAKE_NINJA_GEN_H */
