#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "uiutils.h"

static void uiMenuActivateHandler (GtkMenuItem *mi, gpointer udata);
static void uiMenuToggleHandler (GtkWidget *mi, gpointer udata);

void
uiCreateMenubar (UIWidget *uiwidget)
{
  GtkWidget *menubar;

  menubar = gtk_menu_bar_new ();
  uiwidget->widget = menubar;
}

void
uiCreateSubMenu (UIWidget *uimenuitem, UIWidget *uimenu)
{
  GtkWidget *menu;

  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (uimenuitem->widget), menu);
  uimenu->widget = menu;
}

void
uiMenuCreateItem (UIWidget *uimenu, UIWidget *uimenuitem,
    const char *txt, UICallback *uicb)
{
  GtkWidget *menuitem;

  menuitem = gtk_menu_item_new_with_label (txt);
  gtk_menu_shell_append (GTK_MENU_SHELL (uimenu->widget), menuitem);
  if (uicb != NULL) {
    g_signal_connect (menuitem, "activate",
        G_CALLBACK (uiMenuActivateHandler), uicb);
  }
  uimenuitem->widget = menuitem;
}

void
uiMenuCreateCheckbox (UIWidget *uimenu, UIWidget *uimenuitem,
    const char *txt, int active, UICallback *uicb)
{
  GtkWidget *menuitem;

  menuitem = gtk_check_menu_item_new_with_label (txt);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), active);
  gtk_menu_shell_append (GTK_MENU_SHELL (uimenu->widget), menuitem);
  if (uicb != NULL) {
    g_signal_connect (menuitem, "toggled",
        G_CALLBACK (uiMenuToggleHandler), uicb);
  }
  uimenuitem->widget = menuitem;
}

void
uiMenuInit (uimenu_t *menu)
{
  menu->initialized = false;
  menu->menucount = 0;
  for (int i = 0; i < UIUTILS_MENU_MAX; ++i) {
    uiutilsUIWidgetInit (&menu->menuitem [i]);
  }
}

void
uiMenuAddMainItem (UIWidget *uimenubar, UIWidget *uimenuitem,
    uimenu_t *menu, const char *txt)
{
  int   i;

  if (menu->menucount >= UIUTILS_MENU_MAX) {
    return;
  }

  i = menu->menucount;
  ++menu->menucount;
  uimenuitem->widget = gtk_menu_item_new_with_label (txt);
  gtk_menu_shell_append (GTK_MENU_SHELL (uimenubar->widget),
      uimenuitem->widget);
  uiWidgetHide (uimenuitem);
  uiutilsUIWidgetCopy (&menu->menuitem [i], uimenuitem);
}

void
uiMenuDisplay (uimenu_t *menu)
{
  for (int i = 0; i < menu->menucount; ++i) {
    uiWidgetShowAll (&menu->menuitem [i]);
  }
}

void
uiMenuClear (uimenu_t *menu)
{
  for (int i = 0; i < menu->menucount; ++i) {
    uiWidgetHide (&menu->menuitem [i]);
  }
}

static void
uiMenuActivateHandler (GtkMenuItem *mi, gpointer udata)
{
  UICallback    *uicb = udata;
  uiutilsCallbackHandler (uicb);
}

static void
uiMenuToggleHandler (GtkWidget *mi, gpointer udata)
{
  UICallback    *uicb = udata;
  uiutilsCallbackHandler (uicb);
}
