/* lgr_test.c - fast logger. */
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
  #include <time.h>
  #include <unistd.h>
  #include <sys/time.h>
#endif

/* Intercept CPRT_TIMEOFDAY */
#define gettimeofday tst_timeofday
#define cprt_timeofday tst_timeofday

#include "cprt.h"
#include "lgr.h"

/* Used https://www.epochconverter.com to get the following. */
#define may_17_2022_11_59_59 1652846399

#undef gettimeofday
#undef cprt_timeofday

time_t global_tv_sec = may_17_2022_11_59_59;
time_t global_tv_usec = 0;
int tst_timeofday(struct cprt_timeval *tv, void *unused_tz) 
{
  CPRT_SLEEP_MS(1);  /* For testing certain race conditions. */
  global_tv_usec++;
  tv->tv_sec = global_tv_sec;
  tv->tv_usec = global_tv_usec;
  return 0;
#if defined(_WIN32)
  return cprt_timeofday(tv, unused_tz);
#else
  return gettimeofday(tv, unused_tz);
#endif
}  /* tst_timeofday */


int is_file_readable(char *fname)
{
  FILE *fp;
  fp = fopen(fname, "r");
  if (fp == NULL) {
    return 0;
  }
  fclose(fp);
  return 1;
}  /* is_file_readable */


char *ten_chars = "1234567890";

int main(int argc, char **argv)
{
  lgr_t *lgr;
  int i, bytes;
  int found;

  /* Force localtime_r() to EDT so that the tests can be run anywhere. */
  setenv("TZ", ":America/New_York", 1);
  tzset();

/*****************************************/
  fprintf(stderr, "Testing create..."); fflush(stdout);
  CPRT_ASSERT(lgr_create(&lgr,
      32,    /* max_msg_size */
      16,    /* q_size */
      1,     /* sleep_ms */
      0,     /* flags */
      "x.",  /* file_prefix */
      1)     /* max_file_size_mb */
    == LGR_ERR_OK);

  /* Find "starting" in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._tue -s '2022/05/17 23:59:59.000001 FYI lgr: Starting.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing log..."); fflush(stdout);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "testing log %d", 1) == LGR_ERR_OK);
  /* Make sure timestamp isn't deferred (i.e. GTOD should already be called). */
  CPRT_ASSERT(global_tv_usec == 3);

  /* Find message in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._tue -s '2022/05/17 23:59:59.000003 FYI testing log 1'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  CPRT_ASSERT(! is_file_readable("x._wed"));

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing day rollover..."); fflush(stdout);

  global_tv_sec = may_17_2022_11_59_59 + 1;
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ATTN, "after midnight") == LGR_ERR_OK);

  /* Make sure tuesday's file ends correctly. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._tue -s '2022/05/18 00:00:00.000005 FYI lgr: Closing file.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000004 ATTN after midnight'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing long line..."); fflush(stdout);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_WARN, "12345678901234567890123456789012") == LGR_ERR_OK);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000007 WARN 12345678901234567890123456789012'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ERR, "123456789012345678901234567890123") == LGR_ERR_OK);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000008 ERR 12345678901234567890123456789012...(message truncated)'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing delete..."); fflush(stdout);
  CPRT_ASSERT(lgr_delete(lgr) == LGR_ERR_OK);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000009 FYI lgr: Exiting.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
/* The 10 ms sleep is to force overflows. */

  fprintf(stderr, "Testing DEFER_TS..."); fflush(stdout);
  CPRT_ASSERT(lgr_create(&lgr,
      32,    /* max_msg_size */
      16,    /* q_size */
      10,    /* sleep_ms */
      LGR_FLAGS_DEFER_TS | LGR_FLAGS_NOLOCK,
      "x.",  /* file_prefix */
      1)     /* max_file_size_mb */
    == LGR_ERR_OK);

  /* Find "starting" in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000010 FYI lgr: Starting.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FATAL, "testing log %d", 2) == LGR_ERR_OK);
  /* Make sure timestamp is deferred (i.e. GTOD shouldn't already be called).
   * (The "Opening file." message is 11.) */
  CPRT_ASSERT(global_tv_usec == 11);

  /* Find message in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000012 FATAL testing log 2'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing overflow..."); fflush(stdout);

  /* Since the previous chk_log.sh waited for its log to appear, the logge
   * thread should now be inside its 10 ms sleep. Fill the log queue. */
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow1") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow2") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow3") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow4") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow5") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow6") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow7") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow8") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflow9") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowA") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowB") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowC") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowD") == LGR_ERR_OK);
  /* Remember that 3 fewer. */
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowE") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowF") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowG") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "overflowH") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ERR, "overflowI") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ERR, "overflowJ") == LGR_ERR_QFULL);

  /* Find message in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000013 ERR lgr: Overflow, FYI:4, ATTN:0, WARN:0, ERR:2, FATAL:0 logs dropped over 0.000014 sec'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  /* Do it again to make sure the oveflowing resets itself. */
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow1") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow2") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow3") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow4") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow5") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow6") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow7") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow8") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "Overflow9") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowA") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowB") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowC") == LGR_ERR_OK);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowD") == LGR_ERR_OK);
  /* Remember that 3 fewer. */
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowE") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowF") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowG") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "OverflowH") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ERR, "OverflowI") == LGR_ERR_QFULL);
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_ERR, "OverflowJ") == LGR_ERR_QFULL);

  /* Find message in log file. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.000028 ERR lgr: Overflow, FYI:4, ATTN:0, WARN:0, ERR:2, FATAL:0 logs dropped over 0.000014 sec'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);


/*****************************************/
  fprintf(stderr, "Testing size limit..."); fflush(stdout);

  for (bytes = 1439 + 65; bytes < (1024*1024); bytes += 65) {
    while (q_is_empty(lgr->pool_q)) { };  /* Spin while the log q is full. */
    CPRT_ASSERT(lgr_log(lgr, LGR_SEV_WARN, "123456789012345678901234 %07d", bytes) == LGR_ERR_OK);
  }
  bytes -= 65;  /* Back up to the last used value. */

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.016151 WARN 123456789012345678901234 1048524'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  CPRT_ASSERT(bytes == 1048524);
  CPRT_ASSERT(bytes == (int)lgr->cur_file_size_bytes);

  /* This one succeeds but exceeds the file size. */
  bytes += 65;
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_WARN, "Last successful 78901234 %07d", bytes) == LGR_ERR_OK);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.016152 WARN Last successful 78901234 1048589'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  /* This one returns success but does not write. */
  CPRT_ASSERT(lgr_log(lgr, LGR_SEV_WARN, "Not written 345678901234 %07d", i) == LGR_ERR_OK);

  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.016154 WARN lgr: Log file size exceeded.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing delete again..."); fflush(stdout);
  CPRT_ASSERT(lgr_delete(lgr) == LGR_ERR_OK);

  /* For file size exceeded, the close message is not written. */
  found = 0;
  CPRT_SLEEP_MS(1);
  for (i = 1; i < 5000; i+=50) {
    if (system("./chk_log.sh -f x._wed -s '2022/05/18 00:00:00.016154 WARN lgr: Log file size exceeded.'") == 0) {
      found = 1;
      break;
    }
    else { CPRT_SLEEP_MS(50); }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);


  fprintf(stderr, "All tests completed successfully.\n");

  return 0;
}  /* main */
