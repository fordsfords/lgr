/* lgr_perf.c - fast logger. */

/* This work is dedicated to the public domain under CC0 1.0 Universal:
 * http://creativecommons.org/publicdomain/zero/1.0/
 * 
 * To the extent possible under law, Steven Ford has waived all copyright
 * and related or neighboring rights to this work. In other words, you can 
 * use this code for any purpose without any restrictions.
 * This work is published from: United States.
 * Project home: https://github.com/fordsfords/lgr
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

#include "cprt.h"
#include "lgr.h"


int main(int argc, char **argv)
{
  lgr_t *lgr;
  int i;

  CPRT_ASSERT(lgr_create(&lgr,
      32,    /* max_msg_size */
      4096,  /* q_size */
      1,     /* sleep_ms */
      0,     /* flags */
      "x.",  /* file_prefix */
      1)     /* max_file_size_mb */
    == LGR_ERR_OK);

  for (i = 0; i < 4001; i++) {
    CPRT_ASSERT(lgr_log(lgr, LGR_SEV_FYI, "testing log") == LGR_ERR_OK);
  }

  CPRT_SLEEP_MS(100);
  printf("%s\n", lgr->file_full_name);

  CPRT_ASSERT(lgr_delete(lgr) == LGR_ERR_OK);

  return 0;
}  /* main */
