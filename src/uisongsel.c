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

#include "bdj4intl.h"
#include "bdjvarsdf.h"
#include "conn.h"
#include "dance.h"
#include "musicdb.h"
#include "pathbld.h"
#include "portability.h"
#include "progstate.h"
#include "tagdef.h"
#include "uisongsel.h"
#include "uiutils.h"

enum {
  SONGSEL_COL_IDX,
  SONGSEL_COL_DBIDX,
  SONGSEL_COL_DANCE,
  SONGSEL_COL_TITLE,
  SONGSEL_COL_ARTIST,
  SONGSEL_COL_FAV_COLOR,
  SONGSEL_COL_FAVORITE,
  SONGSEL_COL_MAX,
};

static void uisongselInitializeStore (uisongsel_t *uisongsel);
static void uisongselCreateRows (uisongsel_t *uisongsel);
static void uisongselPopulateData (uisongsel_t *uisongsel);
static void uisongselProcessTreeSize (GtkWidget* w, GtkAllocation* allocation,
    gpointer user_data);
static gboolean uisongselScroll (GtkRange *range, GtkScrollType scrolltype,
    gdouble value, gpointer udata);
static void uisongselRowSelected (GtkTreeView* tv, GtkTreePath* path,
    GtkTreeViewColumn* column, gpointer udata);
static gboolean uisongselScrollEvent (GtkWidget* tv, GdkEventScroll *event,
    gpointer udata);

uisongsel_t *
uisongselInit (progstate_t *progstate, conn_t *conn)
{
  uisongsel_t    *uisongsel;

  uisongsel = malloc (sizeof (uisongsel_t));
  assert (uisongsel != NULL);
  uisongsel->progstate = progstate;
  uisongsel->conn = conn;
  uisongsel->maxRows = 0;
  uisongsel->idxStart = 0;
  uisongsel->createRowProcessFlag = false;
  uisongsel->createRowFlag = false;
  uisongsel->ddbcount = (double) dbCount ();
  uisongsel->lastTreeSize = 0;

  return uisongsel;
}

void
uisongselFree (uisongsel_t *uisongsel)
{
  if (uisongsel != NULL) {
    free (uisongsel);
  }
}

