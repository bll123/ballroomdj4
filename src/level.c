#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "level.h"
#include "datafile.h"
#include "fileutil.h"

static datafilekey_t leveldfkeys[] = {
  { "default",  LEVEL_DEFAULT_FLAG, VALUE_LONG, parseConvBoolean },
  { "label",    LEVEL_LABEL,        VALUE_DATA, NULL },
  { "weight",   LEVEL_WEIGHT,       VALUE_LONG, NULL },
};
#define LEVEL_DFKEY_COUNT (sizeof (leveldfkeys) / sizeof (datafilekey_t))

datafile_t *
levelAlloc (char *fname)
{
  datafile_t    *df;

  if (! fileExists (fname)) {
    return NULL;
  }

  df = datafileAlloc (leveldfkeys, LEVEL_DFKEY_COUNT, fname, DFTYPE_KEY_LONG);
  return df;
}

void
levelFree (datafile_t *df)
{
  datafileFree (df);
}