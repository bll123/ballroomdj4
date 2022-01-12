#ifndef INC_SONG
#define INC_SONG

#include "list.h"

typedef enum {
  SONG_ADJUST_NONE    = 0x0000,
  SONG_ADJUST_NORM    = 0x0001,
  SONG_ADJUST_TRIM    = 0x0002,
  SONG_ADJUST_SPEED   = 0x0004,
} songadjust_t;

typedef struct {
  list_t      *songInfo;
} song_t;

song_t *  songAlloc (void);
void      songFree (void *);
void      songParse (song_t *song, char *data);
char *    songGetData (song_t *, listidx_t);
ssize_t   songGetNum (song_t *, listidx_t);
double    songGetDouble (song_t *, listidx_t);
void      songSetNum (song_t *, listidx_t, ssize_t);
bool      songAudioFileExists (song_t *song);

#endif /* INC_SONG */
