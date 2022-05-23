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

#include "cprt.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if ! defined(_WIN32)
  #include <stdlib.h>
  #include <unistd.h>
  #include <inttypes.h>
  #include <sys/time.h>
#endif

#include "lgr.h"

CPRT_THREAD_ENTRYPOINT lgr_thread(void *in_arg);


/* This list of strings must be kept in sync with the
 * corresponding "LGR_ERR_*" constant definitions in "lgr.h".
 * It is used by the lgr_err2str() function. */
static char *lgr_errs[LGR_LAST_ERR + 3] = {
  "OK",
  "MALLOC",
  "QFULL",
  "EXITING",
  "SEVERITY",
  "BAD_LGR_ERR",
  NULL};
#define BAD_LGR_ERR (sizeof(lgr_errs)/sizeof(lgr_errs[0]) - 2)

char *lgr_err2str(lgr_err_t lgr_err)
{
	if (lgr_err >= BAD_LGR_ERR) {  /* bad lgr_err */
      return lgr_errs[BAD_LGR_ERR];
    }

	return lgr_errs[lgr_err];
}  /* lgr_err2str */


/* This list of strings must be kept in sync with the
 * corresponding "LGR_SEV_*" constant definitions in "lgr.h".
 * It is used by the lgr_sev2str() function. */
static char *lgr_sevs[LGR_LAST_SEV + 3] = {
  "FYI",
  "ATTN",
  "WARN",
  "ERR",
  "FATAL",
  "BAD_LGR_SEV",
  NULL };
#define BAD_LGR_SEV (sizeof(lgr_sevs)/sizeof(lgr_sevs[0]) - 2)

char *lgr_sev2str(lgr_sev_t lgr_sev)
{
	if (lgr_sev >= BAD_LGR_SEV) {  /* bad lgr_sev */
      return lgr_sevs[BAD_LGR_SEV];
    }

	return lgr_sevs[lgr_sev];
}  /* lgr_sev2str */


lgr_err_t lgr_create(lgr_t **rtn_lgr, unsigned int max_msg_size,
    unsigned int q_size, unsigned int sleep_ms, uint32_t flags,
    char *file_prefix, int max_file_size_mb)
{
  int i;
  qerr_t qerr;
  lgr_t *lgr = (lgr_t *)malloc(sizeof(lgr_t));
  if (lgr == NULL) { return LGR_ERR_MALLOC; }

  lgr->max_msg_size = max_msg_size;
  lgr->q_size = q_size;
  lgr->sleep_ms = sleep_ms;
  lgr->flags = flags;
  lgr->file_prefix = NULL;
  lgr->file_full_name = NULL;
  lgr->max_file_size_bytes = (uint64_t)max_file_size_mb * 1024 * 1024;
  lgr->cur_file_size_bytes = 0;

  lgr->state = LGR_STATE_INITIALIZING;
  lgr->pool_q = NULL;
  lgr->log_q = NULL;

  /* Per-severity counters. */
  for (i = 0; i <= LGR_LAST_SEV; i++) {
    lgr->overflows[i] = 0;
    lgr->file_size_drops[i] = 0;
  }
  lgr->overflow_log.type = LGR_LOG_TYPE_OVERFLOW;
  lgr->overflow_log_available = 1;

  lgr->quit_log.type = LGR_LOG_TYPE_QUIT;

  if (! (lgr->flags & LGR_FLAGS_NOLOCK)) {
    CPRT_SPIN_INIT(lgr->log_lock);
  }
  CPRT_SPIN_INIT(lgr->overflow_lock);  /* See doc #locking. */

  lgr->file_prefix_len = strlen(file_prefix);
  lgr->file_prefix = strdup(file_prefix);
  if (lgr->file_prefix == NULL) {
    lgr_delete(lgr); return LGR_ERR_MALLOC;
  }
  /* Allow space for suffix "_xxx" and trailing NUL. */
  lgr->file_full_name = malloc(lgr->file_prefix_len + 5);
  if (lgr->file_full_name == NULL) {
    lgr_delete(lgr); return LGR_ERR_MALLOC;
  }
  memset(lgr->file_full_name, '\0', lgr->file_prefix_len + 5);

  /* Pool of available log objects. */
  if (q_create(&(lgr->pool_q), q_size) != QERR_OK) {
    lgr_delete(lgr); return LGR_ERR_MALLOC;
  }
  /* Queue for outgoing logs. */
  if (q_create(&(lgr->log_q), q_size) != QERR_OK) {
    lgr_delete(lgr); return LGR_ERR_MALLOC;
  }

  /* Create log objects and add them to the pool. But remember that the "q"
   * can never be fuller than q_size - 1. Also, want to leave room for "quit"
   * and "overflow" logs. So create 3 fewer than the queue size. */
  for (i = 0; i < (q_size - 3); i++) {
    /* Leave extra room in string buffer for NUL and truncate test. */
    lgr_log_t *log = (lgr_log_t *)malloc(sizeof(lgr_log_t) + max_msg_size + 2);
    if (log == NULL) { lgr_delete(lgr); return LGR_ERR_MALLOC; }

    log->type = LGR_LOG_TYPE_MSG;
    qerr = q_enq(lgr->pool_q, log);
    CPRT_ASSERT(qerr == QERR_OK);
  }

  CPRT_THREAD_CREATE(lgr->thread_id, lgr_thread, lgr);
  /* Wait for thread to finish initialization. */
  while (lgr->state == LGR_STATE_INITIALIZING) {
    CPRT_SLEEP_MS(1);
  }

  *rtn_lgr = lgr;
  return LGR_ERR_OK;
}  /* lgr_create */


