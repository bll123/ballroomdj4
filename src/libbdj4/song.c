#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "bdj4.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "bdjstring.h"
#include "dance.h"
#include "datafile.h"
#include "fileop.h"
#include "genre.h"
#include "level.h"
#include "nlist.h"
#include "log.h"
#include "rating.h"
#include "song.h"
#include "songfav.h"
#include "songutil.h"
#include "slist.h"
#include "status.h"
#include "tagdef.h"

#include "orgutil.h"

typedef struct song {
  nlist_t      *songInfo;
} song_t;

/* must be sorted in ascii order */
static datafilekey_t songdfkeys [] = {
  { "ADJUSTFLAGS",          TAG_ADJUSTFLAGS,          VALUE_STR, NULL, -1 },
  { "AFMODTIME",            TAG_AFMODTIME,            VALUE_NUM, NULL, -1 },
  { "ALBUM",                TAG_ALBUM,                VALUE_STR, NULL, -1 },
  { "ALBUMARTIST",          TAG_ALBUMARTIST,          VALUE_STR, NULL, -1 },
  { "ARTIST",               TAG_ARTIST,               VALUE_STR, NULL, -1 },
  { "AUTOORGFLAG",          TAG_AUTOORGFLAG,          VALUE_NUM, NULL, -1 },
  { "BPM",                  TAG_BPM,                  VALUE_NUM, NULL, -1 },
  { "COMPOSER",             TAG_COMPOSER,             VALUE_STR, NULL, -1 },
  { "CONDUCTOR",            TAG_CONDUCTOR,            VALUE_STR, NULL, -1 },
  { "DANCE",                TAG_DANCE,                VALUE_NUM, danceConvDance, -1 },
  { "DANCELEVEL",           TAG_DANCELEVEL,           VALUE_STR, levelConv, -1 },
  { "DANCERATING",          TAG_DANCERATING,          VALUE_STR, ratingConv, -1 },
  { "DATE",                 TAG_DATE,                 VALUE_STR, NULL, -1 },
  { "DBADDDATE",            TAG_DBADDDATE,            VALUE_STR, NULL, -1 },
  { "DISC",                 TAG_DISCNUMBER,           VALUE_NUM, NULL, -1 },
  { "DISCTOTAL",            TAG_DISCTOTAL,            VALUE_NUM, NULL, -1 },
  { "DURATION",             TAG_DURATION,             VALUE_NUM, NULL, -1 },
  { "FAVORITE",             TAG_FAVORITE,             VALUE_NUM, songConvFavorite, -1 },
  { "FILE",                 TAG_FILE,                 VALUE_STR, NULL, -1 },
  { "GENRE",                TAG_GENRE,                VALUE_NUM, genreConv, -1 },
  { "KEYWORD",              TAG_KEYWORD,              VALUE_STR, NULL, -1 },
  { "MQDISPLAY",            TAG_MQDISPLAY,            VALUE_STR, NULL, -1 },
  { "NOTES",                TAG_NOTES,                VALUE_STR, NULL, -1 },
  { "RECORDING_ID",         TAG_RECORDING_ID,         VALUE_STR, NULL, -1 },
  { "RRN",                  TAG_RRN,                  VALUE_NUM, NULL, -1 },
  { "SAMESONG",             TAG_SAMESONG,             VALUE_STR, NULL, -1 },
  { "SONGEND",              TAG_SONGEND,              VALUE_NUM, NULL, -1 },
  { "SONGSTART",            TAG_SONGSTART,            VALUE_NUM, NULL, -1 },
  { "SPEEDADJUSTMENT",      TAG_SPEEDADJUSTMENT,      VALUE_NUM, NULL, -1 },
  { "STATUS",               TAG_STATUS,               VALUE_NUM, statusConv, -1 },
  { "TAGS",                 TAG_TAGS,                 VALUE_LIST, convTextList, -1 },
  { "TITLE",                TAG_TITLE,                VALUE_STR, NULL, -1 },
  { "TRACKNUMBER",          TAG_TRACKNUMBER,          VALUE_NUM, NULL, -1 },
  { "TRACKTOTAL",           TAG_TRACKTOTAL,           VALUE_NUM, NULL, -1 },
  { "TRACK_ID",             TAG_TRACK_ID,             VALUE_STR, NULL, -1 },
  { "UPDATEFLAG",           TAG_UPDATEFLAG,           VALUE_NUM, NULL, -1 },
  { "UPDATETIME",           TAG_UPDATETIME,           VALUE_NUM, NULL, -1 },
  { "VOLUMEADJUSTPERC",     TAG_VOLUMEADJUSTPERC,     VALUE_DOUBLE, NULL, -1 },
  { "WORK_ID",              TAG_WORK_ID,              VALUE_STR, NULL, -1 },
  { "WRITETIME",            TAG_WRITETIME,            VALUE_NUM, NULL, -1 },
};
#define SONG_DFKEY_COUNT (sizeof (songdfkeys) / sizeof (datafilekey_t))

