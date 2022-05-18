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
time_t global_tv_usec = 1;
int tst_timeofday(struct cprt_timeval *tv, void *unused_tz) 
{
  tv->tv_sec = global_tv_sec;
  tv->tv_usec = global_tv_usec;
  global_tv_usec++;
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
  int i;
  int found;

  setenv("TZ", ":America/New_York", 1);
  tzset();

/*****************************************/
  fprintf(stderr, "Testing create..."); fflush(stdout);
  CPRT_ASSERT(lgr_create(&lgr, 1024, 4096, 1, 0, "x.", 1) == LGR_ERR_OK);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing log..."); fflush(stdout);
  CPRT_ASSERT(lgr_create(&lgr,
      1024,  /* max_msg_size */
      4096,  /* q_size */
      1,     /* sleep_ms */
      0,     /* flags */
      "x.",  /* file_prefix */
      1)     /* max_file_size_mb */
    == LGR_ERR_OK);
  lgr_log(lgr, LGR_SEV_FYI, "testing log %d", 1);

  found = 0;
  for (i = 0; (! found) && (i < 50); i++) {
    CPRT_SLEEP_MS(100);
    if (system("./chk_log.sh -f x._tue -s \"2022/05/17 23:59:59.000005 FYI testing log 1\"") == 0) {
      found = 1;
    }
  }
  fprintf(stderr, "%d ms...", i*100);
  CPRT_ASSERT(found);

  CPRT_ASSERT(is_file_readable("x._tue"));
  CPRT_ASSERT(! is_file_readable("x._wed"));

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing day rollover..."); fflush(stdout);

  global_tv_sec = may_17_2022_11_59_59 + 1;
  lgr_log(lgr, LGR_SEV_FYI, "after midnight");

  /* Make sure tuesday's file ends correctly. */
  found = 0;
  for (i = 0; (! found) && (i < 50); i++) {
    CPRT_SLEEP_MS(100);
    if (system("./chk_log.sh -f x._tue -s \"2022/05/18 00:00:00.000007 FYI lgr: Closing file.\"") == 0) {
      found = 1;
    }
  }
  fprintf(stderr, "%d ms...", i*100);
  CPRT_ASSERT(found);

  for (i = 0; i < 5000; i++) {
    CPRT_SLEEP_MS(1);
    if (is_file_readable("x._wed")) {
      break;
    }
  }
  fprintf(stderr, "%d ms...", i);
  CPRT_ASSERT(is_file_readable("x._wed"));

  found = 0;
  for (i = 0; (! found) && (i < 50); i++) {
    CPRT_SLEEP_MS(100);
    if (system("./chk_log.sh -f x._wed -s \"2022/05/18 00:00:00.000006 FYI after midnight\"") == 0) {
      found = 1;
    }
  }
  fprintf(stderr, "%d ms...", i*100);
  CPRT_ASSERT(found);

  fprintf(stderr, "OK.\n"); fflush(stdout);

/*****************************************/
  fprintf(stderr, "Testing delete..."); fflush(stdout);
  CPRT_ASSERT(lgr_delete(lgr) == LGR_ERR_OK);

  fprintf(stderr, "OK.\n"); fflush(stdout);

  return 0;
}  /* main */