lgr_err_t lgr_delete(lgr_t *lgr)
{
  qerr_t qerr;

  if (lgr->state != LGR_STATE_RUNNING) {
    return LGR_ERR_EXITING;
  }

  lgr->state = LGR_STATE_EXITING;
  qerr = q_enq(lgr->log_q, (void *)&(lgr->quit_log));
  CPRT_ASSERT(qerr == QERR_OK);  /* The q_enq should always succeed. */
  CPRT_THREAD_JOIN(lgr->thread_id);  /* Wait for thread to exit. */

  if (lgr->log_q != NULL) {
    lgr_log_t *log;
    /* By now there should be no remaining entries in the log q.
     * But just in case, minimize memory leaks. */
    while (q_deq(lgr->log_q, (void **)&log) == QERR_OK) {
      if (log->type == LGR_LOG_TYPE_MSG) {
        free(log);
      }
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

  if (lgr->file_full_name != NULL) {
    free(lgr->file_full_name);
    lgr->file_full_name = NULL;
  }
  if (lgr->file_prefix != NULL) {
    free(lgr->file_prefix);
    lgr->file_prefix = NULL;
  }

  if (! (lgr->flags & LGR_FLAGS_NOLOCK)) {
    CPRT_SPIN_DELETE(lgr->log_lock);
  }
  CPRT_SPIN_DELETE(lgr->overflow_lock);

  free(lgr);

  return LGR_ERR_OK;
}  /* lgr_delete */


/* Called by lgr_log() when an overflow happens. */
void lgr_enqueue_overflow(lgr_t *lgr, unsigned int severity)
{
  qerr_t qerr;

  CPRT_ASSERT(severity >= 0 && severity <= LGR_LAST_SEV);

  CPRT_SPIN_LOCK(lgr->overflow_lock);

  lgr->overflows[severity] ++;

  if (lgr->overflow_log_available) {
    lgr->overflow_log_available = 0;
    CPRT_TIMEOFDAY(&(lgr->overflow_log.tv), NULL);
    qerr = q_enq(lgr->log_q, (void *)&(lgr->overflow_log));
    CPRT_ASSERT(qerr == QERR_OK);  /* The q_enq should always succeed. */
  }

  CPRT_SPIN_UNLOCK(lgr->overflow_lock);
}  /* lgr_enqueue_overflow */


lgr_err_t lgr_log(lgr_t *lgr, unsigned int severity, char *fmt, ...)
{
  lgr_log_t *log;
  qerr_t qerr;
  va_list args;

  if (severity < 0 || severity > LGR_LAST_SEV) {
    return LGR_ERR_SEVERITY;
  }
  if (lgr->state != LGR_STATE_RUNNING) {
    return LGR_ERR_EXITING;
  }

  if (! (lgr->flags & LGR_FLAGS_NOLOCK)) {
    CPRT_SPIN_LOCK(lgr->log_lock);
  }

  qerr = q_deq(lgr->pool_q, (void **)&log);
  if (qerr == QERR_EMPTY) {
    lgr_enqueue_overflow(lgr, severity);
    if (! (lgr->flags & LGR_FLAGS_NOLOCK)) {
      CPRT_SPIN_UNLOCK(lgr->log_lock);
    }
    return LGR_ERR_QFULL;  /* No free logs means the logger is full. */
  }
  CPRT_ASSERT(qerr == QERR_OK);

  /* If user specified LGR_FLAGS_DEFER_TS, take timestamp in logger thread.
   * If not, then take timestamp now. */
  if (! (lgr->flags & LGR_FLAGS_DEFER_TS)) {
    /* LGR_FLAGS_DEFER_TS not specified, take timestamp here. */
    CPRT_TIMEOFDAY(&(log->tv), NULL);
  }
  log->severity = severity;

  /* Truncate test: preset the NUL for the max allowable message.
   * Then do the sprintf into the full buffer (2 larger max message).
   * Then the logger thread checks the NUL for the max allowable message. */
  log->msg[lgr->max_msg_size] = '\0';

  va_start(args, fmt);
  /* Leave room for NUL and truncate test. */
  vsnprintf(log->msg, lgr->max_msg_size + 2, fmt, args);
  va_end(args);

  /* The log queue should always have room. */
  CPRT_ASSERT(q_enq(lgr->log_q, (void *)log) == QERR_OK);

  if (! (lgr->flags & LGR_FLAGS_NOLOCK)) {
    CPRT_SPIN_UNLOCK(lgr->log_lock);
  }

  return LGR_ERR_OK;
}  /* lgr_log */


static char *wday2str[7] = {
  "sun", "mon", "tue", "wed", "thu", "fri", "sat"
};


void lgr_manage_file(lgr_t *lgr, int wday)
{
  struct cprt_timeval cur_tv;
  struct tm tm_buf;

  if (lgr->cur_out_wday != wday) {
    /* New day. Close yesterday's file. */
    if (lgr->cur_out_fp != NULL) {
      CPRT_TIMEOFDAY(&cur_tv, NULL);
      CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
      lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
          "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Closing file.\n",
          tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
          tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
          (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_FYI));
      fclose(lgr->cur_out_fp);
      lgr->cur_out_fp = NULL;
    }

    /* Open new day's file. */
    snprintf(lgr->file_full_name, lgr->file_prefix_len + 5, "%s_%s",
        lgr->file_prefix, wday2str[wday]);
    CPRT_ASSERT(lgr->file_full_name[lgr->file_prefix_len + 4] == '\0');
    lgr->cur_out_fp = fopen(lgr->file_full_name, "w");
    if (lgr->cur_out_fp == NULL) {
      CPRT_PERRNO("ERROR: lgr: fopen failed");
    }
    lgr->cur_file_size_bytes = 0;

    CPRT_TIMEOFDAY(&cur_tv, NULL);
    CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
    lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
        "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Opening file.\n",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_FYI));

    lgr->cur_out_wday = wday;
  }

  if (lgr->cur_out_fp != NULL) {
    unsigned int drops[LGR_LAST_SEV + 1];
    int i, tot_file_size_drops;

    if (lgr->cur_file_size_bytes >= lgr->max_file_size_bytes) {
      CPRT_TIMEOFDAY(&cur_tv, NULL);
      CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
      lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
          "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Log file size exceeded.\n",
          tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
          tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
          (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_ERR));
      fclose(lgr->cur_out_fp);
      lgr->cur_out_fp = NULL;
    }

    tot_file_size_drops = 0;
    for (i = 0; i <= LGR_LAST_SEV; i++) {
      drops[i] = lgr->file_size_drops[i];
      tot_file_size_drops += lgr->file_size_drops[i];
      lgr->file_size_drops[i] = 0;
    }
    if (tot_file_size_drops > 0) {
      CPRT_TIMEOFDAY(&cur_tv, NULL);
      CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
      lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
          "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: File size drops, "
            "FYI:%u, ATTN:%u, WARN:%u, ERR:%u, FATAL:%u "
            "logs dropped\n",
          tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
          tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
          (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_ERR),
          drops[LGR_SEV_FYI], drops[LGR_SEV_ATTN],
          drops[LGR_SEV_WARN], drops[LGR_SEV_ERR],
          drops[LGR_SEV_FATAL]);
    }
  }
}  /* lgr_manage_file */


