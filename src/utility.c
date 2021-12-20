#include "bdjconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#if _hdr_unistd
# include <unistd.h>
#endif

#include "utility.h"

void
msleep (size_t mt)
{
  size_t t = mt * 1000;
  usleep ((useconds_t) t);
}

void
mtimestart (mtime_t *t)
{
  gettimeofday (&t->start, NULL);
}

size_t
mtimeend (mtime_t *t)
{
  struct timeval    end;

  gettimeofday (&end, NULL);
  long s = end.tv_sec - t->start.tv_sec;
  long u = end.tv_usec - t->start.tv_usec;
  long m = s * 1000 + u / 1000;
  return (size_t) m;
}

