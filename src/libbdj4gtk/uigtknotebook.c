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

#include "log.h"
#include "uiutils.h"

GtkWidget *
uiutilsCreateNotebook (void)
{
  GtkWidget   *widget;

  widget = gtk_notebook_new ();
  assert (widget != NULL);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), TRUE);
  gtk_widget_set_margin_top (widget, 4);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, FALSE);
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (widget), GTK_POS_TOP);
  uiutilsSetCss (widget,
      "notebook tab:checked { background-color: shade(@theme_base_color,0.6); }");
  return widget;
}

void
uiutilsNotebookAppendPage (GtkWidget *notebook, GtkWidget *widget,
    GtkWidget *labelwidget)
{
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, labelwidget);
}

void
uiutilsNotebookSetActionWidget (GtkWidget *notebook, GtkWidget *widget, GtkPackType pack)
{
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), widget, pack);
}
