#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "filedata.h"
#include "fileop.h"

char *
filedataReadAll (const char *fname, size_t *rlen)
{
  FILE        *fh;
  ssize_t     len;
  char        *data = NULL;

  len = fileopSize (fname);
  if (len <= 0) {
    if (rlen != NULL) {
      *rlen = 0;
    }
    return NULL;
  }
  fh = fileopOpen (fname, "r");
  if (fh != NULL) {
    data = malloc (len + 1);
    assert (data != NULL);
    len = fread (data, 1, len, fh);
    data [len] = '\0';
    fclose (fh);
    if (rlen != NULL) {
      *rlen = len;
    }
  }

  return data;
}

