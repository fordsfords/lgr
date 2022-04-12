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
  #include <unistd.h>
#endif

#include "cprt.h"
#include "lgr.h"


int main(int argc, char **argv)
{
  lgr_t *lgr;

  CPRT_ASSERT(lgr_create(&lgr, 1024, 4096, 1) == LGRERR_OK);

  lgr_log(lgr, LGRSEV_FYI, "msg_size=%d", 4096);

  CPRT_ASSERT(lgr_delete(lgr) == LGRERR_OK);

  return 0;
}  /* main */
