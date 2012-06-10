/* util header */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef MATEWNCK_DISABLE_DEPRECATED

#include <config.h>

#include <glib/gi18n-lib.h>
#include "util.h"
#include "xutils.h"
#include "private.h"
#include <gdk/gdkx.h>
#include <string.h>
#ifdef HAVE_XRES
#include <X11/extensions/XRes.h>
#endif

/**
 * SECTION:resource
 * @short_description: reading resource usage of X clients.
 * @see_also: matewnck_window_get_xid(), matewnck_application_get_xid(), matewnck_window_get_pid(), matewnck_application_get_pid()
 * @stability: Unstable
 *
 * libmatewnck provides an easy-to-use interface to the XRes X server extension to
 * read resource usage of X clients, which can be defined either by the X
 * window ID of one of their windows or by the process ID of their process.
 */

/**
 * SECTION:misc
 * @short_description: other additional features.
 * @stability: Unstable
 *
 * These functions are utility functions providing some additional features to
 * libwcnk users.
 */

/**
 * matewnck_gtk_window_set_dock_type:
 * @window: a <classname>GtkWindow</classname>.
 *
 * Sets the semantic type of @window to %MATEWNCK_WINDOW_DOCK.
 *
 * Deprecated:2.20: Use gdk_window_set_type_hint() instead.
 */
void
matewnck_gtk_window_set_dock_type (GtkWindow *window)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  gdk_window_set_type_hint (gtk_widget_get_window (GTK_WIDGET (window)),
		  	    GDK_WINDOW_TYPE_HINT_DOCK);
}

typedef enum
{
  MATEWNCK_EXT_UNKNOWN = 0,
  MATEWNCK_EXT_FOUND = 1,
  MATEWNCK_EXT_MISSING = 2
} MatewnckExtStatus;


#if 0
/* useful for debugging */
static void
_matewnck_print_resource_usage (MatewnckResourceUsage *usage)
{
  if (!usage)
    return;

  g_print ("\twindows       : %d\n"
           "\tGCs           : %d\n"
           "\tfonts         : %d\n"
           "\tpixmaps       : %d\n"
           "\tpictures      : %d\n"
           "\tglyphsets     : %d\n"
           "\tcolormaps     : %d\n"
           "\tpassive grabs : %d\n"
           "\tcursors       : %d\n"
           "\tunknowns      : %d\n"
           "\tpixmap bytes  : %ld\n"
           "\ttotal bytes   : ~%ld\n",
           usage->n_windows,
           usage->n_gcs,
           usage->n_fonts,
           usage->n_pixmaps,
           usage->n_pictures,
           usage->n_glyphsets,
           usage->n_colormap_entries,
           usage->n_passive_grabs,
           usage->n_cursors,
           usage->n_other,
           usage->pixmap_bytes,
           usage->total_bytes_estimate);
}
#endif

static MatewnckExtStatus
matewnck_init_resource_usage (GdkDisplay *gdisplay)
{
  int event, error;
  Display *xdisplay;
  MatewnckExtStatus status;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gdisplay),
                                               "matewnck-xres-status"));

  if (status == MATEWNCK_EXT_UNKNOWN)
    {
#ifdef HAVE_XRES
      if (!XResQueryExtension (xdisplay, &event, &error))
        status = MATEWNCK_EXT_MISSING;
      else
        status = MATEWNCK_EXT_FOUND;
#else
      status = MATEWNCK_EXT_MISSING;
#endif

      g_object_set_data (G_OBJECT (gdisplay),
                         "matewnck-xres-status",
                         GINT_TO_POINTER (status));
    }

  g_assert (status != MATEWNCK_EXT_UNKNOWN);

  return status;
}

/**
 * matewnck_xid_read_resource_usage:
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @xid: an X window ID.
 * @usage: return location for the X resource usage of the application owning
 * the X window ID @xid.
 *
 * Looks for the X resource usage of the application owning the X window ID
 * @xid on display @gdisplay. If no resource usage can be found, then all
 * fields of @usage are set to 0.
 *
 * To properly work, this function requires the XRes extension on the X server.
 *
 * Since: 2.6
 */
