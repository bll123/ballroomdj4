#ifndef INC_UISONGFILTER_H
#define INC_UISONGFILTER_H

#include "nlist.h"
#include "songfilter.h"
#include "uiutils.h"

typedef struct uisongfilter uisongfilter_t;

/* uisongfilter.c */
uisongfilter_t * uisfInit (UIWidget *windowp, nlist_t *options, songfilterpb_t pbflag);
void uisfFree (uisongfilter_t *uisf);
void uisfSetApplyCallback (uisongfilter_t *uisf, UICallback *applycb);
void uisfSetDanceSelectCallback (uisongfilter_t *uisf, UICallback *danceselcb);
void uisfShowPlaylistDisplay (uisongfilter_t *uisf);
void uisfHidePlaylistDisplay (uisongfilter_t *uisf);
bool uisfPlaylistInUse (uisongfilter_t *uisf);
bool uisfDialog (void *udata);
void uisfSetPlaylist (uisongfilter_t *uisf, char *slname);
void uisfClearPlaylist (uisongfilter_t *uisf);
void uisfSetDanceIdx (uisongfilter_t *uisf, int danceIdx);
songfilter_t *uisfGetSongFilter (uisongfilter_t *uisf);

#endif /* INC_UISONGFILTER_H */