static songfavoriteinfo_t songfavoriteinfo [SONG_FAVORITE_MAX] = {
  { SONG_FAVORITE_NONE, "\xE2\x98\x86", "", NULL },
  { SONG_FAVORITE_RED, "\xE2\x98\x85", "#9b3128", NULL },
  { SONG_FAVORITE_ORANGE, "\xE2\x98\x85", "#c26a1a", NULL },
  { SONG_FAVORITE_GREEN, "\xE2\x98\x85", "#00b25d", NULL },
  { SONG_FAVORITE_BLUE, "\xE2\x98\x85", "#2f2ad7", NULL },
  { SONG_FAVORITE_PURPLE, "\xE2\x98\x85", "#901ba3", NULL },
};

typedef struct {
  size_t   songcount;
  level_t  *levels;
  status_t *status;
  rating_t *ratings;
} songinit_t;

static songinit_t *gsonginit = NULL;

static void songInit (void);
static void songCleanup (void);

song_t *
songAlloc (void)
{
  song_t    *song;

  songInit ();

  song = malloc (sizeof (song_t));
  assert (song != NULL);
  ++gsonginit->songcount;
  return song;
}

void
songFree (void *tsong)
{
  song_t  *song = (song_t *) tsong;
  if (song != NULL) {
    if (song->songInfo != NULL) {
      nlistFree (song->songInfo);
    }
    free (song);
    --gsonginit->songcount;
    if (gsonginit->songcount <= 0) {
      songCleanup ();
    }
  }
}

void
songParse (song_t *song, char *data, ssize_t didx)
{
  char    tbuff [100];
  ssize_t lkey;

  snprintf (tbuff, sizeof (tbuff), "song-%zd", didx);
  song->songInfo = datafileParse (data, tbuff, DFTYPE_KEY_VAL,
      songdfkeys, SONG_DFKEY_COUNT, DATAFILE_NO_LOOKUP, NULL);
  nlistSort (song->songInfo);

  /* check and set some defaults */
  lkey = nlistGetNum (song->songInfo, TAG_DANCELEVEL);
  if (lkey < 0) {
    lkey = levelGetDefaultKey (gsonginit->levels);
    nlistSetNum (song->songInfo, TAG_DANCELEVEL, lkey);
  }

  lkey = nlistGetNum (song->songInfo, TAG_STATUS);
  if (lkey < 0) {
    nlistSetNum (song->songInfo, TAG_STATUS, 0);
  }

  lkey = nlistGetNum (song->songInfo, TAG_DANCERATING);
  if (lkey < 0) {
    nlistSetNum (song->songInfo, TAG_DANCERATING, 0);
  }
}

char *
songGetStr (song_t *song, nlistidx_t idx)
{
  char    *value;

  if (song == NULL || song->songInfo == NULL) {
    return NULL;
  }

  value = nlistGetStr (song->songInfo, idx);
  return value;
}

ssize_t
songGetNum (song_t *song, nlistidx_t idx)
{
  ssize_t     value;

  if (song == NULL || song->songInfo == NULL) {
    return LIST_VALUE_INVALID;
  }

  value = nlistGetNum (song->songInfo, idx);
  return value;
}

double
songGetDouble (song_t *song, nlistidx_t idx)
{
  double      value;

  if (song == NULL || song->songInfo == NULL) {
    return LIST_DOUBLE_INVALID;
  }

  value = nlistGetDouble (song->songInfo, idx);
  return value;
}

slist_t *
songGetList (song_t *song, nlistidx_t idx)
{
  slist_t   *value;

  if (song == NULL || song->songInfo == NULL) {
    return NULL;
  }

  value = nlistGetList (song->songInfo, idx);
  return value;
}

songfavoriteinfo_t *
songGetFavoriteData (song_t *song)
{
  ssize_t       value;

  if (song == NULL || song->songInfo == NULL) {
    return &songfavoriteinfo [SONG_FAVORITE_NONE];
  }

  value = nlistGetNum (song->songInfo, TAG_FAVORITE);
  if (value == LIST_VALUE_INVALID) {
    value = SONG_FAVORITE_NONE;
  }
  return songGetFavorite (value);
}