void
matewnck_xid_read_resource_usage (GdkDisplay        *gdisplay,
                              gulong             xid,
                              MatewnckResourceUsage *usage)
{
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (matewnck_init_resource_usage (gdisplay) == MATEWNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
 {
   Display *xdisplay;
   XResType *types;
   int n_types;
   unsigned long pixmap_bytes;
   int i;
   Atom pixmap_atom;
   Atom window_atom;
   Atom gc_atom;
   Atom picture_atom;
   Atom glyphset_atom;
   Atom font_atom;
   Atom colormap_entry_atom;
   Atom passive_grab_atom;
   Atom cursor_atom;

   types = NULL;
   n_types = 0;
   pixmap_bytes = 0;

  _matewnck_error_trap_push ();

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  XResQueryClientResources (xdisplay,
                             xid, &n_types,
                             &types);

   XResQueryClientPixmapBytes (xdisplay,
                               xid, &pixmap_bytes);
   _matewnck_error_trap_pop ();

   usage->pixmap_bytes = pixmap_bytes;

   pixmap_atom = _matewnck_atom_get ("PIXMAP");
   window_atom = _matewnck_atom_get ("WINDOW");
   gc_atom = _matewnck_atom_get ("GC");
   font_atom = _matewnck_atom_get ("FONT");
   glyphset_atom = _matewnck_atom_get ("GLYPHSET");
   picture_atom = _matewnck_atom_get ("PICTURE");
   colormap_entry_atom = _matewnck_atom_get ("COLORMAP ENTRY");
   passive_grab_atom = _matewnck_atom_get ("PASSIVE GRAB");
   cursor_atom = _matewnck_atom_get ("CURSOR");

   i = 0;
   while (i < n_types)
     {
       int t = types[i].resource_type;

       if (t == pixmap_atom)
         usage->n_pixmaps += types[i].count;
       else if (t == window_atom)
         usage->n_windows += types[i].count;
       else if (t == gc_atom)
         usage->n_gcs += types[i].count;
       else if (t == picture_atom)
         usage->n_pictures += types[i].count;
       else if (t == glyphset_atom)
         usage->n_glyphsets += types[i].count;
       else if (t == font_atom)
         usage->n_fonts += types[i].count;
       else if (t == colormap_entry_atom)
         usage->n_colormap_entries += types[i].count;
       else if (t == passive_grab_atom)
         usage->n_passive_grabs += types[i].count;
       else if (t == cursor_atom)
         usage->n_cursors += types[i].count;
       else
         usage->n_other += types[i].count;

       ++i;
     }

   XFree(types);

   usage->total_bytes_estimate = usage->pixmap_bytes;

   /* FIXME look in the X server source and come up with better
    * answers here. Ideally we change XRes to return a number
    * like this since it can do things like divide the cost of
    * a shared resource among those sharing it.
    */
   usage->total_bytes_estimate += usage->n_windows * 24;
   usage->total_bytes_estimate += usage->n_gcs * 24;
   usage->total_bytes_estimate += usage->n_pictures * 24;
   usage->total_bytes_estimate += usage->n_glyphsets * 24;
   usage->total_bytes_estimate += usage->n_fonts * 1024;
   usage->total_bytes_estimate += usage->n_colormap_entries * 24;
   usage->total_bytes_estimate += usage->n_passive_grabs * 24;
   usage->total_bytes_estimate += usage->n_cursors * 24;
   usage->total_bytes_estimate += usage->n_other * 24;
 }
#else /* HAVE_XRES */
  g_assert_not_reached ();
#endif /* HAVE_XRES */
}

#ifdef HAVE_XRES
static void
matewnck_pid_read_resource_usage_free_hash (gpointer data)
{
  g_slice_free (gulong, data);
}

static guint
matewnck_gulong_hash (gconstpointer v)
{
  /* FIXME: this is obvioulsy wrong, but nearly 100% of the time, the gulong
   * only contains guint values */
  return *(const guint *) v;
}

static gboolean
matewnck_gulong_equal (gconstpointer a,
                   gconstpointer b)
{
  return *((const gulong *) a) == *((const gulong *) b);
}

static gulong
matewnck_check_window_for_pid (Window win,
                           XID    match_xid,
                           XID    mask)
{
  if ((win & ~mask) == match_xid) {
    return _matewnck_get_pid (win);
  }

  return 0;
}

static void
matewnck_find_pid_for_resource_r (Display *xdisplay,
                              Window   win_top,
                              XID      match_xid,
                              XID      mask,
                              gulong  *xid,
                              gulong  *pid)
{
  Status   qtres;
  int      err;
  Window   dummy;
  Window  *children;
  guint    n_children;
  int      i;
  gulong   found_pid = 0;

  while (gtk_events_pending ())
    gtk_main_iteration ();

  found_pid = matewnck_check_window_for_pid (win_top, match_xid, mask);
  if (found_pid != 0)
    {
      *xid = win_top;
      *pid = found_pid;
    }

  _matewnck_error_trap_push ();
  qtres = XQueryTree (xdisplay, win_top, &dummy, &dummy,
                      &children, &n_children);
  err = _matewnck_error_trap_pop ();

  if (!qtres || err != Success)
    return;

  for (i = 0; i < n_children; i++)
    {
      matewnck_find_pid_for_resource_r (xdisplay, children[i],
                                    match_xid, mask, xid, pid);

      if (*pid != 0)
	break;
    }

  if (children)
    XFree ((char *)children);
}

struct xresclient_state
{
  XResClient *clients;
  int         n_clients;
  int         next;
  Display    *xdisplay;
  GHashTable *hashtable_pid;
};

static struct xresclient_state xres_state = { NULL, 0, -1, NULL, NULL };
static guint       xres_idleid = 0;
static GHashTable *xres_hashtable = NULL;
static time_t      start_update = 0;
static time_t      end_update = 0;
static guint       xres_removeid = 0;

static void
matewnck_pid_read_resource_usage_xres_state_free (gpointer data)
{
  struct xresclient_state *state;

  state = (struct xresclient_state *) data;

  if (state->clients)
    XFree (state->clients);
  state->clients = NULL;

  state->n_clients = 0;
  state->next = -1;
  state->xdisplay = NULL;

  if (state->hashtable_pid)
    g_hash_table_destroy (state->hashtable_pid);
  state->hashtable_pid = NULL;
}

static gboolean
matewnck_pid_read_resource_usage_fill_cache (struct xresclient_state *state)
{
  int    i;
  gulong pid;
  gulong xid;
  XID    match_xid;

  if (state->next >= state->n_clients)
    {
      if (xres_hashtable)
        g_hash_table_destroy (xres_hashtable);
      xres_hashtable = state->hashtable_pid;
      state->hashtable_pid = NULL;

      time (&end_update);

      xres_idleid = 0;
      return FALSE;
    }

  match_xid = (state->clients[state->next].resource_base &
               ~state->clients[state->next].resource_mask);

  pid = 0;
  xid = 0;

  for (i = 0; i < ScreenCount (state->xdisplay); i++)
    {
      Window root;

      root = RootWindow (state->xdisplay, i);

      if (root == None)
        continue;

      matewnck_find_pid_for_resource_r (state->xdisplay, root, match_xid,
                                    state->clients[state->next].resource_mask,
                                    &xid, &pid);

      if (pid != 0 && xid != 0)
        break;
    }

  if (pid != 0 && xid != 0)
    {
      gulong *key;
      gulong *value;

      key = g_slice_new (gulong);
      value = g_slice_new (gulong);
      *key = pid;
      *value = xid;
      g_hash_table_insert (state->hashtable_pid, key, value);
    }

  state->next++;

  return TRUE;
}

static void
matewnck_pid_read_resource_usage_start_build_cache (GdkDisplay *gdisplay)
{
  Display *xdisplay;
  int      err;

  if (xres_idleid != 0)
    return;

  time (&start_update);

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  _matewnck_error_trap_push ();
  XResQueryClients (xdisplay, &xres_state.n_clients, &xres_state.clients);
  err = _matewnck_error_trap_pop ();

  if (err != Success)
    return;

  xres_state.next = (xres_state.n_clients > 0) ? 0 : -1;
  xres_state.xdisplay = xdisplay;
  xres_state.hashtable_pid = g_hash_table_new_full (
                                     matewnck_gulong_hash,
                                     matewnck_gulong_equal,
                                     matewnck_pid_read_resource_usage_free_hash,
                                     matewnck_pid_read_resource_usage_free_hash);

  xres_idleid = g_idle_add_full (
                        G_PRIORITY_HIGH_IDLE,
                        (GSourceFunc) matewnck_pid_read_resource_usage_fill_cache,
                        &xres_state, matewnck_pid_read_resource_usage_xres_state_free);
}

static gboolean
matewnck_pid_read_resource_usage_destroy_hash_table (gpointer data)
{
  xres_removeid = 0;

  if (xres_hashtable)
    g_hash_table_destroy (xres_hashtable);

  xres_hashtable = NULL;

  return FALSE;
}

#define XRES_UPDATE_RATE_SEC 30
static gboolean
matewnck_pid_read_resource_usage_from_cache (GdkDisplay        *gdisplay,
                                         gulong             pid,
                                         MatewnckResourceUsage *usage)
{
  gboolean  need_rebuild;
  gulong   *xid_p;
  int       cache_validity;

  if (end_update == 0)
    time (&end_update);

  cache_validity = MAX (XRES_UPDATE_RATE_SEC, (end_update - start_update) * 2);

  /* we rebuild the cache if it was never built or if it's old */
  need_rebuild = (xres_hashtable == NULL ||
                  (end_update < time (NULL) - cache_validity));

  if (xres_hashtable)
    {
      /* clear the cache after quite some time, because it might not be used
       * anymore */
      if (xres_removeid != 0)
        g_source_remove (xres_removeid);
      xres_removeid = g_timeout_add_seconds (cache_validity * 2,
                                             matewnck_pid_read_resource_usage_destroy_hash_table,
                                             NULL);
    }

  if (need_rebuild)
    matewnck_pid_read_resource_usage_start_build_cache (gdisplay);

  if (xres_hashtable)
    xid_p = g_hash_table_lookup (xres_hashtable, &pid);
  else
    xid_p = NULL;

  if (xid_p)
    {
      matewnck_xid_read_resource_usage (gdisplay, *xid_p, usage);
      return TRUE;
    }

  return FALSE;
}

static void
matewnck_pid_read_resource_usage_no_cache (GdkDisplay        *gdisplay,
                                       gulong             pid,
                                       MatewnckResourceUsage *usage)
{
  Display *xdisplay;
  int i;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdisplay);

  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      MatewnckScreen *screen;
      GList *windows;
      GList *tmp;

      screen = matewnck_screen_get (i);

      g_assert (screen != NULL);

      windows = matewnck_screen_get_windows (screen);
      tmp = windows;
      while (tmp != NULL)
        {
          if (matewnck_window_get_pid (tmp->data) == pid)
            {
              matewnck_xid_read_resource_usage (gdisplay,
                                            matewnck_window_get_xid (tmp->data),
                                            usage);

              /* stop on first window found */
              return;
            }

          tmp = tmp->next;
        }

      ++i;
    }
}
#endif /* HAVE_XRES */

