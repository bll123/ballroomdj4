#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include <gtk/gtk.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "ui.h"
#include "uiutils.h"

enum {
  UIUTILS_DROPDOWN_COL_IDX,
  UIUTILS_DROPDOWN_COL_STR,
  UIUTILS_DROPDOWN_COL_DISP,
  UIUTILS_DROPDOWN_COL_SB_PAD,
  UIUTILS_DROPDOWN_COL_MAX,
};

typedef struct uidropdown {
  char          *title;
  UIWidget      *parentwin;
  UIWidget      button;
  UICallback    buttoncb;
  UIWidget      window;
  UICallback    closecb;
  GtkWidget     *tree;
  GtkTreeSelection  *sel;
  slist_t       *strIndexMap;
  nlist_t       *keylist;
  gulong        closeHandlerId;
  char          *strSelection;
  int           maxwidth;
  bool          open : 1;
  bool          iscombobox : 1;
} uidropdown_t;

/* drop-down/combobox handling */
static bool uiDropDownWindowShow (void *udata);
static bool uiDropDownClose (void *udata);
static void uiDropDownButtonCreate (uidropdown_t *dropdown);
static void uiDropDownWindowCreate (uidropdown_t *dropdown,
    void *processSelectionCallback, void *udata);
static void uiDropDownSelectionSet (uidropdown_t *dropdown,
    nlistidx_t internalidx);

uidropdown_t *
uiDropDownInit (void)
{
  uidropdown_t  *dropdown;

  dropdown = malloc (sizeof (uidropdown_t));

  dropdown->title = NULL;
  dropdown->parentwin = NULL;
  uiutilsUIWidgetInit (&dropdown->button);
  uiutilsUIWidgetInit (&dropdown->window);
  dropdown->tree = NULL;
  dropdown->sel = NULL;
  dropdown->closeHandlerId = 0;
  dropdown->strSelection = NULL;
  dropdown->strIndexMap = NULL;
  dropdown->keylist = NULL;
  dropdown->open = false;
  dropdown->iscombobox = false;
  dropdown->maxwidth = 10;

  return dropdown;
}


void
uiDropDownFree (uidropdown_t *dropdown)
{
  if (dropdown != NULL) {
    if (dropdown->title != NULL) {
      free (dropdown->title);
    }
    if (dropdown->strSelection != NULL) {
      free (dropdown->strSelection);
    }
    if (dropdown->strIndexMap != NULL) {
      slistFree (dropdown->strIndexMap);
    }
    if (dropdown->keylist != NULL) {
      nlistFree (dropdown->keylist);
    }
    free (dropdown);
  }
}

UIWidget *
uiDropDownCreate (UIWidget *parentwin,
    char *title, void *processSelectionCallback,
    uidropdown_t *dropdown, void *udata)
{
  dropdown->parentwin = parentwin;
  dropdown->title = strdup (title);
  uiDropDownButtonCreate (dropdown);
  uiDropDownWindowCreate (dropdown, processSelectionCallback, udata);
  return &dropdown->button;
}

UIWidget *
uiComboboxCreate (UIWidget *parentwin,
    char *title, void *processSelectionCallback,
    uidropdown_t *dropdown, void *udata)
{
  dropdown->iscombobox = true;
  return uiDropDownCreate (parentwin,
      title, processSelectionCallback,
      dropdown, udata);
}

void
uiDropDownSetList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel)
{
  char              *strval;
  char              *dispval;
  GtkTreeIter       iter;
  GtkListStore      *store = NULL;
  GtkCellRenderer   *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  ilistidx_t        iteridx;
  nlistidx_t        internalidx;
  char              tbuff [200];


  store = gtk_list_store_new (UIUTILS_DROPDOWN_COL_MAX,
      G_TYPE_LONG, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  assert (store != NULL);

  dropdown->strIndexMap = slistAlloc ("uiutils-str-index", LIST_ORDERED, NULL);
  internalidx = 0;

  dropdown->maxwidth = slistGetMaxKeyWidth (list);

  if (! dropdown->iscombobox) {
    snprintf (tbuff, sizeof (tbuff), "%-*s",
        dropdown->maxwidth, dropdown->title);
    uiButtonSetText (&dropdown->button, tbuff);
  }

  if (dropdown->iscombobox && selectLabel != NULL) {
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        UIUTILS_DROPDOWN_COL_IDX, (glong) -1,
        UIUTILS_DROPDOWN_COL_STR, "",
        UIUTILS_DROPDOWN_COL_DISP, selectLabel,
        UIUTILS_DROPDOWN_COL_SB_PAD, "  ",
        -1);
    slistSetNum (dropdown->strIndexMap, "", internalidx++);
  }

  slistStartIterator (list, &iteridx);

  while ((dispval = slistIterateKey (list, &iteridx)) != NULL) {
    strval = slistGetStr (list, dispval);
    slistSetNum (dropdown->strIndexMap, strval, internalidx);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        UIUTILS_DROPDOWN_COL_IDX, (glong) internalidx,
        UIUTILS_DROPDOWN_COL_STR, strval,
        UIUTILS_DROPDOWN_COL_DISP, dispval,
        UIUTILS_DROPDOWN_COL_SB_PAD, "  ",
        -1);
    ++internalidx;
  }

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", UIUTILS_DROPDOWN_COL_DISP, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dropdown->tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", UIUTILS_DROPDOWN_COL_SB_PAD, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dropdown->tree), column);

  gtk_tree_view_set_model (GTK_TREE_VIEW (dropdown->tree),
      GTK_TREE_MODEL (store));
  g_object_unref (store);
}

