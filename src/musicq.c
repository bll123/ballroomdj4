#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "bdjvarsdf.h"
#include "dance.h"
#include "musicq.h"
#include "queue.h"
#include "tagdef.h"

static void musicqQueueFree (void *tq);

musicq_t *
musicqAlloc (void)
{
  musicq_t *musicq = malloc (sizeof (musicq_t));
  assert (musicq != NULL);
  musicq->q [MUSICQ_A] = queueAlloc (musicqQueueFree);
  musicq->q [MUSICQ_B] = queueAlloc (musicqQueueFree);
  return musicq;
}

void
musicqFree (musicq_t *musicq)
{
  if (musicq != NULL) {
    if (musicq->q [MUSICQ_A] != NULL) {
      queueFree (musicq->q [MUSICQ_A]);
    }
    if (musicq->q [MUSICQ_B] != NULL) {
      queueFree (musicq->q [MUSICQ_B]);
    }
    free (musicq);
  }
}

void
musicqQueueFree (void *titem)
{
  musicqitem_t        *musicqitem = titem;

  if (musicqitem != NULL) {
    if (musicqitem->playlistName != NULL) {
      free (musicqitem->playlistName);
    }
    if (musicqitem->announce != NULL) {
      free (musicqitem->announce);
    }
    free (musicqitem);
  }
}

void
musicqPush (musicq_t *musicq, musicqidx_t musicqidx, song_t *song, char *plname)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return;
  }

  musicqitem = malloc (sizeof (musicqitem_t));
  assert (musicqitem != NULL);
  musicqitem->song = song;
  musicqitem->playlistName = strdup (plname);
  musicqitem->announce = NULL;
  musicqitem->flags = MUSICQ_FLAG_NONE;
  queuePush (musicq->q [musicqidx], musicqitem);
}

song_t *
musicqGetCurrent (musicq_t *musicq, musicqidx_t musicqidx)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }

  musicqitem = queueGetCurrent (musicq->q [musicqidx]);
  if (musicqitem == NULL) {
    return NULL;
  }
  return musicqitem->song;
}

song_t *
musicqGetByIdx (musicq_t *musicq, musicqidx_t musicqidx, ssize_t qkey)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    return musicqitem->song;
  }
  return NULL;
}

musicqflag_t
musicqGetFlags (musicq_t *musicq, musicqidx_t musicqidx, ssize_t qkey)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return MUSICQ_FLAG_NONE;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    return musicqitem->flags;
  }
  return MUSICQ_FLAG_NONE;
}

void
musicqSetFlags (musicq_t *musicq, musicqidx_t musicqidx,
    ssize_t qkey, musicqflag_t flags)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    musicqitem->flags |= flags;
  }
  return;
}

char *
musicqGetAnnounce (musicq_t *musicq, musicqidx_t musicqidx, ssize_t qkey)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    return musicqitem->announce;
  }
  return NULL;
}

void
musicqSetAnnounce (musicq_t *musicq, musicqidx_t musicqidx,
    ssize_t qkey, char *annfname)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    musicqitem->announce = strdup (annfname);
  }
  return;
}

char *
musicqGetPlaylistName (musicq_t *musicq, musicqidx_t musicqidx, ssize_t qkey)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], qkey);
  if (musicqitem != NULL) {
    return musicqitem->playlistName;
  }
  return NULL;
}

void
musicqPop (musicq_t *musicq, musicqidx_t musicqidx)
{
  musicqitem_t      *musicqitem;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return;
  }

  musicqitem = queuePop (musicq->q [musicqidx]);
  musicqQueueFree (musicqitem);
}

ssize_t
musicqGetLen (musicq_t *musicq, musicqidx_t musicqidx)
{
  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return 0;
  }

  return queueGetCount (musicq->q [musicqidx]);
}


char *
musicqGetData (musicq_t *musicq, musicqidx_t musicqidx, ssize_t idx, tagdefkey_t tagidx)
{
  musicqitem_t  *musicqitem;
  song_t        *song;
  char          *data;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }
  if (idx >= queueGetCount (musicq->q [musicqidx])) {
    return NULL;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], idx);
  if (musicqitem != NULL) {
    song = musicqitem->song;
    data = songGetData (song, tagidx);
  }
  return data;
}

char *
musicqGetDance (musicq_t *musicq, musicqidx_t musicqidx, ssize_t idx)
{
  musicqitem_t  *musicqitem;
  song_t        *song;
  listidx_t     dancekey;
  char          *danceStr;
  dance_t       *dances;

  if (musicq == NULL || musicq->q [musicqidx] == NULL) {
    return NULL;
  }
  if (idx >= queueGetCount (musicq->q [musicqidx])) {
    return NULL;
  }

  musicqitem = queueGetByIdx (musicq->q [musicqidx], idx);
  if (musicqitem != NULL) {
    song = musicqitem->song;
    dancekey = songGetNum (song, TAG_DANCE);
    dances = bdjvarsdf [BDJVDF_DANCES];
    danceStr = danceGetData (dances, dancekey, DANCE_DANCE);
  }
  return danceStr;
}