/**
 * matewnck_pid_read_resource_usage:
 * @gdk_display: a <classname>GdkDisplay</classname>.
 * @pid: a process ID.
 * @usage: return location for the X resource usage of the application with
 * process ID @pid.
 *
 * Looks for the X resource usage of the application with process ID @pid on
 * display @gdisplay. If no resource usage can be found, then all fields of
 * @usage are set to 0.
 *
 * In order to find the resource usage of an application that does not have an
 * X window visible to libmatewnck (panel applets do not have any toplevel windows,
 * for example), matewnck_pid_read_resource_usage() walks through the whole tree of
 * X windows. Since this walk is expensive in CPU, a cache is created. This
 * cache is updated in the background. This means there is a non-null
 * probability that no resource usage will be found for an application, even if
 * it is an X client. If this happens, calling matewnck_pid_read_resource_usage()
 * again after a few seconds should work.
 *
 * To properly work, this function requires the XRes extension on the X server.
 *
 * Since: 2.6
 */
void
matewnck_pid_read_resource_usage (GdkDisplay        *gdisplay,
                              gulong             pid,
                              MatewnckResourceUsage *usage)
{
  g_return_if_fail (usage != NULL);

  memset (usage, '\0', sizeof (*usage));

  if (matewnck_init_resource_usage (gdisplay) == MATEWNCK_EXT_MISSING)
    return;

#ifdef HAVE_XRES
  if (!matewnck_pid_read_resource_usage_from_cache (gdisplay, pid, usage))
    /* the cache might not be built, might be outdated or might not contain
     * data for a new X client, so try to fallback to something else */
    matewnck_pid_read_resource_usage_no_cache (gdisplay, pid, usage);
#endif /* HAVE_XRES */
}