void
uiDropDownSetNumList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel)
{
  char              *dispval;
  GtkTreeIter       iter;
  GtkListStore      *store = NULL;
  GtkCellRenderer   *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  char              tbuff [200];
  ilistidx_t        iteridx;
  nlistidx_t        internalidx;
  nlistidx_t        idx;

  store = gtk_list_store_new (UIUTILS_DROPDOWN_COL_MAX,
      G_TYPE_LONG, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  assert (store != NULL);

  dropdown->keylist = nlistAlloc ("uiutils-keylist", LIST_ORDERED, NULL);
  internalidx = 0;

  dropdown->maxwidth = slistGetMaxKeyWidth (list);

  if (! dropdown->iscombobox) {
    snprintf (tbuff, sizeof (tbuff), "%-*s",
        dropdown->maxwidth, dropdown->title);
    uiButtonSetText (&dropdown->button, tbuff);
  }

  if (dropdown->iscombobox && selectLabel != NULL) {
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
        UIUTILS_DROPDOWN_COL_IDX, (glong) -1,
        UIUTILS_DROPDOWN_COL_STR, "",
        UIUTILS_DROPDOWN_COL_DISP, selectLabel,
        UIUTILS_DROPDOWN_COL_SB_PAD, "  ",
        -1);
    nlistSetNum (dropdown->keylist, -1, internalidx);
    ++internalidx;
  }

  slistStartIterator (list, &iteridx);

  while ((dispval = slistIterateKey (list, &iteridx)) != NULL) {
    idx = slistGetNum (list, dispval);
    nlistSetNum (dropdown->keylist, idx, internalidx);

    gtk_list_store_append (store, &iter);
    snprintf (tbuff, sizeof (tbuff), "%-*s",
        dropdown->maxwidth, dispval);
    gtk_list_store_set (store, &iter,
        UIUTILS_DROPDOWN_COL_IDX, (glong) idx,
        UIUTILS_DROPDOWN_COL_STR, "",
        UIUTILS_DROPDOWN_COL_DISP, tbuff,
        UIUTILS_DROPDOWN_COL_SB_PAD, "  ",
        -1);
    ++internalidx;
  }

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", UIUTILS_DROPDOWN_COL_DISP, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dropdown->tree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", UIUTILS_DROPDOWN_COL_SB_PAD, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dropdown->tree), column);

  gtk_tree_view_set_model (GTK_TREE_VIEW (dropdown->tree),
      GTK_TREE_MODEL (store));
  g_object_unref (store);
}

void
uiDropDownSelectionSetNum (uidropdown_t *dropdown, nlistidx_t idx)
{
  nlistidx_t    internalidx;


  if (dropdown == NULL) {
    return;
  }

  if (dropdown->keylist == NULL) {
    internalidx = 0;
  } else {
    internalidx = nlistGetNum (dropdown->keylist, idx);
  }
  uiDropDownSelectionSet (dropdown, internalidx);
}

void
uiDropDownSelectionSetStr (uidropdown_t *dropdown, const char *stridx)
{
  nlistidx_t    internalidx;


  if (dropdown == NULL) {
    return;
  }

  if (dropdown->strIndexMap == NULL) {
    internalidx = 0;
  } else {
    internalidx = slistGetNum (dropdown->strIndexMap, stridx);
    if (internalidx < 0) {
      internalidx = 0;
    }
  }
  uiDropDownSelectionSet (dropdown, internalidx);
}

void
uiDropDownDisable (uidropdown_t *dropdown)
{
  if (dropdown == NULL) {
    return;
  }
  uiWidgetDisable (&dropdown->button);
}

void
uiDropDownEnable (uidropdown_t *dropdown)
{
  if (dropdown == NULL) {
    return;
  }
  uiWidgetEnable (&dropdown->button);
}

char *
uiDropDownGetString (uidropdown_t *dropdown)
{
  if (dropdown == NULL) {
    return NULL;
  }
  return dropdown->strSelection;
}

/* internal routines */

static bool
uiDropDownWindowShow (void *udata)
{
  uidropdown_t  *dropdown = udata;
  int           x, y;
  int           bx, by;


  if (dropdown == NULL) {
    return UICB_STOP;
  }

  bx = 0;
  by = 0;
  uiWindowGetPosition (dropdown->parentwin, &x, &y);
  if (&dropdown->button != NULL) {
    uiWidgetGetPosition (&dropdown->button, &bx, &by);
  }
  uiWidgetShowAll (&dropdown->window);
  uiWindowMove (&dropdown->window, bx + x + 4, by + y + 4 + 30);
  dropdown->open = true;
  return UICB_CONT;
}

