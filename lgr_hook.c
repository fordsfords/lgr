/* lgr_hook.c - used to hook some functions. */

/* Intercept CPRT_TIMEOFDAY */
#define gettimeofday tst_timeofday
#define cprt_timeofday tst_timeofday

#include "cprt.h"

#include "lgr.c"