static MatewnckClientType client_type = 0;

/**
 * matewnck_set_client_type:
 * @ewmh_sourceindication_client_type: a role for the client.
 *
 * Sets the role of the libmatewnck user.
 *
 * The default role is %MATEWNCK_CLIENT_TYPE_APPLICATION. Therefore, for
 * applications providing some window management features, like pagers or
 * tasklists, it is important to set the role to %MATEWNCK_CLIENT_TYPE_PAGER for
 * libmatewnck to properly work.
 *
 * Since: 2.14
 */
void
matewnck_set_client_type (MatewnckClientType ewmh_sourceindication_client_type)
{
  /* Clients constantly switching types makes no sense; this should only be
   * set once.
   */
  if (client_type != 0)
    g_critical ("matewnck_set_client_type got called multiple times.\n");
  else
    client_type = ewmh_sourceindication_client_type;
}

MatewnckClientType
_matewnck_get_client_type (void)
{
  /* If the type hasn't been set yet, use the default--treat it as a
   * normal application.
   */
  if (client_type == 0)
    client_type = MATEWNCK_CLIENT_TYPE_APPLICATION;

  return client_type;
}

/**
 * _make_gtk_label_bold:
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/
void
_make_gtk_label_bold (GtkLabel *label)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_new ();

  pango_font_description_set_weight (font_desc,
                                     PANGO_WEIGHT_BOLD);

  /* This will only affect the weight of the font, the rest is
   * from the current state of the widget, which comes from the
   * theme or user prefs, since the font desc only has the
   * weight flag turned on.
   */
  gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

  pango_font_description_free (font_desc);
}

