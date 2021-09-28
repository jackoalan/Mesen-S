#ifndef MESEN_S_LUBEU
#define MESEN_S_LUBEU

#define PACKAGE_NAME "Mesen-S elfutils"
#define PACKAGE_VERSION ""
#define PACKAGE_URL ""

#define HAVE_ERR_H 1
#define HAVE_DECL_POWEROF2 1
#define HAVE_DECL_REALLOCARRAY 1

extern char *program_invocation_short_name;

#include "lib/eu-config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/mman.h>

#endif // MESEN_S_LUBEU
