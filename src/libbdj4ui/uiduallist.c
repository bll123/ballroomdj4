#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include <gtk/gtk.h>

#include "bdj4intl.h"
#include "bdjstring.h"
#include "istring.h"
#include "slist.h"
#include "ui.h"
#include "uiduallist.h"

enum {
  DUALLIST_COL_DISP,
  DUALLIST_COL_SB_PAD,
  DUALLIST_COL_DISP_IDX,
  DUALLIST_COL_MAX,
};

enum {
  DUALLIST_MOVE_PREV = -1,
  DUALLIST_MOVE_NEXT = 1,
};

enum {
  DUALLIST_SEARCH_INSERT,
  DUALLIST_SEARCH_REMOVE,
};

static bool uiduallistMovePrev (void *tduallist);
static bool uiduallistMoveNext (void *tduallist);
static void uiduallistMove (uiduallist_t *duallist, int which, int dir);
static bool uiduallistDispSelect (void *udata);
static bool uiduallistDispRemove (void *udata);
static gboolean uiduallistSourceSearch (GtkTreeModel* model,
    GtkTreePath* path, GtkTreeIter* iter, gpointer udata);
static gboolean uiduallistGetData (GtkTreeModel* model, GtkTreePath* path,
    GtkTreeIter* iter, gpointer udata);
static void uiduallistSetDefaultSelection (uiduallist_t *duallist, int which);

