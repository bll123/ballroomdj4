#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "dance.h"
#include "filemanip.h"
#include "log.h"
#include "manageui.h"
#include "pathbld.h"
#include "playlist.h"
#include "ui.h"
#include "uiduallist.h"
#include "uiselectfile.h"

enum {
  MSEQ_MENU_CB_SEQ_LOAD,
  MSEQ_MENU_CB_SEQ_COPY,
  MSEQ_MENU_CB_SEQ_NEW,
  MSEQ_MENU_CB_SEQ_DELETE,
  MSEQ_CB_SEQ_LOAD,
  MSEQ_CB_MAX,
};

typedef struct manageseq {
  UIWidget        *windowp;
  nlist_t         *options;
  uimenu_t        seqmenu;
  UICallback      callback [MSEQ_CB_MAX];
  uiduallist_t    *seqduallist;
  uientry_t       *seqname;
  UIWidget        *statusMsg;
  char            *seqoldname;
  bool            seqbackupcreated : 1;
  bool            changed : 1;
  bool            inload : 1;
} manageseq_t;

static bool   manageSequenceLoad (void *udata);
static bool   manageSequenceCopy (void *udata);
static void   manageSequenceLoadFile (void *udata, const char *fn);
static bool   manageSequenceNew (void *udata);
static bool   manageSequenceDelete (void *udata);
static void   manageSetSequenceName (manageseq_t *manageseq, const char *nm);

manageseq_t *
manageSequenceAlloc (UIWidget *window, nlist_t *options, UIWidget *statusMsg)
{
  manageseq_t *manageseq;

  manageseq = malloc (sizeof (manageseq_t));
  manageseq->seqduallist = NULL;
  manageseq->seqoldname = NULL;
  manageseq->seqbackupcreated = false;
  uiMenuInit (&manageseq->seqmenu);
  manageseq->seqname = uiEntryInit (20, 50);
  manageseq->statusMsg = statusMsg;
  manageseq->windowp = window;
  manageseq->options = options;
  manageseq->changed = false;
  manageseq->inload = false;

  return manageseq;
}

void
manageSequenceFree (manageseq_t *manageseq)
{
  if (manageseq != NULL) {
    if (manageseq->seqduallist != NULL) {
      uiduallistFree (manageseq->seqduallist);
    }
    if (manageseq->seqoldname != NULL) {
      free (manageseq->seqoldname);
    }
    uiEntryFree (manageseq->seqname);
    free (manageseq);
  }
}

void
manageSequenceSetLoadCallback (manageseq_t *manageseq, UICallback *uicb)
{
  if (manageseq == NULL) {
    return;
  }
  memcpy (&manageseq->callback [MSEQ_CB_SEQ_LOAD], uicb, sizeof (UICallback));
}

void
manageBuildUISequence (manageseq_t *manageseq, UIWidget *vboxp)
{
  UIWidget            hbox;
  UIWidget            uiwidget;
  dance_t             *dances;
  slist_t             *dancelist;

  uiutilsUIWidgetInit (&hbox);
  uiutilsUIWidgetInit (&uiwidget);

  /* edit sequences */
  uiWidgetSetAllMargins (vboxp, uiBaseMarginSz * 2);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (vboxp, &hbox);

  /* CONTEXT: sequence editor: label for sequence name */
  uiCreateColonLabel (&uiwidget, _("Sequence"));
  uiBoxPackStart (&hbox, &uiwidget);

  uiEntryCreate (manageseq->seqname);
  uiEntrySetValidate (manageseq->seqname, manageValidateName,
      manageseq->statusMsg, false);
  uiEntrySetColor (manageseq->seqname, bdjoptGetStr (OPT_P_UI_ACCENT_COL));
  /* CONTEXT: sequence editor: default name for a new sequence */
  manageSetSequenceName (manageseq, _("New Sequence"));
  uiBoxPackStart (&hbox, uiEntryGetUIWidget (manageseq->seqname));

  manageseq->seqduallist = uiCreateDualList (vboxp,
      DUALLIST_FLAGS_MULTIPLE | DUALLIST_FLAGS_PERSISTENT,
      /* CONTEXT: sequence editor: titles for the selection list and the sequence list  */
      _("Dance"), _("Sequence"));

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  dancelist = danceGetDanceList (dances);
  uiduallistSet (manageseq->seqduallist, dancelist, DUALLIST_TREE_SOURCE);
}