void lgr_handle_oveflow(lgr_t *lgr, lgr_log_t *log)
{
  unsigned int overflows[LGR_LAST_SEV + 1];
  struct cprt_timeval cur_tv;
  struct tm tm_buf;
  double time_diff_sec;
  unsigned int tot_overflows;
  int i;

  CPRT_SPIN_LOCK(lgr->overflow_lock);

  CPRT_TIMEOFDAY(&cur_tv, NULL);

  tot_overflows = 0;
  for (i = 0; i <= LGR_LAST_SEV; i++) {
    overflows[i] = lgr->overflows[i];
    tot_overflows += lgr->overflows[i];
    lgr->overflows[i] = 0;
  }
  lgr->overflow_log_available = 1;

  CPRT_SPIN_UNLOCK(lgr->overflow_lock);

  if (tot_overflows > 0) {
    time_diff_sec = cur_tv.tv_sec;
    time_diff_sec -= log->tv.tv_sec;
    time_diff_sec += (double)cur_tv.tv_usec / (double)1000000;
    time_diff_sec -= (double)log->tv.tv_usec / (double)1000000;

    CPRT_LOCALTIME_R(&(log->tv.tv_sec), &tm_buf);  /* Parse time stamp. */
    lgr_manage_file(lgr, tm_buf.tm_wday);
    if (lgr->cur_out_fp != NULL) {
      lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
          "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Overflow, "
            "FYI:%u, ATTN:%u, WARN:%u, ERR:%u, FATAL:%u "
            "logs dropped over %f sec\n",
          tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
          tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
          (int)log->tv.tv_usec, lgr_sev2str(LGR_SEV_ERR),
          overflows[LGR_SEV_FYI], overflows[LGR_SEV_ATTN],
          overflows[LGR_SEV_WARN], overflows[LGR_SEV_ERR],
          overflows[LGR_SEV_FATAL], time_diff_sec);
    }
  }
}  /* lgr_handle_oveflow */


