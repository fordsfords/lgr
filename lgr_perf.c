/* lgr_perf.c - fast logger. */
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