uimenu_t *
manageSequenceMenu (manageseq_t *manageseq, UIWidget *uimenubar)
{
  UIWidget  menu;
  UIWidget  menuitem;

  if (! manageseq->seqmenu.initialized) {
    uiMenuAddMainItem (uimenubar, &menuitem,
        /* CONTEXT: sequence editor: menu selection: sequence: edit menu */
        &manageseq->seqmenu, _("Edit"));

    uiCreateSubMenu (&menuitem, &menu);

    uiutilsUICallbackInit (&manageseq->callback [MSEQ_MENU_CB_SEQ_LOAD],
        manageSequenceLoad, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: load */
    uiMenuCreateItem (&menu, &menuitem, _("Load"),
        &manageseq->callback [MSEQ_MENU_CB_SEQ_LOAD]);

    uiutilsUICallbackInit (&manageseq->callback [MSEQ_MENU_CB_SEQ_NEW],
        manageSequenceNew, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: start new sequence */
    uiMenuCreateItem (&menu, &menuitem, _("Start New Sequence"),
        &manageseq->callback [MSEQ_MENU_CB_SEQ_NEW]);

    uiutilsUICallbackInit (&manageseq->callback [MSEQ_MENU_CB_SEQ_COPY],
        manageSequenceCopy, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: create copy */
    uiMenuCreateItem (&menu, &menuitem, _("Create Copy"),
        &manageseq->callback [MSEQ_MENU_CB_SEQ_COPY]);

    uiutilsUICallbackInit (&manageseq->callback [MSEQ_MENU_CB_SEQ_DELETE],
        manageSequenceDelete, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: delete sequence */
    uiMenuCreateItem (&menu, &menuitem, _("Delete"),
        &manageseq->callback [MSEQ_MENU_CB_SEQ_DELETE]);

    manageseq->seqmenu.initialized = true;
  }

  uiMenuDisplay (&manageseq->seqmenu);
  return &manageseq->seqmenu;
}

void
manageSequenceSave (manageseq_t *manageseq)
{
  sequence_t  *seq = NULL;
  slist_t     *slist;
  char        nnm [MAXPATHLEN];
  char        *name;
  bool        changed = false;

  if (manageseq->seqoldname == NULL) {
    return;
  }

  slist = uiduallistGetList (manageseq->seqduallist);
  if (slistGetCount (slist) <= 0) {
    slistFree (slist);
    return;
  }

  if (uiduallistIsChanged (manageseq->seqduallist)) {
    changed = true;
  }

  name = strdup (uiEntryGetValue (manageseq->seqname));

  /* the sequence has been renamed */
  if (strcmp (manageseq->seqoldname, name) != 0) {
    manageRenamePlaylistFiles (manageseq->seqoldname, name);
    changed = true;
  }

  if (! changed) {
    slistFree (slist);
    return;
  }

  /* need the full name for backups */
  pathbldMakePath (nnm, sizeof (nnm),
      name, BDJ4_SEQUENCE_EXT, PATHBLD_MP_DATA);
  if (! manageseq->seqbackupcreated) {
    filemanipBackup (nnm, 1);
    manageseq->seqbackupcreated = true;
  }

  manageSetSequenceName (manageseq, name);
  seq = sequenceCreate (name);
  sequenceSave (seq, slist);
  sequenceFree (seq);

  manageCheckAndCreatePlaylist (name, PLTYPE_SEQUENCE);
  slistFree (slist);
  free (name);
}

/* the current sequence may be renamed or deleted. */
/* check for this and if the sequence has */
/* disappeared, reset */
void
manageSequenceLoadCheck (manageseq_t *manageseq)
{
  const char  *name;

  if (manageseq->seqoldname == NULL) {
    return;
  }

  name = uiEntryGetValue (manageseq->seqname);

  if (! managePlaylistExists (name)) {
    /* make sure no save happens */
    manageseq->seqoldname = NULL;
    manageSequenceNew (manageseq);
  }
}

/* internal routines */

static bool
manageSequenceLoad (void *udata)
{
  manageseq_t  *manageseq = udata;

  logMsg (LOG_DBG, LOG_ACTIONS, "= action: load sequence");
  selectFileDialog (SELFILE_SEQUENCE, manageseq->windowp, manageseq->options,
      manageseq, manageSequenceLoadFile);
  return UICB_CONT;
}

static void
manageSequenceLoadFile (void *udata, const char *fn)
{
  manageseq_t *manageseq = udata;
  sequence_t  *seq = NULL;
  char        *dstr = NULL;
  nlist_t     *dancelist = NULL;
  slist_t     *tlist = NULL;
  nlistidx_t  iteridx;
  nlistidx_t  didx;

  if (manageseq->inload) {
    return;
  }

  logMsg (LOG_DBG, LOG_ACTIONS, "load sequence file");
  manageSequenceSave (manageseq);

  seq = sequenceAlloc (fn);
  if (seq == NULL) {
    return;
  }
  manageseq->inload = true;

  dancelist = sequenceGetDanceList (seq);
  tlist = slistAlloc ("temp-seq", LIST_UNORDERED, NULL);
  nlistStartIterator (dancelist, &iteridx);
  while ((didx = nlistIterateKey (dancelist, &iteridx)) >= 0) {
    dstr = nlistGetStr (dancelist, didx);
    slistSetNum (tlist, dstr, didx);
  }
  uiduallistSet (manageseq->seqduallist, tlist, DUALLIST_TREE_TARGET);
  uiduallistClearChanged (manageseq->seqduallist);
  slistFree (tlist);

  manageSetSequenceName (manageseq, fn);

  uiutilsCallbackStrHandler (&manageseq->callback [MSEQ_CB_SEQ_LOAD], fn);

  manageseq->seqbackupcreated = false;
  manageseq->inload = false;
}

static bool
manageSequenceCopy (void *udata)
{
  manageseq_t *manageseq = udata;
  const char  *oname;
  char        newname [200];

  logMsg (LOG_DBG, LOG_ACTIONS, "= action: copy sequence");
  manageSequenceSave (manageseq);

  oname = uiEntryGetValue (manageseq->seqname);
  /* CONTEXT: sequence editor: the new name after 'create copy' (e.g. "Copy of DJ-2022-04") */
  snprintf (newname, sizeof (newname), _("Copy of %s"), oname);
  if (manageCreatePlaylistCopy (manageseq->statusMsg, oname, newname)) {
    manageSetSequenceName (manageseq, newname);
    manageseq->seqbackupcreated = false;
    uiduallistClearChanged (manageseq->seqduallist);
  }
  return UICB_CONT;
}

static bool
manageSequenceNew (void *udata)
{
  manageseq_t *manageseq = udata;
  char        tbuff [200];
  slist_t     *tlist;

  logMsg (LOG_DBG, LOG_ACTIONS, "= action: new sequence");
  manageSequenceSave (manageseq);

  /* CONTEXT: sequence editor: default name for a new sequence */
  snprintf (tbuff, sizeof (tbuff), _("New Sequence"));
  manageSetSequenceName (manageseq, tbuff);
  manageseq->seqbackupcreated = false;
  tlist = slistAlloc ("tmp-sequence", LIST_UNORDERED, NULL);
  uiduallistSet (manageseq->seqduallist, tlist, DUALLIST_TREE_TARGET);
  uiduallistClearChanged (manageseq->seqduallist);
  slistFree (tlist);
  return UICB_CONT;
}

static bool
manageSequenceDelete (void *udata)
{
  manageseq_t *manageseq = udata;
  const char  *oname;

  logMsg (LOG_DBG, LOG_ACTIONS, "= action: delete sequence");
  oname = uiEntryGetValue (manageseq->seqname);
  manageDeletePlaylist (manageseq->statusMsg, oname);
  uiduallistClearChanged (manageseq->seqduallist);
  manageSequenceNew (manageseq);
  return UICB_CONT;
}

static void
manageSetSequenceName (manageseq_t *manageseq, const char *name)
{
  if (manageseq->seqoldname != NULL) {
    free (manageseq->seqoldname);
  }
  manageseq->seqoldname = strdup (name);
  uiEntrySetValue (manageseq->seqname, name);
}
