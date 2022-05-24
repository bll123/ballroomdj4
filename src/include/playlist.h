#ifndef INC_PLAYLIST_H
#define INC_PLAYLIST_H

#include <stdbool.h>

#include "dancesel.h"
#include "datafile.h"
#include "ilist.h"
#include "musicdb.h"
#include "nlist.h"
#include "sequence.h"
#include "slist.h"
#include "song.h"
#include "songfilter.h"
#include "songlist.h"
#include "songsel.h"

typedef enum {
  PLAYLIST_ALLOWED_KEYWORDS,      //
  PLAYLIST_ANNOUNCE,              //
  PLAYLIST_GAP,                   //
  PLAYLIST_LEVEL_HIGH,            //
  PLAYLIST_LEVEL_LOW,             //
  PLAYLIST_MAX_PLAY_TIME,         //
  PLAYLIST_RATING,                //
  PLAYLIST_STOP_AFTER,            //
  PLAYLIST_STOP_TIME,             //
  PLAYLIST_TYPE,                  //
  PLAYLIST_KEY_MAX,
} playlistkey_t;

typedef enum {
  PLDANCE_DANCE,
  PLDANCE_BPM_LOW,                //
  PLDANCE_BPM_HIGH,               //
  PLDANCE_COUNT,
  PLDANCE_MAXPLAYTIME,            //
  PLDANCE_SELECTED,
  PLDANCE_KEY_MAX,
} pldancekey_t;

typedef enum {
  PLTYPE_NONE,
  PLTYPE_AUTO,
  PLTYPE_SONGLIST,
  PLTYPE_SEQUENCE,
  PLTYPE_ALL,
} pltype_t;

enum {
  PL_LIST_NORMAL = 1,   // excludes the special QueueDance playlist
  PL_LIST_ALL,
  PL_LIST_SONGLIST,
  PL_LIST_SEQUENCE,
};

typedef struct playlist playlist_t;

#define VALID_SONG_ATTEMPTS   40

typedef bool (*playlistCheck_t)(song_t *, void *);

playlist_t *playlistAlloc (musicdb_t *musicdb);
int       playlistLoad (playlist_t *pl, const char *);
void      playlistCreate (playlist_t *pl, const char *plfname, pltype_t type,
    const char *suppfname);
void      playlistFree (void *);
char      *playlistGetName (playlist_t *pl);
ssize_t   playlistGetConfigNum (playlist_t *pl, playlistkey_t key);
void      playlistSetConfigNum (playlist_t *pl, playlistkey_t key, ssize_t value);
void      playlistSetConfigList (playlist_t *pl, playlistkey_t key, const char *value);
ssize_t   playlistGetDanceNum (playlist_t *pl, ilistidx_t dancekey, pldancekey_t key);
void      playlistSetDanceCount (playlist_t *pl, ilistidx_t dancekey, ssize_t value);
void      playlistSetDanceNum (playlist_t *pl, ilistidx_t danceIdx, pldancekey_t key, ssize_t value);
song_t    *playlistGetNextSong (playlist_t *pl, nlist_t *danceCounts,
    ssize_t priorCount, playlistCheck_t checkProc,
    danceselHistory_t historyProc, void *userdata);
slist_t   *playlistGetPlaylistList (int flag);
bool      playlistFilterSong (dbidx_t dbidx, song_t *song, void *tplaylist);
void      playlistAddCount (playlist_t *, song_t *song);
void      playlistAddPlayed (playlist_t *, song_t *song);
void      playlistSave (playlist_t *, const char *name);

#endif /* INC_PLAYLIST_H */
