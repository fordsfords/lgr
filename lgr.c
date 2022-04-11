/* lgr.c - fast logger. */
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#if ! defined(_WIN32)
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "lgr.h"


CPRT_THREAD_ENTRYPOINT lgr_thread(void *in_arg)
{
  lgr_t *lgr = (lgr_t *)in_arg;

  while (lgr->state != LGR_STATE_EXITING) {
    CPRT_SLEEP_MS(lgr->sleep_ms);
  }  /* while state */

  CPRT_THREAD_EXIT;
  return 0;
}  /* lgr_thread */


/* This list of strings must be kept in sync with the
 * corresponding "LGRERR_*" constant definitions in "lgr.h".
 * It is used by the lgr_err_str() function. */
static char *lgrerrs[] = {
  "LGRERR_OK",
  "LGRERR_BADPARM",
  "LGRERR_MALLOC",
  "LGRERR_FULL",
  "BAD_LGRERR", NULL};
#define BAD_LGRERR (sizeof(lgrerrs)/sizeof(lgrerrs[0]) - 2)

char *lgr_err_str(lgrerr_t lgrerr)
{
	if (lgrerr >= BAD_LGRERR) { return lgrerrs[BAD_LGRERR]; }  /* bad lgrerr */

	return lgrerrs[lgrerr];
}  /* lgr_err_str */


/* This list of strings must be kept in sync with the
 * corresponding "LGRSEV_*" constant definitions in "lgr.h".
 * It is used by the lgr_sev_str() function. */
static char *lgrsevs[] = {
  "LGRSEV_NONE",
  "LGRSEV_FYI",
  "LGRSEV_ATTN",
  "LGRSEV_WARN",
  "LGRSEV_ERR",
  "LGRSEV_FATAL",
  "BAD_LGRSEV", NULL };
#define BAD_LGRSEV (sizeof(lgrsevs)/sizeof(lgrsevs[0]) - 2)

char *lgr_sev_str(lgrsev_t lgrsev)
{
	if (lgrsev >= BAD_LGRSEV) { return lgrsevs[BAD_LGRSEV]; }  /* bad lgrsev */

	return lgrsevs[lgrsev];
}  /* lgr_sev_str */


lgrerr_t lgr_create(lgr_t **rtn_lgr, unsigned int msg_size,
    unsigned int q_size, unsigned int sleep_ms)
{
  lgr_t *lgr = (lgr_t *)malloc(sizeof(lgr_t));
  if (lgr == NULL) { return LGRERR_MALLOC; }

  lgr->pool_q = NULL;        lgr->log_q = NULL;
  lgr->q_size = q_size;      lgr->msg_size = msg_size;
  lgr->sleep_ms = sleep_ms;  lgr->state = LGR_STATE_INITIALIZING;

  CPRT_MUTEX_INIT(lgr->pool_get_mutex);
  CPRT_MUTEX_INIT(lgr->log_put_mutex);

  if (q_create(&(lgr->pool_q), q_size) != QERR_OK) {
    lgr_delete(lgr); return LGRERR_MALLOC;
  }
  if (q_create(&(lgr->log_q), q_size) != QERR_OK) {
    lgr_delete(lgr); return LGRERR_MALLOC;
  }

  int i;
  for (i = 0; i < q_size; i++) {
    lgr_log_t *log = (lgr_log_t *)malloc(sizeof(lgr_log_t) + msg_size);
    if (log == NULL) { lgr_delete(lgr); return LGRERR_MALLOC; }

    q_enq(lgr->pool_q, log);
  }

  CPRT_THREAD_CREATE(lgr->thread_id, lgr_thread, lgr);
  lgr->state = LGR_STATE_RUNNING;

  *rtn_lgr = lgr;
  return LGRERR_OK;
}  /* lgr_create */


lgrerr_t lgr_delete(lgr_t *lgr)
{
  if (lgr->state == LGR_STATE_RUNNING) {
    lgr->state = LGR_STATE_EXITING;
    CPRT_THREAD_JOIN(lgr->thread_id);  /* Wait for thread to exit. */
  }

  if (lgr->log_q != NULL) {
    lgr_log_t *log;
    /* User shouldn't delete logger if there are pending logs,
     * but just in case, minimize memory leaks. */
    while (q_deq(lgr->log_q, (void **)&log) == QERR_OK) {
      free(log);
    }
    q_delete(lgr->log_q);
    lgr->log_q = NULL;
  }

  if (lgr->pool_q != NULL) {
    lgr_log_t *log;
    while (q_deq(lgr->pool_q, (void **)&log) == QERR_OK) {
      free(log);
    }
    q_delete(lgr->pool_q);
    lgr->pool_q = NULL;
  }

  free(lgr);

  return LGRERR_OK;
}  /* lgr_delete */