uiduallist_t *
uiCreateDualList (UIWidget *mainvbox, int flags,
    const char *sourcetitle, const char *targettitle)
{
  uiduallist_t  *duallist;
  UIWidget      vbox;
  UIWidget      hbox;
  UIWidget      dvbox;
  UIWidget      uiwidget;
  GtkWidget     *tree;
  GtkListStore  *store;
  GtkCellRenderer *renderer = NULL;
  GtkTreeViewColumn *column = NULL;

  duallist = malloc (sizeof (uiduallist_t));
  for (int i = 0; i < DUALLIST_TREE_MAX; ++i) {
    duallist->trees [i].tree = NULL;
    duallist->trees [i].sel = NULL;
  }
  duallist->flags = flags;
  duallist->pos = 0;
  duallist->searchtype = DUALLIST_SEARCH_INSERT;
  duallist->searchstr = NULL;
  duallist->savelist = NULL;
  duallist->changed = false;

  uiutilsUICallbackInit (&duallist->moveprevcb, uiduallistMovePrev, duallist, NULL);
  uiutilsUICallbackInit (&duallist->movenextcb, uiduallistMoveNext, duallist, NULL);
  uiutilsUICallbackInit (&duallist->selectcb, uiduallistDispSelect, duallist, NULL);
  uiutilsUICallbackInit (&duallist->removecb, uiduallistDispRemove, duallist, NULL);

  uiutilsUIWidgetInit (&vbox);
  uiutilsUIWidgetInit (&hbox);
  uiutilsUIWidgetInit (&dvbox);
  uiutilsUIWidgetInit (&uiwidget);

  uiCreateHorizBox (&hbox);
  uiWidgetAlignHorizStart (&hbox);
  uiBoxPackStartExpand (mainvbox, &hbox);

  uiCreateVertBox (&vbox);
  uiWidgetSetMarginStart (&vbox, uiBaseMarginSz * 8);
  uiWidgetSetMarginTop (&vbox, uiBaseMarginSz * 8);
  uiBoxPackStartExpand (&hbox, &vbox);

  if (sourcetitle != NULL) {
    uiCreateLabel (&uiwidget, sourcetitle);
    uiBoxPackStart (&vbox, &uiwidget);
  }

  uiCreateScrolledWindow (&uiwidget, 300);
  uiWidgetExpandVert (&uiwidget);
  uiBoxPackStartExpand (&vbox, &uiwidget);

  tree = uiCreateTreeView ();
  uiSetCss (tree,
      "treeview { background-color: shade(@theme_base_color,0.8); } "
      "treeview:selected { background-color: @theme_selected_bg_color; } ");
  uiWidgetExpandVertW (tree);
  uiBoxPackInWindowUW (&uiwidget, tree);
  duallist->trees [DUALLIST_TREE_SOURCE].tree = tree;
  duallist->trees [DUALLIST_TREE_SOURCE].sel =
      gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

  store = gtk_list_store_new (DUALLIST_COL_MAX,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_LONG);
  gtk_tree_view_set_model (GTK_TREE_VIEW (tree), GTK_TREE_MODEL (store));
  g_object_unref (store);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", DUALLIST_COL_DISP,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", DUALLIST_COL_SB_PAD,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  uiCreateVertBox (&dvbox);
  uiWidgetSetAllMargins (&dvbox, uiBaseMarginSz * 4);
  uiWidgetSetMarginTop (&dvbox, uiBaseMarginSz * 64);
  uiWidgetAlignVertStart (&dvbox);
  uiBoxPackStart (&hbox, &dvbox);

  uiCreateButton (&uiwidget, &duallist->selectcb,
      /* CONTEXT: configuration: display settings: button: add the selected field */
      _("Select"), "button_right");
  uiBoxPackStart (&dvbox, &uiwidget);

  uiCreateButton (&uiwidget, &duallist->removecb,
      /* CONTEXT: configuration: display settings: button: remove the selected field */
      _("Remove"), "button_left");
  uiBoxPackStart (&dvbox, &uiwidget);

  uiCreateVertBox (&vbox);
  uiWidgetSetMarginStart (&vbox, uiBaseMarginSz * 8);
  uiWidgetSetMarginTop (&vbox, uiBaseMarginSz * 8);
  uiBoxPackStartExpand (&hbox, &vbox);

  if (targettitle != NULL) {
    uiCreateLabel (&uiwidget, targettitle);
    uiBoxPackStart (&vbox, &uiwidget);
  }

  uiCreateScrolledWindow (&uiwidget, 300);
  uiWidgetExpandVert (&uiwidget);
  uiBoxPackStartExpand (&vbox, &uiwidget);

  tree = uiCreateTreeView ();
  uiSetCss (tree,
      "treeview { background-color: shade(@theme_base_color,0.8); } "
      "treeview:selected { background-color: @theme_selected_bg_color; } ");
  uiWidgetExpandVertW (tree);
  uiBoxPackInWindowUW (&uiwidget, tree);
  duallist->trees [DUALLIST_TREE_TARGET].tree = tree;
  duallist->trees [DUALLIST_TREE_TARGET].sel =
      gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

  store = gtk_list_store_new (DUALLIST_COL_MAX,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_LONG);
  gtk_tree_view_set_model (GTK_TREE_VIEW (tree), GTK_TREE_MODEL (store));
  g_object_unref (store);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", DUALLIST_COL_DISP,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", DUALLIST_COL_SB_PAD,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  uiCreateVertBox (&dvbox);
  uiWidgetSetAllMargins (&dvbox, uiBaseMarginSz * 4);
  uiWidgetSetMarginTop (&dvbox, uiBaseMarginSz * 64);
  uiWidgetAlignVertStart (&dvbox);
  uiBoxPackStart (&hbox, &dvbox);

  uiCreateButton (&uiwidget, &duallist->moveprevcb,
      /* CONTEXT: configuration: display settings: button: move the selected field up */
      _("Move Up"), "button_up");
  uiBoxPackStart (&dvbox, &uiwidget);

  uiCreateButton (&uiwidget, &duallist->movenextcb,
      /* CONTEXT: configuration: display settings: button: move the selected field down */
      _("Move Down"), "button_down");
  uiBoxPackStart (&dvbox, &uiwidget);

  return duallist;
}

void
uiduallistFree (uiduallist_t *duallist)
{
  if (duallist != NULL) {
    free (duallist);
  }
}


