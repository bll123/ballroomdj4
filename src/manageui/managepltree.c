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
#include "bdjvarsdf.h"
#include "dance.h"
#include "manageui.h"
#include "playlist.h"
#include "ui.h"
#include "uiutils.h"

enum {
  MPLTREE_COL_DANCE_SELECT,
  MPLTREE_COL_DANCE,
  MPLTREE_COL_COUNT,
  MPLTREE_COL_MAXPLAYTIME,
  MPLTREE_COL_LOWBPM,
  MPLTREE_COL_HIGHBPM,
  MPLTREE_COL_SB_PAD,
  MPLTREE_COL_DANCE_IDX,
  MPLTREE_COL_EDITABLE,
  MPLTREE_COL_ADJUST,
  MPLTREE_COL_DIGITS,
  MPLTREE_COL_MAX,
};

typedef struct managepltree {
  GtkWidget         *tree;
  GtkTreeViewColumn *danceselcol;
  GtkTreeViewColumn *countcol;
  GtkTreeViewColumn *lowbpmcol;
  GtkTreeViewColumn *highbpmcol;
} managepltree_t;

static void managePlaylistTreeSetColumnVisibility (managepltree_t *managepltree, int pltype);
static void managePlaylistTreeCreate (managepltree_t *managepltree);

managepltree_t *
managePlaylistTreeAlloc (void)
{
  managepltree_t *managepltree;

  managepltree = malloc (sizeof (managepltree_t));
  managepltree->tree = NULL;
  return managepltree;
}

void
managePlaylistTreeFree (managepltree_t *managepltree)
{
  if (managepltree != NULL) {
    free (managepltree);
  }
}

void
manageBuildUIPlaylistTree (managepltree_t *managepltree, UIWidget *vboxp)
{
  UIWidget    uiwidget;
  GtkWidget   *tree;
  GtkCellRenderer *renderer = NULL;
  GtkTreeViewColumn *column = NULL;

  uiCreateScrolledWindow (&uiwidget, 300);
  uiWidgetExpandVert (&uiwidget);
  uiBoxPackStartExpand (vboxp, &uiwidget);

  tree = uiCreateTreeView ();
  uiWidgetExpandVertW (tree);
  uiBoxPackInWindowUW (&uiwidget, tree);
  managepltree->tree = tree;

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), TRUE);

  renderer = gtk_cell_renderer_toggle_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "active", MPLTREE_COL_DANCE_SELECT,
      NULL);
  managepltree->danceselcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_DANCE,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: dance column header */
  gtk_tree_view_column_set_title (column, _("Dance"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_COUNT));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_COUNT,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->countcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: count column header */
  gtk_tree_view_column_set_title (column, _("Count"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_MAXPLAYTIME,
      "editable", MPLTREE_COL_EDITABLE,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: max play time column header (keep short) */
  gtk_tree_view_column_set_title (column, _("Maximum\nPlay Time"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_LOWBPM));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_LOWBPM,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->lowbpmcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: low bpm/mpm column header */
  gtk_tree_view_column_set_title (column, _("Low BPM"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_LOWBPM));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_HIGHBPM,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->highbpmcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: high bpm/mpm column */
  gtk_tree_view_column_set_title (column, _("High BPM"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_SB_PAD,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  managePlaylistTreeCreate (managepltree);
}

void
managePlaylistTreePopulate (managepltree_t *managepltree, playlist_t *pl)
{
  dance_t       *dances;
  int           pltype;
  ilistidx_t    iteridx;
  ilistidx_t    dkey;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  int           count;
  char          tbuff [40];

  dances = bdjvarsdfGet (BDJVDF_DANCES);

  pltype = playlistGetConfigNum (pl, PLAYLIST_TYPE);
  managePlaylistTreeSetColumnVisibility (managepltree, pltype);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (managepltree->tree));

  danceStartIterator (dances, &iteridx);
  count = 0;
  while ((dkey = danceIterate (dances, &iteridx)) >= 0) {
    long  sel, dcount, bpmlow, bpmhigh, mpt;

    sel = playlistGetDanceNum (pl, dkey, PLDANCE_SELECTED);
    dcount = playlistGetDanceNum (pl, dkey, PLDANCE_COUNT);
    if (dcount < 0) { dcount = 0; }
    mpt = playlistGetDanceNum (pl, dkey, PLDANCE_MAXPLAYTIME);
    if (mpt < 0) { mpt = 0; }
    bpmlow = playlistGetDanceNum (pl, dkey, PLDANCE_BPM_LOW);
    if (bpmlow < 0) { bpmlow = 0; }
    bpmhigh = playlistGetDanceNum (pl, dkey, PLDANCE_BPM_HIGH);
    if (bpmhigh < 0) { bpmhigh = 0; }

    snprintf (tbuff, sizeof (tbuff), "%d", count);
    if (gtk_tree_model_get_iter_from_string (model, &iter, tbuff)) {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
          MPLTREE_COL_DANCE_SELECT, sel,
          MPLTREE_COL_MAXPLAYTIME, mpt,
          MPLTREE_COL_COUNT, dcount,
          MPLTREE_COL_LOWBPM, bpmlow,
          MPLTREE_COL_HIGHBPM, bpmhigh,
          -1);
    }
    ++count;
  }
}


/* internal routines */

static void
managePlaylistTreeSetColumnVisibility (managepltree_t *managepltree, int pltype)
{
  switch (pltype) {
    case PLTYPE_SONGLIST: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->countcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, FALSE);
      break;
    }
    case PLTYPE_AUTO: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->countcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, TRUE);
      break;
    }
    case PLTYPE_SEQUENCE: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->countcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, TRUE);
      break;
    }
  }
}

static void
managePlaylistTreeCreate (managepltree_t *managepltree)
{
  dance_t       *dances;
  GtkTreeIter   iter;
  slist_t       *dancelist;
  slistidx_t    iteridx;
  ilistidx_t    key;
  GtkWidget   *tree;
  GtkListStore  *store;

  tree = managepltree->tree;

  store = gtk_list_store_new (MPLTREE_COL_MAX,
      G_TYPE_BOOLEAN, // dance select
      G_TYPE_STRING,  // dance
      G_TYPE_LONG,    // count
      G_TYPE_LONG,    // max play time
      G_TYPE_LONG,    // low bpm
      G_TYPE_LONG,    // high bpm
      G_TYPE_STRING,  // pad
      G_TYPE_LONG,    // dance idx
      G_TYPE_LONG,    // editable
      G_TYPE_OBJECT,  // adjust
      G_TYPE_LONG);   // digits

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  dancelist = danceGetDanceList (dances);

  slistStartIterator (dancelist, &iteridx);
  while ((key = slistIterateValueNum (dancelist, &iteridx)) >= 0) {
    char        *dancedisp;

    dancedisp = danceGetStr (dances, key, DANCE_DANCE);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        MPLTREE_COL_DANCE_SELECT, 0,
        MPLTREE_COL_DANCE, dancedisp,
        MPLTREE_COL_MAXPLAYTIME, 0,
        MPLTREE_COL_LOWBPM, 0,
        MPLTREE_COL_HIGHBPM, 0,
        MPLTREE_COL_SB_PAD, "  ",
        MPLTREE_COL_DANCE_IDX, key,
        MPLTREE_COL_EDITABLE, 1,
        MPLTREE_COL_ADJUST, NULL,
        MPLTREE_COL_DIGITS, 0,
        -1);
  }

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree), GTK_TREE_MODEL (store));
  g_object_unref (store);
}

