#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "bdjvars.h"
#include "sysvars.h"
#include "bdjstring.h"
#include "bdjopt.h"

static void   bdjvarsAdjustPorts (void);

char *      bdjvars [BDJV_MAX];
ssize_t     bdjvarsl [BDJVL_MAX];

void
bdjvarsInit (void)
{
  bdjvarsl [BDJVL_MAIN_PORT] = 35548;
  bdjvarsl [BDJVL_PLAYER_PORT] = bdjvarsl [BDJVL_MAIN_PORT] + 1;
  bdjvarsl [BDJVL_NUM_PORTS] = 2;

  bdjvarsAdjustPorts ();
}

void
bdjvarsCleanup (void)
{
  return;
}

char *
songFullFileName (char *sfname)
{
  char      *tname;

  tname = malloc (MAXPATHLEN);
  assert (tname != NULL);

  if (sfname [0] == '/' || (sfname [1] == ':' && sfname [2] == '/')) {
    strlcpy (tname, sfname, MAXPATHLEN);
  } else {
    snprintf (tname, MAXPATHLEN, "%s/%s",
        (char *) bdjoptGetData (OPT_M_DIR_MUSIC), sfname);
  }
  return tname;
}

/* internal routines */

static void
bdjvarsAdjustPorts (void)
{
  ssize_t idx = lsysvars [SVL_BDJIDX];

  bdjvarsl [BDJVL_MAIN_PORT] += bdjvarsl [BDJVL_NUM_PORTS] * idx;
  bdjvarsl [BDJVL_PLAYER_PORT] += bdjvarsl [BDJVL_NUM_PORTS] * idx;
}