songfavoriteinfo_t *
songGetFavorite (int value)
{
  return &songfavoriteinfo [value];
}

void
songSetNum (song_t *song, nlistidx_t tagidx, ssize_t value)
{
  if (song == NULL || song->songInfo == NULL) {
    return;
  }

  nlistSetNum (song->songInfo, tagidx, value);
}

void
songSetDouble (song_t *song, nlistidx_t tagidx, double value)
{
  if (song == NULL || song->songInfo == NULL) {
    return;
  }

  nlistSetDouble (song->songInfo, tagidx, value);
}

void
songSetStr (song_t *song, nlistidx_t tagidx, const char *str)
{
  if (song == NULL || song->songInfo == NULL) {
    return;
  }

  nlistSetStr (song->songInfo, tagidx, str);
}

void
songChangeFavorite (song_t *song)
{
  ssize_t fav = SONG_FAVORITE_NONE;

  if (song == NULL || song->songInfo == NULL) {
    return;
  }

  fav = nlistGetNum (song->songInfo, TAG_FAVORITE);
  if (fav == LIST_VALUE_INVALID) {
    fav = SONG_FAVORITE_NONE;
  }
  ++fav;
  if (fav >= SONG_FAVORITE_MAX) {
    fav = SONG_FAVORITE_NONE;
  }
  nlistSetNum (song->songInfo, TAG_FAVORITE, fav);
}

bool
songAudioFileExists (song_t *song)
{
  char      *sfname;
  char      *ffn;
  bool      exists = false;

  sfname = songGetStr (song, TAG_FILE);
  ffn = songFullFileName (sfname);
  exists = fileopFileExists (ffn);
  free (ffn);
  return exists;
}

char *
songDisplayString (song_t *song, int tagidx)
{
  valuetype_t     vt;
  dfConvFunc_t    convfunc;
  datafileconv_t  conv;
  char            *str;
  songfavoriteinfo_t  * favorite;

  if (tagidx == TAG_FAVORITE) {
    favorite = songGetFavoriteData (song);
    str = strdup (favorite->spanStr);
    return str;
  }

  vt = tagdefs [tagidx].valueType;
  convfunc = tagdefs [tagidx].convfunc;
  if (convfunc != NULL) {
    conv.allocated = false;
    if (vt == VALUE_NUM) {
      conv.num = songGetNum (song, tagidx);
    } else if (vt == VALUE_LIST) {
      conv.list = songGetList (song, tagidx);
    }
    conv.valuetype = vt;
    convfunc (&conv);
    if (conv.str == NULL) { conv.str = ""; }
    str = strdup (conv.str);
    if (conv.allocated) {
      free (conv.str);
    }
  } else {
    str = songGetStr (song, tagidx);
    if (str == NULL) { str = ""; }
    str = strdup (str);
  }

  return str;
}

slist_t *
songTagList (song_t *song)
{
  slist_t   *taglist;

  taglist = datafileSaveKeyValList ("song-tag", songdfkeys, SONG_DFKEY_COUNT, song->songInfo);
  return taglist;
}

/* internal routines */

static void
songInit (void)
{
  char  *col;
  char  tbuff [100];

  if (gsonginit != NULL) {
    return;
  }

  gsonginit = malloc (sizeof (songinit_t));
  assert (gsonginit != NULL);

  for (int i = 0; i < SONG_FAVORITE_MAX; ++i) {
    if (i == 0) {
      col = "#ffffff";
    } else {
      col = songfavoriteinfo [i].color;
    }
    snprintf (tbuff, sizeof (tbuff), "<span color=\"%s\">%s</span>",
        col, songfavoriteinfo [i].dispStr);
    songfavoriteinfo [i].spanStr = strdup (tbuff);
  }

  gsonginit->levels = bdjvarsdfGet (BDJVDF_LEVELS);
  gsonginit->status = bdjvarsdfGet (BDJVDF_STATUS);
  gsonginit->ratings = bdjvarsdfGet (BDJVDF_RATINGS);
  gsonginit->songcount = 0;
}

static void
songCleanup (void)
{
  if (! gsonginit) {
    return;
  }

  for (int i = 0; i < SONG_FAVORITE_MAX; ++i) {
    if (songfavoriteinfo [i].spanStr != NULL) {
      free (songfavoriteinfo [i].spanStr);
      songfavoriteinfo [i].spanStr = NULL;
    }
  }

  free (gsonginit);
  gsonginit = NULL;
}
