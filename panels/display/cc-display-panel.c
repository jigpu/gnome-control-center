/*
 * Copyright (C) 2007, 2008  Red Hat, Inc.
 * Copyright (C) 2012 Wacom.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 *         Jason Gerecke  <killertofu@gmail.com>
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "cc-display-panel.h"

#include <gtk/gtk.h>
#include "scrollarea.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>
#include <libgnome-desktop/gnome-rr-labeler.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <glib/gi18n.h>
#include <gdesktop-enums.h>

G_DEFINE_DYNAMIC_TYPE (CcDisplayPanel, cc_display_panel, CC_TYPE_PANEL)

#define DISPLAY_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PANEL, CcDisplayPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

/* The minimum supported size for the panel, see:
 * http://live.gnome.org/Design/SystemSettings */
#define MINIMUM_WIDTH 675
#define MINIMUM_HEIGHT 530

struct _CcDisplayPanelPrivate
{
  GnomeRRScreen       *screen;
  GnomeRROutputInfo         *current_output;

  GtkBuilder     *builder;
  guint           focus_id;

  /* We store the event timestamp when the Apply button is clicked */
  guint32         apply_button_clicked_timestamp;

  GtkWidget      *panel;
  GtkWidget      *foo_panel;

  /* These are used while we are waiting for the ApplyConfiguration method to be executed over D-bus */
  GDBusProxy *proxy;
};

static void select_current_output_from_dialog_position (CcDisplayPanel *self);
static void apply_configuration_returned_cb (GObject *proxy, GAsyncResult *res, gpointer data);
static GObject *cc_display_panel_constructor (GType                  gtype,
					      guint                  n_properties,
					      GObjectConstructParam *properties);

static void
cc_display_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_display_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_display_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
cc_display_panel_finalize (GObject *object)
{
  CcDisplayPanel *self;
  CcShell *shell;
  GtkWidget *toplevel;

  self = CC_DISPLAY_PANEL (object);

  g_object_unref (self->priv->screen);
  g_object_unref (self->priv->builder);

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  g_signal_handler_disconnect (G_OBJECT (toplevel),
                               self->priv->focus_id);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->finalize (object);
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDisplayPanelPrivate));

  object_class->constructor = cc_display_panel_constructor;
  object_class->get_property = cc_display_panel_get_property;
  object_class->set_property = cc_display_panel_set_property;
  object_class->dispose = cc_display_panel_dispose;
  object_class->finalize = cc_display_panel_finalize;
}

static void
cc_display_panel_class_finalize (CcDisplayPanelClass *klass)
{
}

static void
error_message (CcDisplayPanel *self, const char *primary_text, const char *secondary_text)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  if (self && self->priv->panel)
    toplevel = gtk_widget_get_toplevel (self->priv->panel);
  else
    toplevel = NULL;

  dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", primary_text);

  if (secondary_text)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_text);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
compute_virtual_size_for_configuration (GnomeRRConfig *config, int *ret_width, int *ret_height)
{
  int i;
  int width, height;
  int output_x, output_y, output_width, output_height;
  GnomeRROutputInfo **outputs;

  width = height = 0;

  outputs = gnome_rr_config_get_outputs (config);
  for (i = 0; outputs[i] != NULL; i++)
    {
      if (gnome_rr_output_info_is_active (outputs[i]))
	{
	  gnome_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);
	  width = MAX (width, output_x + output_width);
	  height = MAX (height, output_y + output_height);
	}
    }

  *ret_width = width;
  *ret_height = height;
}

