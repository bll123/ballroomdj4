#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>

#include "bdjmsg.h"
#include "log.h"
#include "msgparse.h"
#include "nlist.h"

mp_musicqupdate_t *
msgparseMusicQueueData (char *args)
{
  int               mqidx;
  char              *p;
  char              *tokstr;
  int               idx;
  mp_musicqupditem_t   *musicqupditem;
  mp_musicqupdate_t    *musicqupdate;


  logProcBegin (LOG_PROC, "msgparseMusicQueueDataParse");

  musicqupdate = malloc (sizeof (mp_musicqupdate_t));

  /* first, build ourselves a list to work with */
  musicqupdate->dispList = nlistAlloc ("temp-musicq-disp", LIST_ORDERED, NULL);

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  mqidx = atoi (p);
  musicqupdate->mqidx = mqidx;

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  /* queue duration */
  musicqupdate->tottime = atol (p);

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  idx = 1;
  while (p != NULL) {
    musicqupditem = malloc (sizeof (mp_musicqupditem_t));
    assert (musicqupditem != NULL);

    musicqupditem->dispidx = atoi (p);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    if (p != NULL) {
      musicqupditem->uniqueidx = atoi (p);
    }
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    if (p != NULL) {
      musicqupditem->dbidx = atol (p);
    }
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    if (p != NULL) {
      musicqupditem->pflag = atoi (p);
    }

    nlistSetData (musicqupdate->dispList, idx, musicqupditem);

    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    ++idx;
  }

  logProcEnd (LOG_PROC, "msgparseMusicQueueDataParse", "");
  return musicqupdate;
}

void
msgparseMusicQueueDataFree (mp_musicqupdate_t *musicqupdate)
{
  if (musicqupdate != NULL) {
    if (musicqupdate->dispList != NULL) {
      nlistFree (musicqupdate->dispList);
      musicqupdate->dispList = NULL;
    }
    free (musicqupdate);
  }
}

mp_songselect_t *
msgparseSongSelect (char *args)
{
  int               mqidx;
  char              *p;
  char              *tokstr;
  mp_songselect_t   *songselect;


  logProcBegin (LOG_PROC, "msgparseMusicQueueDataParse");

  songselect = malloc (sizeof (mp_songselect_t));

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  mqidx = atoi (p);
  songselect->mqidx = mqidx;

  p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  /* queue duration */
  songselect->loc = atol (p);

  return songselect;
}

void
msgparseSongSelectFree (mp_songselect_t *songselect)
{
  if (songselect != NULL) {
    free (songselect);
  }
}
