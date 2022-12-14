#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>

#include "ui.h"

/* widget interface */

inline void
uiWidgetDisable (UIWidget *uiwidget)
{
  if (uiwidget == NULL || uiwidget->widget == NULL) {
    return;
  }
  gtk_widget_set_sensitive (uiwidget->widget, FALSE);
}

inline void
uiWidgetEnable (UIWidget *uiwidget)
{
  if (uiwidget == NULL || uiwidget->widget == NULL) {
    return;
  }
  gtk_widget_set_sensitive (uiwidget->widget, TRUE);
}

inline void
uiWidgetExpandHoriz (UIWidget *uiwidget)
{
  gtk_widget_set_hexpand (uiwidget->widget, TRUE);
}

inline void
uiWidgetExpandVert (UIWidget *uiwidget)
{
  gtk_widget_set_vexpand (uiwidget->widget, TRUE);
}

inline void
uiWidgetSetAllMargins (UIWidget *uiwidget, int margin)
{
  gtk_widget_set_margin_top (uiwidget->widget, margin);
  gtk_widget_set_margin_bottom (uiwidget->widget, margin);
  gtk_widget_set_margin_start (uiwidget->widget, margin);
  gtk_widget_set_margin_end (uiwidget->widget, margin);
}

inline void
uiWidgetSetMarginTop (UIWidget *uiwidget, int margin)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_margin_top (uiwidget->widget, margin);
}

inline void
uiWidgetSetMarginBottom (UIWidget *uiwidget, int margin)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_margin_bottom (uiwidget->widget, margin);
}

inline void
uiWidgetSetMarginStart (UIWidget *uiwidget, int margin)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_margin_start (uiwidget->widget, margin);
}


inline void
uiWidgetSetMarginEnd (UIWidget *uiwidget, int margin)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_margin_end (uiwidget->widget, margin);
}

inline void
uiWidgetAlignHorizFill (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_halign (uiwidget->widget, GTK_ALIGN_FILL);
}

inline void
uiWidgetAlignHorizStart (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_halign (uiwidget->widget, GTK_ALIGN_START);
}

inline void
uiWidgetAlignHorizEnd (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_halign (uiwidget->widget, GTK_ALIGN_END);
}

inline void
uiWidgetAlignHorizCenter (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_halign (uiwidget->widget, GTK_ALIGN_CENTER);
}

inline void
uiWidgetAlignVertFill (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_valign (uiwidget->widget, GTK_ALIGN_FILL);
}

inline void
uiWidgetAlignVertStart (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_valign (uiwidget->widget, GTK_ALIGN_START);
}

inline void
uiWidgetAlignVertCenter (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_valign (uiwidget->widget, GTK_ALIGN_CENTER);
}

inline void
uiWidgetDisableFocus (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_can_focus (uiwidget->widget, FALSE);
}

inline void
uiWidgetHide (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_hide (uiwidget->widget);
}

inline void
uiWidgetShow (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_show (uiwidget->widget);
}

inline void
uiWidgetShowAll (UIWidget *uiwidget)
{
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_show_all (uiwidget->widget);
}


inline void
uiWidgetMakePersistent (UIWidget *uiwidget)
{
  if (uiwidget == NULL) {
    return;
  }
  if (uiwidget->widget == NULL) {
    return;
  }

  if (G_IS_OBJECT (uiwidget->widget)) {
    g_object_ref_sink (G_OBJECT (uiwidget->widget));
  }
}

inline void
uiWidgetClearPersistent (UIWidget *uiwidget)
{
  if (uiwidget == NULL) {
    return;
  }
  if (uiwidget->widget == NULL) {
    return;
  }

  if (G_IS_OBJECT (uiwidget->widget)) {
    g_object_unref (G_OBJECT (uiwidget->widget));
  }
}

inline void
uiWidgetSetSizeRequest (UIWidget *uiwidget, int width, int height)
{
  if (uiwidget == NULL) {
    return;
  }
  if (uiwidget->widget == NULL) {
    return;
  }

  gtk_widget_set_size_request (uiwidget->widget, width, height);
}

inline bool
uiWidgetIsValid (UIWidget *uiwidget)
{
  bool    rc = false;
  if (uiwidget->widget != NULL) {
    rc = true;
  }
  return rc;
}

void
uiWidgetGetPosition (UIWidget *uiwidget, int *x, int *y)
{
  GtkAllocation alloc;

  gtk_widget_get_allocation (uiwidget->widget, &alloc);
  *x = alloc.x;
  *y = alloc.y;
}

/* these routines will be removed at a later date */

inline void
uiWidgetExpandHorizW (GtkWidget *widget)
{
  gtk_widget_set_hexpand (widget, TRUE);
}

inline void
uiWidgetExpandVertW (GtkWidget *widget)
{
  gtk_widget_set_vexpand (widget, TRUE);
}

inline void
uiWidgetSetAllMarginsW (GtkWidget *widget, int margin)
{
  gtk_widget_set_margin_top (widget, margin);
  gtk_widget_set_margin_bottom (widget, margin);
  gtk_widget_set_margin_start (widget, margin);
  gtk_widget_set_margin_end (widget, margin);
}

inline void
uiWidgetSetMarginStartW (GtkWidget *widget, int margin)
{
  if (widget == NULL) {
    return;
  }

  gtk_widget_set_margin_start (widget, margin);
}


inline void
uiWidgetAlignHorizFillW (GtkWidget *widget)
{
  if (widget == NULL) {
    return;
  }

  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
}


inline void
uiWidgetAlignVertFillW (GtkWidget *widget)
{
  if (widget == NULL) {
    return;
  }

  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
}
