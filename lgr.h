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

#ifndef LGR_H
#define LGR_H

#include <inttypes.h>
#include "cprt.h"
#include "q.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Values for lgr_create() flags parameter (and lgr_t.flags field). */
#define LGR_FLAGS_NOLOCK   0x00000001
#define LGR_FLAGS_DEFER_TS 0x00000002

typedef unsigned int lgr_err_t;     /* See LGR_ERR_* definitions below. */

/* Most lgr APIs return "lgr_err_t".  These definitions must
 * be kept in sync with the "lgr_errs" string array in "lgr.c". */
#define LGR_ERR_OK 0       /* Success. */
#define LGR_ERR_QSIZE 1    /* Supplied q_size is not a power of 2. */
#define LGR_ERR_MSGSIZE 2  /* Supplied max_msg_size not > 0. */
#define LGR_ERR_FILESIZE 3 /* Supplied max_msg_size not > 0. */
#define LGR_ERR_MALLOC 4   /* No memory available. */
#define LGR_ERR_QFULL 5    /* No room in queue. */
#define LGR_ERR_EXITING 6  /* Lgr is exiting. */
#define LGR_ERR_SEVERITY 7 /* Bad severity value. */
#define LGR_LAST_ERR 7     /* Set to value of last "LGR_ERR_*" definition. */


typedef unsigned int lgr_sev_t;  /* See LGR_SEV_* definitions below. */

/* When logging a message, caller specifies a severity. These definitions
 * must be kept in sync with the "lgr_sevs" string array in "lgr.c". */
#define LGR_SEV_FYI  0   /* Informational. */
#define LGR_SEV_ATTN 1   /* No error, but intended to get attention. */
#define LGR_SEV_WARN 2   /* Error, but funny handled. */
#define LGR_SEV_ERR  3   /* Error, not fully handled, but can continue. */
#define LGR_SEV_FATAL 4  /* Error, cannot continue. */
#define LGR_LAST_SEV 4   /* Set to value of last "LGR_SEV_*" definition. */


/* Log entry. */
#define LGR_LOG_TYPE_MSG 0
#define LGR_LOG_TYPE_OVERFLOW 1
#define LGR_LOG_TYPE_QUIT 2

struct lgr_log_s {
  struct cprt_timeval tv;
  unsigned int type;   /* LGR_LOG_TYPE_* */
  lgr_sev_t severity;  /* LGR_SEV_* */
  char msg[1];
};
typedef struct lgr_log_s lgr_log_t;

/* Values for lgr_t.state */
#define LGR_STATE_INITIALIZING 1
#define LGR_STATE_RUNNING 2
#define LGR_STATE_EXITING 3

/* Logger object. */
struct lgr_s {
  unsigned int q_size;
  unsigned int max_msg_size;
  unsigned int sleep_ms;
  uint32_t flags;              /* See LGR_FLAGS_* constants above. */
  char *file_prefix;
  uint64_t max_file_size_bytes;

  int file_prefix_len;
  uint64_t cur_file_size_bytes;
  char *file_full_name;
  FILE *cur_out_fp;            /* Output stream. */
  int cur_out_wday;            /* 0=SUN..6=SAT. */
  unsigned int state;          /* See LGR_STATE_* constants above. */
  q_t *pool_q;
  q_t *log_q;
  CPRT_SPIN_T log_lock;        /* Used unless LGR_FLAGS_NOLOCK. */

  CPRT_SPIN_T overflow_lock;   /* Always used, even if LGR_FLAGS_NOLOCK. */
  unsigned int overflows[LGR_LAST_SEV + 1];  /* Number of queue overflows. */
  lgr_log_t overflow_log;      /* Dedicated log for queue overflow. */
  lgr_log_t quit_log;          /* Dedicated log for shutting down. */
  int overflow_log_available;  /* 0 = not available. */

  unsigned int file_size_drops[LGR_LAST_SEV + 1];

  CPRT_THREAD_T thread_id;
};
typedef struct lgr_s lgr_t;


char *lgr_sev_str(lgr_sev_t lgr_sev);
char *lgr_err2str(lgr_err_t lgr_err);
lgr_err_t lgr_create(lgr_t **rtn_lgr, unsigned int max_msg_size,
    unsigned int q_size, unsigned int sleep_ms, uint32_t flags,
    char *file_prefix, int max_file_size_mb);
lgr_err_t lgr_delete(lgr_t *lgr);
lgr_err_t lgr_log(lgr_t *lgr, unsigned int severity, char *fmt, ...);

#if defined(__cplusplus)
}
#endif

#endif  /* LGR_H */