GtkWidget *
uisongselActivate (uisongsel_t *uisongsel)
{
  GtkWidget         *hbox;
  GtkWidget         *vbox;
  GtkWidget         *widget;
  GtkWidget         *twidget;
  GtkTreeSelection  *sel;
  GtkCellRenderer   *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  GtkAdjustment     *adjustment;
  GValue            gvalue = G_VALUE_INIT;
  double            dval;

  g_value_init (&gvalue, G_TYPE_INT);
  g_value_set_int (&gvalue, PANGO_ELLIPSIZE_END);

  uisongsel->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  assert (uisongsel->vbox != NULL);
  gtk_widget_set_hexpand (GTK_WIDGET (uisongsel->vbox), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (uisongsel->vbox), TRUE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  assert (hbox != NULL);
  gtk_widget_set_hexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (uisongsel->vbox), GTK_WIDGET (hbox),
      FALSE, FALSE, 0);

  widget = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (widget), _("Queue"));
  assert (widget != NULL);
  gtk_widget_set_margin_start (GTK_WIDGET (widget), 2);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (widget),
      FALSE, FALSE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  assert (hbox != NULL);
  gtk_widget_set_vexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (uisongsel->vbox), GTK_WIDGET (hbox),
      TRUE, TRUE, 0);

  adjustment = gtk_adjustment_new (0.0, 0.0, uisongsel->ddbcount, 1.0, 10.0, 10.0);
  uisongsel->songselScrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adjustment);
  assert (uisongsel->songselScrollbar != NULL);
  gtk_widget_set_vexpand (uisongsel->songselScrollbar, TRUE);
  uiutilsSetCss (uisongsel->songselScrollbar,
      "scrollbar, scrollbar slider { min-width: 8px; } ");
  gtk_box_pack_end (GTK_BOX (hbox), uisongsel->songselScrollbar,
      FALSE, FALSE, 0);
  g_signal_connect (uisongsel->songselScrollbar, "change-value",
      G_CALLBACK (uisongselScroll), uisongsel);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  assert (vbox != NULL);
  gtk_widget_set_vexpand (hbox, TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  widget = gtk_scrolled_window_new (NULL, NULL);
  g_object_set (widget, "overlay-scrolling", FALSE, NULL);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (widget), FALSE);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (widget), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (widget), TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (widget),
      FALSE, FALSE, 0);

  uisongsel->songselTree = gtk_tree_view_new ();
  assert (uisongsel->songselTree != NULL);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (uisongsel->songselTree), FALSE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (uisongsel->songselTree), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (uisongsel->songselTree), TRUE);
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (uisongsel->songselTree));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_widget_set_margin_start (uisongsel->songselTree, 4);
  gtk_widget_set_margin_end (uisongsel->songselTree, 4);
  gtk_widget_set_margin_top (uisongsel->songselTree, 2);
  gtk_widget_set_margin_bottom (uisongsel->songselTree, 4);
  gtk_widget_set_hexpand (uisongsel->songselTree, TRUE);
  gtk_widget_set_vexpand (uisongsel->songselTree, FALSE);
  adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (uisongsel->songselTree));
  gtk_adjustment_set_upper (adjustment, uisongsel->ddbcount);
  uisongsel->scrollController =
      gtk_event_controller_scroll_new (uisongsel->songselTree,
      GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
      GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
//  gtk_widget_add_events (uisongsel->songselTree, GDK_SCROLL_MASK);
  gtk_container_add (GTK_CONTAINER (widget), uisongsel->songselTree);
  g_signal_connect (uisongsel->songselTree, "size-allocate",
      G_CALLBACK (uisongselProcessTreeSize), uisongsel);
  g_signal_connect (uisongsel->songselTree, "row-activated",
      G_CALLBACK (uisongselRowSelected), uisongsel);
  g_signal_connect (uisongsel->songselTree, "scroll-event",
      G_CALLBACK (uisongselScrollEvent), uisongsel);

  /* this box pushes the tree view up to the top */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  assert (hbox != NULL);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_widget_set_vexpand (hbox, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox),
      TRUE, TRUE, 0);

  gtk_event_controller_scroll_new (uisongsel->songselTree,
      GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", SONGSEL_COL_DANCE, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_column_set_title (column, _("Dance"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (uisongsel->songselTree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", SONGSEL_COL_TITLE, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_fixed_width (column, 400);
  gtk_tree_view_column_set_expand (column, TRUE);
  g_object_set_property (G_OBJECT (renderer), "ellipsize", &gvalue);
  gtk_tree_view_column_set_title (column, _("Title"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (uisongsel->songselTree), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "text", SONGSEL_COL_ARTIST, NULL);
  gtk_tree_view_column_set_fixed_width (column, 250);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  g_object_set_property (G_OBJECT (renderer), "ellipsize", &gvalue);
  gtk_tree_view_column_set_title (column, _("Artist"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (uisongsel->songselTree), column);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_alignment (renderer, 0.5, 0.5);
  column = gtk_tree_view_column_new_with_attributes ("",
      renderer, "markup", SONGSEL_COL_FAVORITE,
      "foreground", SONGSEL_COL_FAV_COLOR, NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_column_set_title (column, "\xE2\x98\x85");
  gtk_tree_view_append_column (GTK_TREE_VIEW (uisongsel->songselTree), column);
  uisongsel->favColumn = column;

  uisongselInitializeStore (uisongsel);
  uisongselCreateRows (uisongsel);

//  g_object_set (gtk_widget_get_settings (uisongsel->songselTree),
//      "gtk-enable-animations", FALSE, NULL);

  return uisongsel->vbox;
}

void
uisongselMainLoop (uisongsel_t *uisongsel)
{
  return;
}

/* internal routines */

static void
uisongselInitializeStore (uisongsel_t *uisongsel)
{
  GtkTreeIter       iter;
  GtkListStore      *store = NULL;

  store = gtk_list_store_new (SONGSEL_COL_MAX,
      G_TYPE_ULONG, G_TYPE_ULONG,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING);
  assert (store != NULL);

  gtk_tree_view_set_model (GTK_TREE_VIEW (uisongsel->songselTree), GTK_TREE_MODEL (store));
  uisongsel->createRowProcessFlag = true;
  uisongsel->createRowFlag = true;
}

static void
uisongselCreateRows (uisongsel_t *uisongsel)
{
  GtkAdjustment     *adjustment = NULL;
  GtkTreeModel      *model = NULL;
  GtkTreeIter       iter;
  int               start;
  int               end;
  char              tbuff [40];
  double            ps, si;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uisongsel->songselTree));
  /* for now, some number that should be large enough */
  for (int i = 0; i < 50; ++i) {
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  }

  uisongselPopulateData (uisongsel);
}

static void
uisongselPopulateData (uisongsel_t *uisongsel)
{
  GtkTreeModel        * model = NULL;
  GtkTreeIter         iter;
  ssize_t             idx;
  int                 count;
  song_t              * song;
  dance_t             * dances;
  ilistidx_t          danceIdx;
  char                * danceStr;
  songfavoriteinfo_t  * favorite;
  char                * color;
  char                tmp [40];
  char                tbuff [100];

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uisongsel->songselTree));

  count = 0;
  idx = uisongsel->idxStart;
  while (count < uisongsel->maxRows) {
    snprintf (tbuff, sizeof (tbuff), "%d", count);
    if (gtk_tree_model_get_iter_from_string (model, &iter, tbuff)) {
      song = dbGetByIdx (idx);
      if (song != NULL) {
        danceIdx = songGetNum (song, TAG_DANCE);
        danceStr = danceGetData (dances, danceIdx, DANCE_DANCE);
        favorite = songGetFavoriteData (song);
        color = favorite->color;
        if (strcmp (color, "") == 0) {
          GtkStyleContext   * context;
          GdkRGBA           gcolor;

          context = gtk_widget_get_style_context (uisongsel->songselTree);
          gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &gcolor);
          color = gdk_rgba_to_string (&gcolor);
          snprintf (tmp, sizeof (tmp), "#%02x%02x%02x",
              (int) round (gcolor.red * 255.0),
              (int) round (gcolor.green * 255.0),
              (int) round (gcolor.blue * 255.0));
          color = tmp;
        }
        snprintf (tbuff, sizeof (tbuff), "<span color=\"%s\">%s</span>",
            color, favorite->dispStr);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
            SONGSEL_COL_IDX, idx,
            SONGSEL_COL_DBIDX, idx,
            SONGSEL_COL_DANCE, danceStr,
            SONGSEL_COL_ARTIST, songGetData (song, TAG_ARTIST),
            SONGSEL_COL_TITLE, songGetData (song, TAG_TITLE),
            SONGSEL_COL_FAV_COLOR, color,
            SONGSEL_COL_FAVORITE, tbuff,
            -1);
      }
    }

    ++idx;
    ++count;
  }
}