void
_make_gtk_label_normal (GtkLabel *label)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_new ();

  pango_font_description_set_weight (font_desc,
                                     PANGO_WEIGHT_NORMAL);

  /* This will only affect the weight of the font, the rest is
   * from the current state of the widget, which comes from the
   * theme or user prefs, since the font desc only has the
   * weight flag turned on.
   */
  gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

  pango_font_description_free (font_desc);
}

#ifdef HAVE_STARTUP_NOTIFICATION
static gboolean
_matewnck_util_sn_utf8_validator (const char *str,
                              int         max_len)
{
  return g_utf8_validate (str, max_len, NULL);
}
#endif /* HAVE_STARTUP_NOTIFICATION */

void
_matewnck_init (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      bindtextdomain (GETTEXT_PACKAGE, MATEWNCK_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#ifdef HAVE_STARTUP_NOTIFICATION
      sn_set_utf8_validator (_matewnck_util_sn_utf8_validator);
#endif /* HAVE_STARTUP_NOTIFICATION */

      done = TRUE;
    }
}

Display *
_matewnck_get_default_display (void)
{
  /* FIXME: when we fix libmatewnck to not use the GDK default display, we will
   * need to fix matewnckprop accordingly. */
  return GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}

/* stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org> */
typedef struct
{
  char *stock_id;
  const guint8 *icon_data;
} StockIcon;

void
_matewnck_stock_icons_init (void)
{
  GtkIconFactory *factory;
  int i;
  static gboolean done = FALSE;

  StockIcon items[] =
  {
    { MATEWNCK_STOCK_DELETE,   NULL },
    { MATEWNCK_STOCK_MINIMIZE, NULL },
    { MATEWNCK_STOCK_MAXIMIZE, NULL }
  };

  if (done)
    return;

  done = TRUE;

  factory = gtk_icon_factory_new ();
  gtk_icon_factory_add_default (factory);

  for (i = 0; i < (gint) G_N_ELEMENTS (items); i++)
    {
      GtkIconSet *icon_set;
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_inline (-1, items[i].icon_data,
					   FALSE,
					   NULL);

      icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
      gtk_icon_set_unref (icon_set);

      g_object_unref (G_OBJECT (pixbuf));
    }

  g_object_unref (G_OBJECT (factory));
}