void lgr_handle_log(lgr_t *lgr, lgr_log_t *log)
{
  struct tm tm_buf;

  if (lgr->flags & LGR_FLAGS_DEFER_TS) {
    CPRT_TIMEOFDAY(&(log->tv), NULL);
  }
  /* Truncate test: log API preset the NUL for the max allowable message,
   * then did the sprintf into the full buffer (2 larger max message).
   * Now check the NUL for the max allowable message. */
  char *msg_suffix = "";
  if (log->msg[lgr->max_msg_size] != '\0') {
    log->msg[lgr->max_msg_size] = '\0';
    msg_suffix = "...(message truncated)";
  }

  CPRT_LOCALTIME_R(&(log->tv.tv_sec), &tm_buf);  /* Parse time stamp. */
  lgr_manage_file(lgr, tm_buf.tm_wday);
  if (lgr->cur_out_fp != NULL) {
    lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
        "%04d/%02d/%02d %02d:%02d:%02d.%06d %s %s%s\n",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        (int)log->tv.tv_usec, lgr_sev2str(log->severity), log->msg,
        msg_suffix);
  }
  else {  /* File closed, accumulate file size drops. */
    CPRT_ASSERT(log->severity >= 0 && log->severity <= LGR_LAST_SEV);
    lgr->file_size_drops[log->severity] ++;
  }

  CPRT_ASSERT(q_enq(lgr->pool_q, (void *)log) == QERR_OK);
}  /* lgr_handle_log */


