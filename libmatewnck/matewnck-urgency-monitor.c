/* vim: set sw=2 et: */
/*
 * Copyright (C) 2009 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libmatewnck/libmatewnck.h>

static void
status_icon_activated (GtkStatusIcon *icon,
                       MateWnckWindow    *window)
{
  MateWnckWorkspace *workspace;
  guint32 timestamp;

  /* We're in an activate callback, so gtk_get_current_time() works... */
  timestamp = gtk_get_current_event_time ();

  /* FIXME: THIS IS SICK AND WRONG AND BUGGY.  See the end of
   * http://mail.gnome.org/archives/wm-spec-list/2005-July/msg00032.html
   * There should only be *one* activate call.
   */
  workspace = matewnck_window_get_workspace (window);
  if (workspace)
    matewnck_workspace_activate (workspace, timestamp);

  matewnck_window_activate (window, timestamp);
}

static GtkStatusIcon *
status_icon_get (MateWnckWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "matewnck-urgency-icon");
}

static void
status_icon_update (MateWnckWindow *window)
{
  GtkStatusIcon *icon;

  icon = status_icon_get (window);

  if (icon == NULL)
    {
      return;
    }

  if (matewnck_window_get_icon_is_fallback (window))
    {
      gtk_status_icon_set_from_icon_name (icon, "dialog-information");
    }
  else
    {
      gtk_status_icon_set_from_pixbuf (icon,
                                       matewnck_window_get_mini_icon (window));
    }

    gtk_status_icon_set_tooltip_text (icon, matewnck_window_get_name (window));
}

static void
status_icon_create (MateWnckWindow *window)
{
  GtkStatusIcon *icon;

  icon = gtk_status_icon_new ();
  g_object_set_data (G_OBJECT (window), "matewnck-urgency-icon", icon);

  g_signal_connect (icon, "activate",
                    G_CALLBACK (status_icon_activated), window);

  status_icon_update (window);
}

static void
status_icon_remove (MateWnckWindow *window)
{
  GtkStatusIcon *icon;

  icon = status_icon_get (window);
  if (icon != NULL)
    {
      gtk_status_icon_set_visible (icon, FALSE);
      g_object_unref (icon);
      g_object_set_data (G_OBJECT (window), "matewnck-urgency-icon", NULL);
    }
}

static void
window_state_changed (MateWnckWindow      *window,
                      MateWnckWindowState  changed_mask,
                      MateWnckWindowState  new_state,
                      gpointer         data)
{
  GtkStatusIcon *icon;

  if (!
      (changed_mask &
       (MATEWNCK_WINDOW_STATE_DEMANDS_ATTENTION |
        MATEWNCK_WINDOW_STATE_URGENT)))
    return;

  icon = status_icon_get (window);

  if (matewnck_window_or_transient_needs_attention (window))
    {
      if (icon == NULL)
        {
          status_icon_create (window);
        }
    }
  else
    {
      status_icon_remove (window);
    }
}

static void
window_icon_changed (MateWnckWindow *window,
                     gpointer    data)
{
  status_icon_update (window);
}

static void
window_name_changed (MateWnckWindow *window,
                     gpointer    data)
{
  status_icon_update (window);
}

static void
connect_to_window (MateWnckScreen *screen,
                   MateWnckWindow *window)
{
  if (matewnck_window_or_transient_needs_attention (window))
    {
      status_icon_create (window);
    }

  g_signal_connect (window, "state_changed",
                    G_CALLBACK (window_state_changed), NULL);
  g_signal_connect (window, "icon_changed",
                    G_CALLBACK (window_icon_changed), NULL);
  g_signal_connect (window, "name_changed",
                    G_CALLBACK (window_name_changed), NULL);
}

static void
disconnect_from_window (MateWnckScreen *screen,
                        MateWnckWindow *window)
{
  status_icon_remove (window);
}

int
main (int argc, char **argv)
{
  GOptionContext *ctxt;
  GError         *error;
  MateWnckScreen     *screen;

  ctxt = g_option_context_new (NULL);
  g_option_context_set_summary (ctxt, "Monitor windows with the urgency hint "
                                      "set, and display a notification icon "
                                      "for each of them.");
  g_option_context_add_group (ctxt, gtk_get_option_group (TRUE));

  error = NULL;
  if (!g_option_context_parse (ctxt, &argc, &argv, &error))
    {
      g_printerr ("Error while parsing arguments: %s\n", error->message);
      g_option_context_free (ctxt);
      g_error_free (error);
      return 1;
    }

  g_option_context_free (ctxt);
  ctxt = NULL;

  gtk_init (&argc, &argv);

  matewnck_set_client_type (MATEWNCK_CLIENT_TYPE_PAGER);

  screen = matewnck_screen_get_default ();
  g_signal_connect (screen, "window_opened",
                    G_CALLBACK (connect_to_window), NULL);
  g_signal_connect (screen, "window_closed",
                    G_CALLBACK (disconnect_from_window), NULL);

  gtk_main ();

  return 0;
}