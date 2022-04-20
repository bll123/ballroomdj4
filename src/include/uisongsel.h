#ifndef INC_UISONGSEL_H
#define INC_UISONGSEL_H

#include <stdbool.h>
#include <gtk/gtk.h>

#include "conn.h"
#include "dispsel.h"
#include "level.h"
#include "musicdb.h"
#include "nlist.h"
#include "progstate.h"
#include "rating.h"
#include "songfilter.h"
#include "sortopt.h"
#include "status.h"
#include "uiutils.h"

enum {
  FILTER_DISP_GENRE,
  FILTER_DISP_DANCELEVEL,
  FILTER_DISP_STATUS,
  FILTER_DISP_FAVORITE,
  FILTER_DISP_STATUSPLAYABLE,
  FILTER_DISP_MAX,
};

typedef struct {
  progstate_t       *progstate;
  conn_t            *conn;
  ssize_t           idxStart;
  ilistidx_t        danceIdx;
  songfilter_t      *songfilter;
  rating_t          *ratings;
  level_t           *levels;
  status_t          *status;
  nlist_t           *options;
  datafile_t        *filterDisplayDf;
  nlist_t           *filterDisplaySel;
  sortopt_t         *sortopt;
  dispsel_t         *dispsel;
  double            dfilterCount;
  /* filter data */
  uiutilsdropdown_t sortbysel;
  uiutilsdropdown_t filterdancesel;
  uiutilsdropdown_t filtergenresel;
  uiutilsentry_t    searchentry;
  uiutilsspinbox_t  filterratingsel;
  uiutilsspinbox_t  filterlevelsel;
  uiutilsspinbox_t  filterstatussel;
  uiutilsspinbox_t  filterfavoritesel;
  /* song selection tab */
  uiutilsdropdown_t dancesel;
  /* widget data */
  void              *uiWidgetData;
} uisongsel_t;

/* uisongsel.c */
uisongsel_t * uisongselInit (progstate_t *progstate, conn_t *conn,
    dispsel_t *dispsel, nlist_t *opts, songfilterpb_t filterFlags);
void  uisongselFree (uisongsel_t *uisongsel);
void  uisongselMainLoop (uisongsel_t *uisongsel);
void  uisongselFilterDanceProcess (uisongsel_t *uisongsel, ssize_t idx);
void  uisongselDanceSelect (uisongsel_t *uisongsel, ssize_t idx);
void  uisongselQueueProcess (uisongsel_t *uisongsel, dbidx_t dbidx);
void  uisongselChangeFavorite (uisongsel_t *uisongsel, dbidx_t dbidx);
void  uisongselApplySongFilter (uisongsel_t *uisongsel);
void  uisongselInitFilterDisplay (uisongsel_t *uisongsel);
char  * uisongselRatingGet (void *udata, int idx);
char  * uisongselLevelGet (void *udata, int idx);
char  * uisongselStatusGet (void *udata, int idx);
char  * uisongselFavoriteGet (void *udata, int idx);
void  uisongselSortBySelect (uisongsel_t *uisongsel, ssize_t idx);
void  uisongselCreateSortByList (uisongsel_t *uisongsel);
void  uisongselGenreSelect (uisongsel_t *uisongsel, ssize_t idx);
void  uisongselCreateGenreList (uisongsel_t *uisongsel);


/* uisongselgtk.c */
void      uisongselUIInit (uisongsel_t *uisongsel);
void      uisongselUIFree (uisongsel_t *uisongsel);
GtkWidget * uisongselActivate (uisongsel_t *uisongsel, GtkWidget *parentwin);
void      uisongselClearData (uisongsel_t *uisongsel);
void      uisongselPopulateData (uisongsel_t *uisongsel);
void      uisongselSetFavoriteForeground (uisongsel_t *uisongsel, char *color);

#endif /* INC_UISONGSEL_H */