static bool
uiDropDownClose (void *udata)
{
  uidropdown_t *dropdown = udata;

  if (dropdown->open) {
    uiWidgetHide (&dropdown->window);
    dropdown->open = false;
  }
  uiWindowPresent (dropdown->parentwin);

  return UICB_CONT;
}

static void
uiDropDownButtonCreate (uidropdown_t *dropdown)
{
  uiutilsUICallbackInit (&dropdown->buttoncb, uiDropDownWindowShow, dropdown);
  uiCreateButton (&dropdown->button, &dropdown->buttoncb, NULL,
      "button_down_small");
  uiButtonAlignLeft (&dropdown->button);
  uiButtonSetImagePosRight (&dropdown->button);
  uiWidgetSetMarginTop (&dropdown->button, uiBaseMarginSz);
  uiWidgetSetMarginStart (&dropdown->button, uiBaseMarginSz);
}


static void
uiDropDownWindowCreate (uidropdown_t *dropdown,
    void *processSelectionCallback, void *udata)
{
  UIWidget          uiwidget;
  UIWidget          vbox;
  UIWidget          uiscwin;


  uiutilsUICallbackInit (&dropdown->closecb, uiDropDownClose, dropdown);
  uiCreateDialogWindow (&dropdown->window, dropdown->parentwin,
      &dropdown->button, &dropdown->closecb, "");

  uiCreateVertBox (&uiwidget);
  uiWidgetExpandHoriz (&uiwidget);
  uiWidgetExpandVert (&uiwidget);
  uiBoxPackInWindow (&dropdown->window, &uiwidget);

  uiCreateVertBox (&vbox);
  uiBoxPackStartExpand (&uiwidget, &vbox);

  uiCreateScrolledWindow (&uiscwin, 300);
  uiWidgetExpandHoriz (&uiscwin);
  uiBoxPackStartExpand (&vbox, &uiscwin);

  dropdown->tree = gtk_tree_view_new ();
  assert (dropdown->tree != NULL);
  if (G_IS_OBJECT (dropdown->tree)) {
    g_object_ref_sink (G_OBJECT (dropdown->tree));
  }
  dropdown->sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dropdown->tree));
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (dropdown->tree), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dropdown->tree), FALSE);
  gtk_tree_selection_set_mode (dropdown->sel, GTK_SELECTION_SINGLE);
  gtk_widget_set_hexpand (dropdown->tree, TRUE);
  gtk_widget_set_vexpand (dropdown->tree, TRUE);
  uiBoxPackInWindowUW (&uiscwin, dropdown->tree);
  g_signal_connect (dropdown->tree, "row-activated",
      G_CALLBACK (processSelectionCallback), udata);
}

static void
uiDropDownSelectionSet (uidropdown_t *dropdown, nlistidx_t internalidx)
{
  GtkTreePath   *path;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  char          tbuff [200];
  char          *p;


  if (dropdown == NULL) {
    return;
  }
  if (dropdown->tree == NULL) {
    return;
  }

  if (internalidx < 0) {
    internalidx = 0;
  }
  snprintf (tbuff, sizeof (tbuff), "%d", internalidx);
  path = gtk_tree_path_new_from_string (tbuff);
  if (path != NULL) {
    gtk_tree_selection_select_path (dropdown->sel, path);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (dropdown->tree));
    if (model != NULL) {
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter, UIUTILS_DROPDOWN_COL_DISP, &p, -1);
      if (p != NULL) {
        snprintf (tbuff, sizeof (tbuff), "%-*s",
            dropdown->maxwidth, p);
        uiButtonSetText (&dropdown->button, tbuff);
      }
    }
  }
}

/* these routines will be removed at a later date */

nlistidx_t
uiDropDownSelectionGetW (uidropdown_t *dropdown, GtkTreePath *path)
{
  GtkTreeIter   iter;
  GtkTreeModel  *model = NULL;
  long          idx = 0;
  int32_t       idx32;
  nlistidx_t    retval;
  char          tbuff [200];

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dropdown->tree));
  if (gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_model_get (model, &iter, UIUTILS_DROPDOWN_COL_IDX, &idx, -1);
    /* despite the model using an unsigned long, setting it to -1 */
    /* sets it to a 32-bit value */
    /* want the special -1 index (all items for combobox) */
    idx32 = (int32_t) idx;
    retval = idx32;
    if (dropdown->strSelection != NULL) {
      free (dropdown->strSelection);
    }
    gtk_tree_model_get (model, &iter, UIUTILS_DROPDOWN_COL_STR,
        &dropdown->strSelection, -1);
    if (dropdown->iscombobox) {
      char  *p;

      gtk_tree_model_get (model, &iter, UIUTILS_DROPDOWN_COL_DISP, &p, -1);
      snprintf (tbuff, sizeof (tbuff), "%-*s",
          dropdown->maxwidth, p);
      uiButtonSetText (&dropdown->button, tbuff);
      free (p);
    }
    uiWidgetHide (&dropdown->window);
    dropdown->open = false;
  } else {
    return -1;
  }

  return retval;
}

