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
uiutilsCreateSwitch (int value)
{
  GtkWidget   *widget;

  logProcBegin (LOG_PROC, "uiutilsCreateSwitch");

  widget = gtk_switch_new ();
  assert (widget != NULL);
  gtk_switch_set_active (GTK_SWITCH (widget), value);
  gtk_widget_set_margin_top (widget, 2);
  gtk_widget_set_margin_start (widget, 2);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  logProcEnd (LOG_PROC, "uiutilsCreateSwitch", "");
  return widget;
}

void
uiutilsSwitchSetValue (GtkWidget *w, int value)
{
  if (w == NULL) {
    return;
  }
  gtk_switch_set_active (GTK_SWITCH (w), value);
}