void
uiduallistSet (uiduallist_t *duallist, slist_t *slist, int which)
{
  GtkWidget     *tree;
  GtkListStore  *store;
  GtkTreeIter   iter;
  char          *keystr;
  slistidx_t    siteridx;
  char          tmp [40];


  if (duallist == NULL) {
    return;
  }

  if (which >= DUALLIST_TREE_MAX) {
    return;
  }

  if (which == DUALLIST_TREE_SOURCE) {
    duallist->sourcelist = slist;
  }
  /* the assumption made is that the source tree has been populated */
  /* just before the target tree */

  tree = duallist->trees [which].tree;
  store = gtk_list_store_new (DUALLIST_COL_MAX,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_LONG);
  assert (store != NULL);

  slistStartIterator (slist, &siteridx);
  while ((keystr = slistIterateKey (slist, &siteridx)) != NULL) {
    long    val;

    val = slistGetNum (slist, keystr);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        DUALLIST_COL_DISP, keystr,
        DUALLIST_COL_SB_PAD, "    ",
        DUALLIST_COL_DISP_IDX, val,
        -1);

    /* if inserting into the target tree, and the persistent flag */
    /* is not set, remove the matching entries from the source tree */
    if (which == DUALLIST_TREE_TARGET &&
        (duallist->flags & DUALLIST_FLAGS_PERSISTENT) != DUALLIST_FLAGS_PERSISTENT) {
      GtkWidget         *stree;
      GtkTreeModel      *smodel;
      GtkTreePath       *path;
      GtkTreeIter       siter;

      stree = duallist->trees [DUALLIST_TREE_SOURCE].tree;
      smodel = gtk_tree_view_get_model (GTK_TREE_VIEW (stree));

      duallist->pos = 0;
      duallist->searchstr = keystr;
      duallist->searchtype = DUALLIST_SEARCH_REMOVE;
      /* this is not efficient, but the lists are relatively short */
      gtk_tree_model_foreach (smodel, uiduallistSourceSearch, duallist);

      snprintf (tmp, sizeof (tmp), "%d", duallist->pos);
      path = gtk_tree_path_new_from_string (tmp);
      if (gtk_tree_model_get_iter (smodel, &siter, path)) {
        gtk_list_store_remove (GTK_LIST_STORE (smodel), &siter);
      }
      gtk_tree_path_free (path);
    }
  }

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree), GTK_TREE_MODEL (store));
  uiduallistSetDefaultSelection (duallist, which);
  g_object_unref (store);
}

inline bool
uiduallistIsChanged (uiduallist_t *duallist)
{
  if (duallist == NULL) {
    return false;
  }

  return duallist->changed;
}

inline void
uiduallistClearChanged (uiduallist_t *duallist)
{
  if (duallist == NULL) {
    return;
  }

  duallist->changed = false;
}

slist_t *
uiduallistGetList (uiduallist_t *duallist)
{
  slist_t       *slist;
  GtkWidget     *ttree;
  GtkTreeModel  *tmodel;


  ttree = duallist->trees [DUALLIST_TREE_TARGET].tree;
  tmodel = gtk_tree_view_get_model (GTK_TREE_VIEW (ttree));

  slist = slistAlloc ("duallist-return", LIST_UNORDERED, NULL);
  duallist->savelist = slist;
  gtk_tree_model_foreach (tmodel, uiduallistGetData, duallist);
  return slist;
}


/* internal routines */

static bool
uiduallistMovePrev (void *tduallist)
{
  uiduallist_t  *duallist = tduallist;
  uiduallistMove (duallist, DUALLIST_TREE_TARGET, DUALLIST_MOVE_PREV);
  return UICB_CONT;
}

static bool
uiduallistMoveNext (void *tduallist)
{
  uiduallist_t  *duallist = tduallist;
  uiduallistMove (duallist, DUALLIST_TREE_TARGET, DUALLIST_MOVE_NEXT);
  return UICB_CONT;
}