CPRT_THREAD_ENTRYPOINT lgr_thread(void *in_arg)
{
  lgr_t *lgr = (lgr_t *)in_arg;
  struct cprt_timeval cur_tv;
  struct tm tm_buf;
  int need_flush;
  int quitting;

  lgr->cur_out_fp = NULL;
  /* Guarantee first call to lgr_manage_file() results in "day change". */
  lgr->cur_out_wday = 99;

  CPRT_TIMEOFDAY(&cur_tv, NULL);
  CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
  lgr_manage_file(lgr, tm_buf.tm_wday);
  if (lgr->cur_out_fp != NULL) {
    lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
      "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Starting.\n",
      tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
      (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_FYI));
  }
  need_flush = 1;

  /* Release the "lgr_create()" call. */
  lgr->state = LGR_STATE_RUNNING;

  quitting = 0;
  while (! quitting) {
    lgr_log_t *log;
    qerr_t qerr;
    while ((qerr = q_deq(lgr->log_q, (void **)&log)) == QERR_OK) {
      need_flush = 1;

      if (log->type == LGR_LOG_TYPE_QUIT) {
        quitting = 1;
      }
      else if (log->type == LGR_LOG_TYPE_OVERFLOW) {
        lgr_handle_oveflow(lgr, log);
      }
      else if (log->type == LGR_LOG_TYPE_MSG) {
        lgr_handle_log(lgr, log);
      }
      else {  /* Bad log type; log object corrupted? */
        CPRT_TIMEOFDAY(&cur_tv, NULL);
        CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
        lgr_manage_file(lgr, tm_buf.tm_wday);
        if (lgr->cur_out_fp != NULL) {
          lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
            "%04d/%02d/%02d %02d:%02d:%02d.%06d %s Bad log type (%d)\n",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            (int)log->tv.tv_usec, lgr_sev2str(LGR_SEV_ERR),
            log->type);
        }
        /* Corrupted log object, do not put it into the pool. */
      }
    }  /* while dequeue */
    CPRT_ASSERT(qerr == QERR_EMPTY);

    if (! quitting) {
      /* Log queue empty, flush log file if needed. */
      if (need_flush) {
        if (lgr->cur_out_fp != NULL) {
          fflush(lgr->cur_out_fp);
        }
        need_flush = 0;
      }

      /* If log queue still empty, sleep. */
      if (q_is_empty(lgr->log_q)) {
        CPRT_SLEEP_MS(lgr->sleep_ms);
      }
    }
  }  /* while ! quitting */

  CPRT_TIMEOFDAY(&cur_tv, NULL);
  CPRT_LOCALTIME_R(&cur_tv.tv_sec, &tm_buf);  /* Parse time stamp. */
  /* Don't call lgr_manage_file(). Don't want to create new file for exit. */
  if (lgr->cur_out_fp != NULL) {
    lgr->cur_file_size_bytes += fprintf(lgr->cur_out_fp,
      "%04d/%02d/%02d %02d:%02d:%02d.%06d %s lgr: Exiting.\n",
      tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
      tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
      (int)cur_tv.tv_usec, lgr_sev2str(LGR_SEV_FYI));

    fclose(lgr->cur_out_fp);
    lgr->cur_out_fp = NULL;
  }

  CPRT_THREAD_EXIT;
  return 0;
}  /* lgr_thread */
