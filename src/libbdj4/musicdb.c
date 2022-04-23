#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "dance.h"
#include "fileop.h"
#include "nlist.h"
#include "slist.h"
#include "lock.h"
#include "log.h"
#include "musicdb.h"
#include "rafile.h"
#include "song.h"
#include "songutil.h"
#include "tagdef.h"

musicdb_t *
dbOpen (char *fn)
{
  dance_t       *dances;
  int32_t       dcount;
  musicdb_t     *musicdb;

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  dcount = danceGetCount (dances);

  musicdb = malloc (sizeof (musicdb_t));
  assert (musicdb != NULL);

  musicdb->songs = slistAlloc ("db-songs", LIST_UNORDERED, songFree);
  musicdb->danceCounts = nlistAlloc ("db-dance-counts", LIST_ORDERED, NULL);
  nlistSetSize (musicdb->danceCounts, dcount);
  musicdb->danceCount = dcount;
  musicdb->count = 0L;
  musicdb->radb = NULL;
  musicdb->fn = strdup (fn);
  dbLoad (musicdb);

  return musicdb;
}

void
dbClose (musicdb_t *musicdb)
{
  /* for each song in db, free the song */
  if (musicdb != NULL) {
    if (musicdb->songs != NULL) {
      slistFree (musicdb->songs);
    }
    if (musicdb->danceCounts != NULL) {
      nlistFree (musicdb->danceCounts);
    }
    if (musicdb->fn != NULL) {
      free (musicdb->fn);
    }
    if (musicdb->radb != NULL) {
      raClose (musicdb->radb);
    }
    free (musicdb);
  }
  musicdb = NULL;
}

dbidx_t
dbCount (musicdb_t *musicdb)
{
  dbidx_t tcount = 0L;
  if (musicdb != NULL) {
    tcount = musicdb->count;
  }
  return tcount;
}

int
dbLoad (musicdb_t *musicdb)
{
  char        data [RAFILE_REC_SIZE];
  char        *fstr;
  char        *ffn;
  song_t      *song;
  rafileidx_t srrn;
  rafileidx_t rc;
  nlistidx_t  dkey;
  nlistidx_t  iteridx;
  slistidx_t  dbidx;
  slistidx_t  siteridx;
  bool        ok;


  fstr = "";
  musicdb->radb = raOpen (musicdb->fn, MUSICDB_VERSION);
  slistSetSize (musicdb->songs, raGetCount (musicdb->radb));

  raStartBatch (musicdb->radb);

  for (rafileidx_t i = 1L; i <= raGetCount (musicdb->radb); ++i) {
    rc = raRead (musicdb->radb, i, data);
    if (rc != 1) {
      logMsg (LOG_DBG, LOG_IMPORTANT, "ERR: Unable to access rrn %zd", i);
    }
    if (rc == 0 || ! *data) {
      continue;
    }

    song = songAlloc ();
    songParse (song, data, i);
    fstr = songGetStr (song, TAG_FILE);
    ffn = songFullFileName (fstr);
    ok = false;
    if (fileopFileExists (ffn)) {
      ok = true;
    }
    free (ffn);

    if (! ok) {
      logMsg (LOG_DBG, LOG_IMPORTANT, "song %s not found", fstr);
    }

    if (ok) {
      srrn = songGetNum (song, TAG_RRN);
      dkey = songGetNum (song, TAG_DANCE);
      if (dkey >= 0) {
        nlistIncrement (musicdb->danceCounts, dkey);
      }
      if (i != srrn) {
        /* a double check to make sure the song has the correct rrn */
        songSetNum (song, TAG_RRN, i);
      }
      slistSetData (musicdb->songs, fstr, song);
    }
    ++musicdb->count;
  }
  slistSort (musicdb->songs);

  /* set the database index according to the sorted values */
  dbidx = 0;
  slistStartIterator (musicdb->songs, &siteridx);
  while ((song = slistIterateValueData (musicdb->songs, &siteridx)) != NULL) {
    songSetNum (song, TAG_DBIDX, dbidx);
    ++dbidx;
  }

  nlistStartIterator (musicdb->danceCounts, &iteridx);
  while ((dkey = nlistIterateKey (musicdb->danceCounts, &iteridx)) >= 0) {
    dbidx_t count = nlistGetNum (musicdb->danceCounts, dkey);
    if (count > 0) {
      logMsg (LOG_DBG, LOG_BASIC, "db-load: dance: %zd count: %ld", dkey, count);
    }
  }

  raEndBatch (musicdb->radb);
  raClose (musicdb->radb);
  musicdb->radb = NULL;
  return 0;
}

void
dbStartBatch (musicdb_t *musicdb)
{
  if (musicdb->radb == NULL) {
    musicdb->radb = raOpen (musicdb->fn, MUSICDB_VERSION);
    raStartBatch (musicdb->radb);
  }
}

void
dbEndBatch (musicdb_t *musicdb)
{
  if (musicdb->radb != NULL) {
    raEndBatch (musicdb->radb);
    raClose (musicdb->radb);
  }
  musicdb->radb = NULL;
}

song_t *
dbGetByName (musicdb_t *musicdb, char *songname)
{
  song_t *song = slistGetData (musicdb->songs, songname);
  return song;
}

song_t *
dbGetByIdx (musicdb_t *musicdb, dbidx_t idx)
{
  song_t  *song;

  if (idx < 0) {
    return NULL;
  }

  song = slistGetDataByIdx (musicdb->songs, idx);
  return song;
}

void
dbWrite (musicdb_t *musicdb, char *fn, slist_t *tagList)
{
  slistidx_t    iteridx;
  char          *tag;
  char          *data;
  char          tbuff [RAFILE_REC_SIZE];
  char          tmp [40];
  dbidx_t       rrn;

  if (musicdb == NULL) {
    return;
  }
  if (musicdb->radb == NULL) {
    return;
  }

  snprintf (tbuff, sizeof (tbuff), "FILE\n..%s\n", fn);
  rrn = raGetNextRRN (musicdb->radb);
  slistStartIterator (tagList, &iteridx);
  while ((tag = slistIterateKey (tagList, &iteridx)) != NULL) {
    if (strcmp (tag, "FILE") == 0) {
      return;
    }
    data = slistGetStr (tagList, tag);
    strlcat (tbuff, tag, sizeof (tbuff));
    strlcat (tbuff, "\n", sizeof (tbuff));
    strlcat (tbuff, "..", sizeof (tbuff));
    strlcat (tbuff, data, sizeof (tbuff));
    strlcat (tbuff, "\n", sizeof (tbuff));
  }
  strlcat (tbuff, "RRN", sizeof (tbuff));
  strlcat (tbuff, "\n", sizeof (tbuff));
  strlcat (tbuff, "..", sizeof (tbuff));
  snprintf (tmp, sizeof (tmp), "%d", rrn);
  strlcat (tbuff, tmp, sizeof (tbuff));
  strlcat (tbuff, "\n", sizeof (tbuff));
  raWrite (musicdb->radb, RAFILE_NEW, tbuff);
}

void
dbStartIterator (musicdb_t *musicdb, slistidx_t *iteridx)
{
  slistStartIterator (musicdb->songs, iteridx);
}

song_t *
dbIterate (musicdb_t *musicdb, dbidx_t *idx, slistidx_t *iteridx)
{
  song_t    *song;

  song = slistIterateValueData (musicdb->songs, iteridx);
  *idx = slistIterateGetIdx (musicdb->songs, iteridx);
  return song;
}

nlist_t *
dbGetDanceCounts (musicdb_t *musicdb)
{
  return musicdb->danceCounts;
}