static void
uiduallistMove (uiduallist_t *duallist, int which, int dir)
{
  GtkWidget         *tree;
  GtkTreeSelection  *sel;
  GtkTreeModel      *model;
  GtkTreeIter       iter;
  GtkTreeIter       citer;
  GtkTreePath       *path;
  char              *pathstr;
  int               count;
  gboolean          valid;
  int               idx;

  tree = duallist->trees [which].tree;
  if (tree == NULL) {
    return;
  }

  sel = duallist->trees [which].sel;
  count = gtk_tree_selection_count_selected_rows (sel);
  if (count != 1) {
    return;
  }
  gtk_tree_selection_get_selected (sel, &model, &iter);

  path = gtk_tree_model_get_path (model, &iter);
  pathstr = gtk_tree_path_to_string (path);
  sscanf (pathstr, "%d", &idx);
  free (pathstr);
  gtk_tree_path_free (path);

  memcpy (&citer, &iter, sizeof (iter));
  if (dir == DUALLIST_MOVE_PREV) {
    valid = gtk_tree_model_iter_previous (model, &iter);
    if (valid) {
      gtk_list_store_move_before (GTK_LIST_STORE (model), &citer, &iter);
    }
  } else {
    valid = gtk_tree_model_iter_next (model, &iter);
    if (valid) {
      gtk_list_store_move_after (GTK_LIST_STORE (model), &citer, &iter);
    }
  }
  duallist->changed = true;
}

static bool
uiduallistDispSelect (void *udata)
{
  uiduallist_t      *duallist = udata;
  GtkWidget         *ttree;
  GtkTreeSelection  *tsel;
  GtkWidget         *stree;
  GtkTreeModel      *smodel;
  GtkTreeIter       siter;
  int               count;

  stree = duallist->trees [DUALLIST_TREE_SOURCE].tree;

  count = uiTreeViewGetSelection (stree, &smodel, &siter);

  if (count == 1) {
    char          *str;
    glong         tval;
    GtkTreeModel  *tmodel;
    GtkTreeIter   titer;
    GtkTreePath   *path;

    ttree = duallist->trees [DUALLIST_TREE_TARGET].tree;
    tsel = duallist->trees [DUALLIST_TREE_TARGET].sel;
    tmodel = gtk_tree_view_get_model (GTK_TREE_VIEW (ttree));

    gtk_tree_model_get (smodel, &siter, DUALLIST_COL_DISP, &str, -1);
    gtk_tree_model_get (smodel, &siter, DUALLIST_COL_DISP_IDX, &tval, -1);

    gtk_list_store_append (GTK_LIST_STORE (tmodel), &titer);
    gtk_list_store_set (GTK_LIST_STORE (tmodel), &titer,
        DUALLIST_COL_DISP, str,
        DUALLIST_COL_SB_PAD, "    ",
        DUALLIST_COL_DISP_IDX, tval,
        -1);

    path = gtk_tree_model_get_path (tmodel, &titer);
    gtk_tree_selection_select_path (tsel, path);

    if ((duallist->flags & DUALLIST_FLAGS_PERSISTENT) != DUALLIST_FLAGS_PERSISTENT) {
      gtk_list_store_remove (GTK_LIST_STORE (smodel), &siter);
    }
    duallist->changed = true;
  }
  return UICB_CONT;
}

