#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>

#include "log.h"
#include "uiutils.h"

GtkWidget *
uiutilsCreateLabel (const char *label)
{
  GtkWidget *widget;

  logProcBegin (LOG_PROC, "uiutilsCreateLabel");

  widget = gtk_label_new (label);
  assert (widget != NULL);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_widget_set_margin_top (widget, 2);
  gtk_widget_set_margin_start (widget, 2);
  logProcEnd (LOG_PROC, "uiutilsCreateLabel", "");
  return widget;
}

GtkWidget *
uiutilsCreateColonLabel (const char *label)
{
  GtkWidget *widget;
  char      tbuff [100];

  logProcBegin (LOG_PROC, "uiutilsCreateColonLabel");

  snprintf (tbuff, sizeof (tbuff), "%s:", label);
  widget = gtk_label_new (tbuff);
  assert (widget != NULL);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_widget_set_margin_top (widget, 2);
  gtk_widget_set_margin_start (widget, 2);
  logProcEnd (LOG_PROC, "uiutilsCreateColonLabel", "");
  return widget;
}

void
uiutilsLabelSetText (GtkWidget *widget, const char *text)
{
  gtk_label_set_text (GTK_LABEL (widget), text);
}

void
uiutilsLabelSetEllipsizeOn (GtkWidget *widget)
{
  gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
}
