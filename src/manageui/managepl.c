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

#include <gtk/gtk.h>

#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "filemanip.h"
#include "manageui.h"
#include "pathbld.h"
#include "playlist.h"
#include "ui.h"
#include "uilevel.h"
#include "uirating.h"
#include "uiselectfile.h"
#include "uiutils.h"

typedef struct managepl {
  UIWidget        *windowp;
  nlist_t         *options;
  UIWidget        *statusMsg;
  uimenu_t        plmenu;
  char            *ploldname;
  bool            plbackupcreated;
  uientry_t       plname;
  pltype_t        pltype;
  uispinbox_t     ratingspinbox;
  uirating_t      *uirating;
  UIWidget        uiratingitem;
  uilevel_t       *uilowlevel;
  UIWidget        uilowlevelitem;
  uilevel_t       *uihighlevel;
  UIWidget        uihighlevelitem;
  uientry_t       allowedkeywords;
  UIWidget        uiallowedkeywordsitem;
  UIWidget        uipltype;
  managepltree_t  *managepltree;
} managepl_t;

static void   managePlaylistLoad (GtkMenuItem *mi, gpointer udata);
static void   managePlaylistCopy (GtkMenuItem *mi, gpointer udata);
static void   managePlaylistLoadFile (void *udata, const char *fn);
static void   managePlaylistNew (GtkMenuItem *mi, gpointer udata);
static void   manageSetPlaylistName (managepl_t *managepl, const char *nm);

managepl_t *
managePlaylistAlloc (UIWidget *window, nlist_t *options, UIWidget *statusMsg)
{
  managepl_t *managepl;

  managepl = malloc (sizeof (managepl_t));
  uiutilsUIWidgetInit (&managepl->uipltype);
  managepl->ploldname = NULL;
  managepl->plbackupcreated = false;
  uiMenuInit (&managepl->plmenu);
  uiEntryInit (&managepl->plname, 20, 50);
  managepl->statusMsg = statusMsg;
  managepl->windowp = window;
  managepl->options = options;
  managepl->pltype = PLTYPE_AUTO;
  managepl->managepltree = NULL;
  managepl->uirating = NULL;
  managepl->uilowlevel = NULL;
  managepl->uihighlevel = NULL;
  uiutilsUIWidgetInit (&managepl->uiratingitem);
  uiutilsUIWidgetInit (&managepl->uilowlevelitem);
  uiutilsUIWidgetInit (&managepl->uihighlevelitem);
  uiEntryInit (&managepl->allowedkeywords, 20, 50);
  uiutilsUIWidgetInit (&managepl->uiallowedkeywordsitem);

  return managepl;
}

void
managePlaylistFree (managepl_t *managepl)
{
  if (managepl != NULL) {
    if (managepl->ploldname != NULL) {
      free (managepl->ploldname);
    }
    if (managepl->managepltree != NULL) {
      managePlaylistTreeFree (managepl->managepltree);
    }
    uiEntryFree (&managepl->plname);
    uiEntryFree (&managepl->allowedkeywords);
    uiratingFree (managepl->uirating);
    uilevelFree (managepl->uilowlevel);
    uilevelFree (managepl->uihighlevel);
    free (managepl);
  }
}

