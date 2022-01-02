#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dncspeeds.h"
#include "datafile.h"
#include "fileop.h"

datafile_t *
dncspeedsAlloc (char *fname)
{
  datafile_t    *df;

  if (! fileopExists (fname)) {
    return NULL;
  }
  df = datafileAllocParse ("dance-speeds", DFTYPE_LIST, fname, NULL, 0);
  return df;
}

void
dncspeedsFree (datafile_t *df)
{
  datafileFree (df);
}
