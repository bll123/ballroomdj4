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

typedef struct {
  progstate_t       *progstate;
  conn_t            *conn;
  int               maxRows;
  int               lastTreeSize;
  double            lastRowHeight;
  ssize_t           idxStart;
  ilistidx_t        danceIdx;
  double            dfilterCount;
  songfilter_t      *songfilter;
  rating_t          *ratings;
  level_t           *levels;
  status_t          *status;
  nlist_t           *options;
  sortopt_t         *sortopt;
  dispsel_t         *dispsel;
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
  GtkWidget         *parentwin;
  GtkWidget         *vbox;
  GtkWidget         *songselTree;
  GtkWidget         *songselScrollbar;
  GtkEventController  *scrollController;
  GtkTreeViewColumn   * favColumn;
  GtkWidget         *filterDialog;
  /* internal flags */
  bool              createRowProcessFlag : 1;
  bool              createRowFlag : 1;
} uisongsel_t;

/* uisongsel.c */
uisongsel_t * uisongselInit (progstate_t *progstate, conn_t *conn,
                dispsel_t *dispsel, nlist_t *opts, songfilterpb_t filterFlags);
void        uisongselFree (uisongsel_t *uisongsel);
void        uisongselMainLoop (uisongsel_t *uisongsel);

/* uisongselgtk.c */
GtkWidget   * uisongselActivate (uisongsel_t *uisongsel, GtkWidget *parentwin);

#endif /* INC_UISONGSEL_H */
