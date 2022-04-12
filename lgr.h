/* lgr.h - fast logger. */
/*
I want there to be NO barriers to using this code, so I am releasing it to the public domain.
But "public domain" does not have an internationally agreed-upon definition, so I use CC0:

Copyright 2022 Steven Ford http://geeky-boy.com and licensed
"public domain" style under
[CC0](http://creativecommons.org/publicdomain/zero/1.0/):
![CC0](https://licensebuttons.net/p/zero/1.0/88x31.png "CC0")

To the extent possible under law, the contributors to this project have
waived all copyright and related or neighboring rights to this work.
In other words, you can use this code for any purpose without any
restrictions.  This work is published from: United States.  The project home
is https://github.com/fordsfords/lgr

To contact me, Steve Ford, project owner, you can find my email address
at http://geeky-boy.com.  Can't see it?  Keep looking.
*/

#include <inttypes.h>
#include "cprt.h"
#include "q.h"

#ifndef LGR_H
#define LGR_H

typedef unsigned int lgrerr_t;     /* See LGRERR_* definitions below. */

/* Most lgr APIs return "lgrerr_t".  These definitions must
 * be kept in sync with the "lgrerrs" string array in "lgr.c". */
#define LGRERR_OK 0         /* Success. */
#define LGRERR_BADPARM 1    /* Bad input paramter value. */
#define LGRERR_MALLOC 2     /* No memory available. */
#define LGRERR_FULL 3       /* No room in queue. */
#define LGRERR_INTERNAL 4   /* Internal error. */
#define LAST_LGRERR 4   /* Set to value of last "LGRERR_*" definition. */


typedef unsigned int lgrsev_t;  /* See LGRSEV_* definitions below. */

/* When logging a message, caller specifies a severity. These definitions
 * must be kept in sync with the "lgrsevs" string array in "lgr.c". */
#define LGRSEV_NONE 0     /* Not used. */
#define LGRSEV_FYI  1     /* Informational. */
#define LGRSEV_ATTN 2     /* No error, but intended to get attention. */
#define LGRSEV_WARN 3     /* Error, but funny handled. */
#define LGRSEV_ERR  4     /* Error, not fully handled, but can continue. */
#define LGRSEV_FATAL 5    /* Error, cannot continue. */
#define LAST_LGRSEV 5   /* Set to value of last "LGRSEV_*" definition. */

/* Values for lgr_t.state */
#define LGR_STATE_INITIALIZING 1
#define LGR_STATE_RUNNING 2
#define LGR_STATE_EXITING 3

/* Logger. */
struct lgr_s {
  CPRT_MUTEX_T pool_get_mutex;
  CPRT_MUTEX_T log_put_mutex;
  q_t *pool_q;
  q_t *log_q;
  unsigned int q_size;
  unsigned int msg_size;
  unsigned int sleep_ms;
  unsigned int state;  /* See LGR_STATE_* above. */
  CPRT_THREAD_T thread_id;
};
typedef struct lgr_s lgr_t;

/* Log entry. */
struct lgr_log_s {
  struct timeval tv;
  unsigned int severity;
  char msg[1];
};
typedef struct lgr_log_s lgr_log_t;


char *lgr_sev_str(lgrsev_t lgrsev);
char *lgr_err_str(lgrerr_t lgrerr);
lgrerr_t lgr_create(lgr_t **rtn_lgr, unsigned int msg_size,
    unsigned int q_size, unsigned int sleep_ms);
lgrerr_t lgr_delete(lgr_t *lgr);
lgrerr_t lgr_log(lgr_t *lgr, unsigned int severity, char *fmt, ...);

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
}
#endif

#endif  /* LGR_H */