void
manageBuildUIPlaylist (managepl_t *managepl, UIWidget *vboxp)
{
  UIWidget            lcol;
  UIWidget            rcol;
  UIWidget            mainhbox;
  UIWidget            tophbox;
  UIWidget            hbox;
  UIWidget            uiwidget;
  GtkWidget           *widget;
  UIWidget            sg;
  UIWidget            sgB;

  uiutilsUIWidgetInit (&hbox);
  uiutilsUIWidgetInit (&uiwidget);
  uiCreateSizeGroupHoriz (&sg);
  uiCreateSizeGroupHoriz (&sgB);

  uiWidgetSetAllMargins (vboxp, uiBaseMarginSz * 2);

  uiCreateHorizBox (&tophbox);
  uiWidgetAlignVertStart (&tophbox);
  uiBoxPackStart (vboxp, &tophbox);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&tophbox, &hbox);

  /* CONTEXT: playlist management: label for playlist name */
  uiCreateColonLabel (&uiwidget, _("Playlist"));
  uiBoxPackStart (&hbox, &uiwidget);

  uiEntryCreate (&managepl->plname);
  uiEntrySetColor (&managepl->plname, bdjoptGetStr (OPT_P_UI_ACCENT_COL));
  /* CONTEXT: playlist management: default name for a new playlist */
  manageSetPlaylistName (managepl, _("New Playlist"));
  uiBoxPackStart (&hbox, &managepl->plname.uientry);

  uiCreateHorizBox (&hbox);
  uiWidgetSetMarginStart (&hbox, uiBaseMarginSz * 20);
  uiBoxPackStart (&tophbox, &hbox);

  /* CONTEXT: playlist management: label for playlist name */
  uiCreateColonLabel (&uiwidget, _("Playlist Type"));
  uiBoxPackStart (&hbox, &uiwidget);

  /* CONTEXT: playlist management: default playlist type */
  uiCreateLabel (&uiwidget, _("Automatic"));
  uiLabelSetColor (&uiwidget, bdjoptGetStr (OPT_P_UI_ACCENT_COL));
  uiBoxPackStart (&hbox, &uiwidget);
  uiutilsUIWidgetCopy (&managepl->uipltype, &uiwidget);

  uiCreateHorizBox (&mainhbox);
  uiBoxPackStartExpand (vboxp, &mainhbox);

  /* left side */
  uiCreateVertBox (&lcol);
  uiBoxPackStartExpand (&mainhbox, &lcol);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);

  /* CONTEXT: playlist management: maximum play time */
  uiCreateColonLabel (&uiwidget, _("Maximum Play Time"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

//  widget = uiSpinboxTimeCreate ();
//  uiBoxPackStart (&hbox, &uiwidget);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);

  /* CONTEXT: playlist management: stop at */
  uiCreateColonLabel (&uiwidget, _("Stop At"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

//  widget = uiSpinboxTimeCreate ();
//  uiBoxPackStart (&hbox, &uiwidget);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);

  /* CONTEXT: playlist management: stop after */
  uiCreateColonLabel (&uiwidget, _("Stop After"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  widget = uiSpinboxIntCreate ();
  uiBoxPackStartUW (&hbox, widget);
  uiSizeGroupAddW (&sgB, widget);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);

  /* CONTEXT: playlist management: Gap */
  uiCreateColonLabel (&uiwidget, _("Gap"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  widget = uiSpinboxIntCreate ();
  uiBoxPackStartUW (&hbox, widget);
  uiSizeGroupAddW (&sgB, widget);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);

  /* CONTEXT: playlist management: Play Announcements */
  uiCreateColonLabel (&uiwidget, _("Play Announcements"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  /* automatic and sequenced playlists; keep the widget so these */
  /* can be hidden */

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);
  uiutilsUIWidgetCopy (&managepl->uiratingitem, &hbox);

  /* CONTEXT: playlist management: Dance Rating */
  uiCreateColonLabel (&uiwidget, _("Dance Rating"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  managepl->uirating = uiratingSpinboxCreate (&hbox, false);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);
  uiutilsUIWidgetCopy (&managepl->uilowlevelitem, &hbox);

  /* CONTEXT: playlist management: Low Dance Level */
  uiCreateColonLabel (&uiwidget, _("Low Dance Level"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  managepl->uilowlevel = uilevelSpinboxCreate (&hbox, false);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);
  uiutilsUIWidgetCopy (&managepl->uihighlevelitem, &hbox);

  /* CONTEXT: playlist management: High Dance Level */
  uiCreateColonLabel (&uiwidget, _("High Dance Level"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  managepl->uihighlevel = uilevelSpinboxCreate (&hbox, false);

  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&lcol, &hbox);
  uiutilsUIWidgetCopy (&managepl->uiallowedkeywordsitem, &hbox);

  /* CONTEXT: playlist management: allowed keywords */
  uiCreateColonLabel (&uiwidget, _("Allowed Keywords"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiEntryCreate (&managepl->allowedkeywords);
  uiBoxPackStart (&hbox, &managepl->allowedkeywords.uientry);

  /* right side to hold the tree */
  uiCreateVertBox (&rcol);
  uiBoxPackStartExpand (&mainhbox, &rcol);

  managepl->managepltree = managePlaylistTreeAlloc ();
  manageBuildUIPlaylistTree (managepl->managepltree, &rcol);
}

uimenu_t *
managePlaylistMenu (managepl_t *managepl, GtkWidget *menubar)
{
  GtkWidget *menu;
  GtkWidget *menuitem;

  if (! managepl->plmenu.initialized) {
    menuitem = uiMenuAddMainItem (menubar,
        /* CONTEXT: menu selection: sequence: edit menu */
        &managepl->plmenu, _("Edit"));

    menu = uiCreateSubMenu (menuitem);

    /* CONTEXT: menu selection: playlist: edit menu: load */
    menuitem = uiMenuCreateItem (menu, _("Load"),
        managePlaylistLoad, managepl);

    /* CONTEXT: menu selection: playlist: edit menu: create copy */
    menuitem = uiMenuCreateItem (menu, _("Create Copy"),
        managePlaylistCopy, managepl);

    /* CONTEXT: menu selection: playlist: edit menu: new automatic sequence */
    menuitem = uiMenuCreateItem (menu, _("New Automatic Playlist"),
        managePlaylistNew, managepl);

    managepl->plmenu.initialized = true;
  }

  uiMenuDisplay (&managepl->plmenu);
  return &managepl->plmenu;
}

void
managePlaylistSave (managepl_t *managepl)
{
  char        onm [MAXPATHLEN];
  char        nnm [MAXPATHLEN];
  const char  *name;

  if (managepl->ploldname == NULL) {
    return;
  }

  name = uiEntryGetValue (&managepl->plname);

  /* the playlist has been renamed */
  if (strcmp (managepl->ploldname, name) != 0) {
    manageRenamePlaylistFiles (managepl->ploldname, name);
  }

  pathbldMakePath (onm, sizeof (onm),
      name, BDJ4_PLAYLIST_EXT, PATHBLD_MP_DATA);
  strlcat (onm, ".n", sizeof (onm));

  // ### allocate, create, populate the playlist

  pathbldMakePath (nnm, sizeof (nnm),
      name, BDJ4_SEQUENCE_EXT, PATHBLD_MP_DATA);
  if (! managepl->plbackupcreated) {
    filemanipBackup (nnm, 1);
    managepl->plbackupcreated = true;
  }
  filemanipMove (onm, nnm);
}

/* internal routines */

static void
managePlaylistLoad (GtkMenuItem *mi, gpointer udata)
{
  managepl_t  *managepl = udata;

  selectFileDialog (SELFILE_PLAYLIST, managepl->windowp, managepl->options,
      managepl, managePlaylistLoadFile);
}

static void
managePlaylistLoadFile (void *udata, const char *fn)
{
  managepl_t  *managepl = udata;
  playlist_t  *pl;

  managePlaylistSave (managepl);

  // ### load the playlist
  pl = playlistAlloc (NULL);
  playlistLoad (pl, fn);

  // ### set the data on-screen.

  // ### hide/show elements as necessary

  manageSetPlaylistName (managepl, fn);
  managepl->plbackupcreated = false;
}

static void
managePlaylistCopy (GtkMenuItem *mi, gpointer udata)
{
  managepl_t *managepl = udata;
  const char  *oname;
  char        newname [200];

  managePlaylistSave (managepl);

  oname = uiEntryGetValue (&managepl->plname);
  /* CONTEXT: the new name after 'create copy' (e.g. "Copy of DJ-2022-04") */
  snprintf (newname, sizeof (newname), _("Copy of %s"), oname);
  if (manageCreatePlaylistCopy (managepl->statusMsg, oname, newname)) {
    manageSetPlaylistName (managepl, newname);
    managepl->plbackupcreated = false;
  }
}

static void
managePlaylistNew (GtkMenuItem *mi, gpointer udata)
{
  managepl_t *managepl = udata;
  char        tbuff [200];

  managePlaylistSave (managepl);

  /* CONTEXT: sequence: default name for a new sequence */
  snprintf (tbuff, sizeof (tbuff), _("New Playlist"));
  manageSetPlaylistName (managepl, tbuff);
  managepl->plbackupcreated = false;
  managepl->pltype = PLTYPE_AUTO;

  // ### reset everything to defaults
}

static void
manageSetPlaylistName (managepl_t *managepl, const char *name)
{
  uiEntrySetValue (&managepl->plname, name);
  if (managepl->ploldname != NULL) {
    free (managepl->ploldname);
  }
  managepl->ploldname = strdup (name);
}
