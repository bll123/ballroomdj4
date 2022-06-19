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

#include "ui.h"

typedef struct uitextbox {
  UIWidget      scw;
  UIWidget      textbox;
  UIWidget      buffer;
} uitextbox_t;

uitextbox_t *
uiTextBoxCreate (int height)
{
  uitextbox_t   *tb;

  tb = malloc (sizeof (uitextbox_t));
  assert (tb != NULL);
  uiutilsUIWidgetInit (&tb->scw);
  uiutilsUIWidgetInit (&tb->textbox);
  uiutilsUIWidgetInit (&tb->buffer);

  uiCreateScrolledWindow (&tb->scw, height);

  tb->buffer.buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_create_tag (tb->buffer.buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
  tb->textbox.widget = gtk_text_view_new_with_buffer (tb->buffer.buffer);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tb->textbox.widget), GTK_WRAP_WORD);
  uiWidgetSetAllMargins (&tb->textbox, uiBaseMarginSz * 2);
  uiWidgetSetSizeRequest (&tb->textbox, -1, -1);
  uiWidgetAlignVertStart (&tb->textbox);
  uiBoxPackInWindow (&tb->scw, &tb->textbox);

  return tb;
}

void
uiTextBoxFree (uitextbox_t *tb)
{
  if (tb != NULL) {
    free (tb);
  }
}

UIWidget *
uiTextBoxGetScrolledWindow (uitextbox_t *tb)
{
  return &tb->scw;
}

inline void
uiTextBoxSetReadonly (uitextbox_t *tb)
{
  uiWidgetDisableFocus (&tb->textbox);
}

char *
uiTextBoxGetValue (uitextbox_t *tb)
{
  GtkTextIter   siter;
  GtkTextIter   eiter;
  char          *val;

  gtk_text_buffer_get_start_iter (tb->buffer.buffer, &siter);
  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  val = gtk_text_buffer_get_text (tb->buffer.buffer, &siter, &eiter, FALSE);
  return val;
}

void
uiTextBoxScrollToEnd (uitextbox_t *tb)
{
  GtkTextIter iter;

  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &iter);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (tb->textbox.widget),
      &iter, 0, false, 0, 0);
}

void
uiTextBoxAppendStr (uitextbox_t *tb, const char *str)
{
  GtkTextIter eiter;

  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  gtk_text_buffer_insert (tb->buffer.buffer, &eiter, str, -1);
}

void
uiTextBoxAppendBoldStr (uitextbox_t *tb, const char *str)
{
  GtkTextIter siter;
  GtkTextIter eiter;
  GtkTextMark *mark;

  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  mark = gtk_text_buffer_create_mark (tb->buffer.buffer, NULL, &eiter, TRUE);
  gtk_text_buffer_insert (tb->buffer.buffer, &eiter, str, -1);
  gtk_text_buffer_get_iter_at_mark (tb->buffer.buffer, &siter, mark);
  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  gtk_text_buffer_apply_tag_by_name (tb->buffer.buffer, "bold", &siter, &eiter);
  gtk_text_buffer_delete_mark (tb->buffer.buffer, mark);
}

void
uiTextBoxSetValue (uitextbox_t *tb, const char *str)
{
  GtkTextIter siter;
  GtkTextIter eiter;

  gtk_text_buffer_get_start_iter (tb->buffer.buffer, &siter);
  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  gtk_text_buffer_delete (tb->buffer.buffer, &siter, &eiter);
  gtk_text_buffer_get_end_iter (tb->buffer.buffer, &eiter);
  gtk_text_buffer_insert (tb->buffer.buffer, &eiter, str, -1);
}

/* this does not handle any selected text */

void
uiTextBoxDarken (uitextbox_t *tb)
{
  uiSetCss (tb->textbox.widget,
      "textview text { background-color: shade(@theme_base_color,0.8); } ");
}

void
uiTextBoxHorizExpand (uitextbox_t *tb)
{
  uiWidgetExpandHoriz (&tb->scw);
  uiWidgetAlignHorizFill (&tb->textbox);
  uiWidgetExpandHoriz (&tb->textbox);
}

void
uiTextBoxVertExpand (uitextbox_t *tb)
{
  uiWidgetExpandVert (&tb->scw);
  uiWidgetAlignVertFill (&tb->textbox);
  uiWidgetExpandVert (&tb->textbox);
}

void
uiTextBoxSetHeight (uitextbox_t *tb, int h)
{
  uiWidgetSetSizeRequest (&tb->textbox, -1, h);
}