static void
check_required_virtual_size (CcDisplayPanel *self)
{
  GnomeRRConfig *config;
  int req_width, req_height;
  int min_width, max_width;
  int min_height, max_height;

  config = foo_display_panel_get_configuration (self->priv->foo_panel);
  compute_virtual_size_for_configuration (config, &req_width, &req_height);

  gnome_rr_screen_get_ranges (self->priv->screen, &min_width, &max_width, &min_height, &max_height);

#if 0
  g_debug ("X Server supports:");
  g_debug ("min_width = %d, max_width = %d", min_width, max_width);
  g_debug ("min_height = %d, max_height = %d", min_height, max_height);

  g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

  if (!(min_width <= req_width && req_width <= max_width
        && min_height <= req_height && req_height <= max_height))
    {
      /* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
      g_debug ("Your X server needs a larger Virtual size!");
#endif
    }
}

static void
begin_version2_apply_configuration (CcDisplayPanel *self, GdkWindow *parent_window, guint32 timestamp)
{
  XID parent_window_xid;
  GError *error = NULL;

  parent_window_xid = GDK_WINDOW_XID (parent_window);

  self->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     "org.gnome.SettingsDaemon",
                                                     "/org/gnome/SettingsDaemon/XRANDR",
                                                     "org.gnome.SettingsDaemon.XRANDR_2",
                                                     NULL,
                                                     &error);
  if (self->priv->proxy == NULL) {
    error_message (self, _("Failed to apply configuration: %s"), error->message);
    g_error_free (error);
    return;
  }

  g_dbus_proxy_call (self->priv->proxy,
                     "ApplyConfiguration",
                     g_variant_new ("(xx)", (gint64) parent_window_xid, (gint64) timestamp),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     apply_configuration_returned_cb,
                     self);
}

static void
ensure_current_configuration_is_saved (void)
{
  GnomeRRScreen *rr_screen;
  GnomeRRConfig *rr_config;

  /* Normally, gnome_rr_config_save() creates a backup file based on the
   * old monitors.xml.  However, if *that* file didn't exist, there is
   * nothing from which to create a backup.  So, here we'll save the
   * current/unchanged configuration and then let our caller call
   * gnome_rr_config_save() again with the new/changed configuration, so
   * that there *will* be a backup file in the end.
   */

  rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL); /* NULL-GError */
  if (!rr_screen)
    return;

  rr_config = gnome_rr_config_new_current (rr_screen, NULL);
  gnome_rr_config_ensure_primary (rr_config);
  gnome_rr_config_save (rr_config, NULL); /* NULL-GError */

  g_object_unref (rr_config);
  g_object_unref (rr_screen);
}

static void
apply_configuration_returned_cb (GObject          *proxy,
                                 GAsyncResult     *res,
                                 gpointer          data)
{
  CcDisplayPanel *self = data;
  GVariant *result;
  GError *error = NULL;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &error);
  if (error)
    error_message (self, _("Failed to apply configuration: %s"), error->message);
  g_clear_error (&error);  
  if (result)
    g_variant_unref (result);

  g_object_unref (self->priv->proxy);
  self->priv->proxy = NULL;

  gtk_widget_set_sensitive (self->priv->panel, TRUE);
}

static gboolean
sanitize_and_save_configuration (CcDisplayPanel *self)
{
  GnomeRRConfig *config;
  GError *error;

  config = foo_display_panel_get_configuration (self->priv->foo_panel);
  gnome_rr_config_sanitize (config);
  gnome_rr_config_ensure_primary (config);

  check_required_virtual_size (self);

  /* TODO: Invalidate the foo_panel */
  foo_scroll_area_invalidate (FOO_SCROLL_AREA (self->priv->area));

  ensure_current_configuration_is_saved ();

  error = NULL;
  if (!gnome_rr_config_save (self->priv->current_configuration, &error))
    {
      error_message (self, _("Could not save the monitor configuration"), error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

static void
apply (CcDisplayPanel *self)
{
  GdkWindow *window;

  self->priv->apply_button_clicked_timestamp = gtk_get_current_event_time ();

  if (!sanitize_and_save_configuration (self))
    return;

  g_assert (self->priv->proxy == NULL);

  gtk_widget_set_sensitive (self->priv->panel, FALSE);

  window = gtk_widget_get_window (gtk_widget_get_toplevel (self->priv->panel));

  begin_version2_apply_configuration (self, window,
                                      self->priv->apply_button_clicked_timestamp);
}

static GnomeRROutputInfo *
get_nearest_output (GnomeRRConfig *configuration, int x, int y)
{
  int i;
  int nearest_index;
  int nearest_dist;
  GnomeRROutputInfo **outputs;

  nearest_index = -1;
  nearest_dist = G_MAXINT;

  outputs = gnome_rr_config_get_outputs (configuration);
  for (i = 0; outputs[i] != NULL; i++)
    {
      int dist_x, dist_y;
      int output_x, output_y, output_width, output_height;

      if (!(gnome_rr_output_info_is_connected (outputs[i]) && gnome_rr_output_info_is_active (outputs[i])))
	continue;

      gnome_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);

      if (x < output_x)
	dist_x = output_x - x;
      else if (x >= output_x + output_width)
	dist_x = x - (output_x + output_width) + 1;
      else
	dist_x = 0;

      if (y < output_y)
	dist_y = output_y - y;
      else if (y >= output_y + output_height)
	dist_y = y - (output_y + output_height) + 1;
      else
	dist_y = 0;

      if (MIN (dist_x, dist_y) < nearest_dist)
	{
	  nearest_dist = MIN (dist_x, dist_y);
	  nearest_index = i;
	}
    }

  if (nearest_index != -1)
    return outputs[nearest_index];
  else
    return NULL;
}

/* Gets the output that contains the largest intersection with the window.
 * Logic stolen from gdk_screen_get_monitor_at_window().
 */
static GnomeRROutputInfo *
get_output_for_window (GnomeRRConfig *configuration, GdkWindow *window)
{
  GdkRectangle win_rect;
  int i;
  int largest_area;
  int largest_index;
  GnomeRROutputInfo **outputs;

  gdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width, &win_rect.height);
  gdk_window_get_origin (window, &win_rect.x, &win_rect.y);

  largest_area = 0;
  largest_index = -1;

  outputs = gnome_rr_config_get_outputs (configuration);
  for (i = 0; outputs[i] != NULL; i++)
    {
      GdkRectangle output_rect, intersection;

      gnome_rr_output_info_get_geometry (outputs[i], &output_rect.x, &output_rect.y, &output_rect.width, &output_rect.height);

      if (gnome_rr_output_info_is_connected (outputs[i]) && gdk_rectangle_intersect (&win_rect, &output_rect, &intersection))
	{
	  int area;

	  area = intersection.width * intersection.height;
	  if (area > largest_area)
	    {
	      largest_area = area;
	      largest_index = i;
	    }
	}
    }

  if (largest_index != -1)
    return outputs[largest_index];
  else
    return get_nearest_output (configuration,
			       win_rect.x + win_rect.width / 2,
			       win_rect.y + win_rect.height / 2);
}

static void
dialog_toplevel_focus_changed (GtkWindow      *window,
			       GParamSpec     *pspec,
			       CcDisplayPanel *self)
{
  foo_display_panel_enable_labler (self->priv->foo_panel, gtk_window_has_toplevel_focus (window));
}

static void
on_toplevel_realized (GtkWidget     *widget,
                      CcDisplayPanel *self)
{
  GnomeRROutputInfo *output;
  output = get_output_for_window (foo_display_panel_get_configuration (self->priv->foo_panel),
                                  gtk_widget_get_window (widget));
  foo_display_panel_set_output (self->priv->foo_panel, output);
}

/* We select the current output, i.e. select the one being edited, based on
 * which output is showing the configuration dialog.
 */
static void
select_current_output_from_dialog_position (CcDisplayPanel *self)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (self->priv->panel);

  if (gtk_widget_get_realized (toplevel)) {
    GnomeRROutputInfo *output;
    output = get_output_for_window (foo_display_panel_get_configuration (self->priv->foo_panel),
                                    gtk_widget_get_window (toplevel));
    foo_display_panel_set_output (self->priv->foo_panel, output);
  } else {
    g_signal_connect (toplevel, "realize", G_CALLBACK (on_toplevel_realized), self);
    foo_display_panel_set_output (self->priv->foo_panel, NULL);
  }
}

