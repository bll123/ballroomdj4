#include "config.h"

#if __linux__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "bdjstring.h"
#include "osutils.h"

char *
osRegistryGet (char *key, char *name)
{
  return NULL;
}

char *
osGetSystemFont (const char *gsettingspath)
{
  char    *tptr;
  char    *rptr = NULL;

  tptr = osRunProgram (gsettingspath, "get",
      "org.gnome.desktop.interface",  "font-name", NULL);
  if (tptr != NULL) {
    /* gsettings puts quotes around the data */
    stringTrim (tptr);
    stringTrimChar (tptr, '\'');
    rptr = strdup (tptr + 1);
  }
  free (tptr);

  return rptr;
}

#endif