static void
uisongselProcessTreeSize (GtkWidget* w, GtkAllocation* allocation,
    gpointer udata)
{
  uisongsel_t   *uisongsel = udata;
  GtkAdjustment *adjustment;
  double        ps, si;


  if (allocation->height != uisongsel->lastTreeSize) {
    /* get the page size and the step increment from the scrolled window */
    /* scrollbar; this will allow us to determine how many rows are displayed */
    adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (uisongsel->songselTree));
    ps = gtk_adjustment_get_page_size (adjustment);
    si = gtk_adjustment_get_step_increment (adjustment);
    uisongsel->maxRows = (int) (ps / si);

    if (allocation->height > 200) {
      gtk_adjustment_set_value (adjustment, 0.0);
      g_signal_emit_by_name (GTK_RANGE (uisongsel->songselScrollbar),
          "change-value", NULL, 0.0, uisongsel, NULL);
    }

    adjustment = gtk_range_get_adjustment (GTK_RANGE (uisongsel->songselScrollbar));
    /* the step increment does not work correctly */
    gtk_adjustment_set_step_increment (adjustment, 4.0);
    gtk_adjustment_set_page_increment (adjustment, (double) uisongsel->maxRows);
    gtk_adjustment_set_page_size (adjustment, (double) uisongsel->maxRows);

    uisongsel->lastTreeSize = allocation->height;
  }
}

static gboolean
uisongselScroll (GtkRange *range, GtkScrollType scrolltype,
    gdouble value, gpointer udata)
{
  uisongsel_t   *uisongsel = udata;
  double        start;

  if (value < 0 || value > uisongsel->ddbcount) {
    return TRUE;
  }

  start = floor (value);
  if (start > uisongsel->ddbcount - uisongsel->maxRows) {
    start = uisongsel->ddbcount - uisongsel->maxRows;
  }
  uisongsel->idxStart = (ssize_t) start;

  uisongselPopulateData (uisongsel);
  return FALSE;
}

static void
uisongselRowSelected (GtkTreeView* tv, GtkTreePath* path,
    GtkTreeViewColumn* column, gpointer udata)
{
  uisongsel_t       * uisongsel = udata;
  GtkTreeSelection  * sel;
  int               count;
  GtkTreeModel      * model = NULL;
  GtkTreeIter       iter;
  gulong            dbidx;
  song_t            * song;

  if (column != uisongsel->favColumn) {
    return;
  }

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (uisongsel->songselTree));
  count = gtk_tree_selection_count_selected_rows (sel);
  if (count != 1) {
    return;
  }
  gtk_tree_selection_get_selected (sel, &model, &iter);
  gtk_tree_model_get (model, &iter, SONGSEL_COL_DBIDX, &dbidx, -1);
  song = dbGetByIdx ((ssize_t) dbidx);
  songChangeFavorite (song);
  // ## TODO song data must be saved to the database.
  uisongselPopulateData (uisongsel);
}

static gboolean
uisongselScrollEvent (GtkWidget* tv, GdkEventScroll *event, gpointer udata)
{
  uisongsel_t   *uisongsel = udata;

  if (event->direction == GDK_SCROLL_SMOOTH) {
    double dx, dy;

    gdk_event_get_scroll_deltas ((GdkEvent *) event, &dx, &dy);
    if (dy < 0.0) {
      --uisongsel->idxStart;
    }
    if (dy > 0.0) {
      ++uisongsel->idxStart;
    }
  }
  if (event->direction == GDK_SCROLL_DOWN) {
    ++uisongsel->idxStart;
  }
  if (event->direction == GDK_SCROLL_UP) {
    --uisongsel->idxStart;
  }
  g_signal_emit_by_name (GTK_RANGE (uisongsel->songselScrollbar),
      "change-value", NULL, (double) uisongsel->idxStart, uisongsel, NULL);

  return TRUE;
}