/* This is a GtkWidget::map-event handler.  We wait for the display-properties
 * dialog to be mapped, and then we select the output which corresponds to the
 * monitor on which the dialog is being shown.
 */
static gboolean
dialog_map_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
  CcDisplayPanel *self = data;

  select_current_output_from_dialog_position (self);
  return FALSE;
}

static void
cc_display_panel_init (CcDisplayPanel *self)
{
}

static GObject *
cc_display_panel_constructor (GType                  gtype,
                              guint                  n_properties,
                              GObjectConstructParam *properties)
{
  GtkBuilder *builder;
  GtkWidget *align;
  GError *error;
  GObject *obj;
  CcDisplayPanel *self;
  CcShell *shell;
  GtkWidget *toplevel;
  gchar *objects[] = {"display-panel", NULL};

  obj = G_OBJECT_CLASS (cc_display_panel_parent_class)->constructor (gtype, n_properties, properties);
  self = CC_DISPLAY_PANEL (obj);
  self->priv = DISPLAY_PANEL_PRIVATE (self);

  error = NULL;
  self->priv->builder = builder = gtk_builder_new ();

  if (!gtk_builder_add_objects_from_file (builder, UIDIR "/display-capplet.ui", objects, &error))
    {
      g_warning ("Could not parse UI definition: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      return obj;
    }

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  self->priv->focus_id = g_signal_connect (toplevel, "notify::has-toplevel-focus",
                                           G_CALLBACK (dialog_toplevel_focus_changed), self);
  on_screen_changed (self->priv->screen, self);

  self->priv->panel = WID ("display-panel");
  g_signal_connect_after (self->priv->panel, "show",
                          G_CALLBACK (dialog_map_event_cb), self);

  align = WID ("align");

  /* TODO: Create a foo-display-panel, save it to self->priv->foo_panel, and add it to the align container */
  gtk_container_add (GTK_CONTAINER (align), self->priv->area);

  on_screen_changed (self->priv->screen, self);

  g_signal_connect_swapped (WID ("apply_button"),
                            "clicked", G_CALLBACK (apply), self);

  gtk_widget_show (self->priv->panel);
  gtk_container_add (GTK_CONTAINER (self), self->priv->panel);
  cc_display_panel_edit_outputs (self, FALSE);
  return obj;
}

void
cc_display_panel_register (GIOModule *module)
{
  cc_display_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_DISPLAY_PANEL,
                                  "display", 0);
}