static bool
uiduallistDispRemove (void *udata)
{
  uiduallist_t  *duallist = udata;
  GtkWidget     *ttree;
  GtkTreeSelection *tsel;
  GtkTreeModel  *tmodel;
  GtkTreeIter   piter;
  GtkTreeIter   titer;
  GtkTreePath   *path;
  int           count;


  ttree = duallist->trees [DUALLIST_TREE_TARGET].tree;
  tsel = duallist->trees [DUALLIST_TREE_TARGET].sel;
  count = uiTreeViewGetSelection (ttree, &tmodel, &titer);

  if (count == 1) {
    char          *str;
    long          tval;
    GtkWidget     *stree;
    GtkTreeSelection *ssel;
    GtkTreeModel  *smodel;
    GtkTreeIter   siter;

    if ((duallist->flags & DUALLIST_FLAGS_PERSISTENT) != DUALLIST_FLAGS_PERSISTENT) {
      stree = duallist->trees [DUALLIST_TREE_SOURCE].tree;
      ssel = duallist->trees [DUALLIST_TREE_SOURCE].sel;
      smodel = gtk_tree_view_get_model (GTK_TREE_VIEW (stree));

      gtk_tree_model_get (tmodel, &titer, DUALLIST_COL_DISP, &str, -1);
      gtk_tree_model_get (tmodel, &titer, DUALLIST_COL_DISP_IDX, &tval, -1);

      duallist->pos = 0;
      duallist->searchstr = str;
      duallist->searchtype = DUALLIST_SEARCH_INSERT;
      gtk_tree_model_foreach (smodel, uiduallistSourceSearch, duallist);

      gtk_list_store_insert (GTK_LIST_STORE (smodel), &siter, duallist->pos);
      gtk_list_store_set (GTK_LIST_STORE (smodel), &siter,
          DUALLIST_COL_DISP, str,
          DUALLIST_COL_SB_PAD, "    ",
          DUALLIST_COL_DISP_IDX, tval,
          -1);

      path = gtk_tree_model_get_path (smodel, &siter);
      gtk_tree_selection_select_path (ssel, path);
      gtk_tree_path_free (path);
    }

    path = NULL;

    /* only select the previous if the current iter is at the last */
    memcpy (&piter, &titer, sizeof (GtkTreeIter));
    if (! gtk_tree_model_iter_next (tmodel, &piter)) {
      memcpy (&piter, &titer, sizeof (GtkTreeIter));
      if (gtk_tree_model_iter_previous (tmodel, &piter)) {
        path = gtk_tree_model_get_path (tmodel, &piter);
      }
    }

    gtk_list_store_remove (GTK_LIST_STORE (tmodel), &titer);

    if (path != NULL) {
      gtk_tree_selection_select_path (tsel, path);
      gtk_tree_path_free (path);
    }

    duallist->changed = true;
  }
  return UICB_CONT;
}

static gboolean
uiduallistSourceSearch (GtkTreeModel* model, GtkTreePath* path,
    GtkTreeIter* iter, gpointer udata)
{
  uiduallist_t  *duallist = udata;
  char          *str;

  gtk_tree_model_get (model, iter, DUALLIST_COL_DISP, &str, -1);
  if (duallist->searchtype == DUALLIST_SEARCH_INSERT) {
    if (istringCompare (duallist->searchstr, str) < 0) {
      return TRUE;
    }
  }
  if (duallist->searchtype == DUALLIST_SEARCH_REMOVE) {
    if (istringCompare (duallist->searchstr, str) == 0) {
      return TRUE;
    }
  }

  duallist->pos += 1;
  return FALSE; // continue iterating
}

static gboolean
uiduallistGetData (GtkTreeModel* model, GtkTreePath* path,
    GtkTreeIter* iter, gpointer udata)
{
  uiduallist_t  *duallist = udata;
  char          *str;
  long          tval;

  gtk_tree_model_get (model, iter, DUALLIST_COL_DISP, &str, -1);
  gtk_tree_model_get (model, iter, DUALLIST_COL_DISP_IDX, &tval, -1);
  slistSetNum (duallist->savelist, str, tval);
  return FALSE; // continue iterating
}


static void
uiduallistSetDefaultSelection (uiduallist_t *duallist, int which)
{
  int               count;
  GtkWidget         *tree;
  GtkTreeSelection  *sel;
  GtkTreeIter       iter;
  GtkTreeModel      *model;

  if (duallist == NULL) {
    return;
  }
  if (which >= DUALLIST_TREE_MAX) {
    return;
  }

  tree = duallist->trees [which].tree;
  sel = duallist->trees [which].sel;

  count = uiTreeViewGetSelection (tree, &model, &iter);
  if (count != 1) {
    GtkTreePath   *path;

    path = gtk_tree_path_new_from_string ("0");
    if (path != NULL) {
      gtk_tree_selection_select_path (sel, path);
      gtk_tree_path_free (path);
    }
  }
}
