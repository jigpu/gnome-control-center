/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */


#ifndef _FOO_DISPLAY_PANEL_H
#define _FOO_DISPLAY_PANEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FOO_TYPE_DISPLAY_PANEL foo_display_panel_get_type()

#define FOO_DISPLAY_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  FOO_TYPE_DISPLAY_PANEL, FooDisplayPanel))

#define FOO_DISPLAY_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  FOO_TYPE_DISPLAY_PANEL, FooDisplayPanelClass))

#define FOO_IS_DISPLAY_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  FOO_TYPE_DISPLAY_PANEL))

#define FOO_IS_DISPLAY_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  FOO_TYPE_DISPLAY_PANEL))

#define FOO_DISPLAY_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  FOO_TYPE_DISPLAY_PANEL, FooDisplayPanelClass))

typedef struct _FooDisplayPanel FooDisplayPanel;
typedef struct _FooDisplayPanelClass FooDisplayPanelClass;
typedef struct _FooDisplayPanelPrivate FooDisplayPanelPrivate;

struct _FooDisplayPanel
{
  GtkBox parent;

  FooDisplayPanelPrivate *priv;
};

struct _FooDisplayPanelClass
{
  GtkBoxClass parent_class;
};

GType foo_display_panel_get_type (void) G_GNUC_CONST;

void  foo_display_panel_edit_layout              (FooDisplayPanel *self,
                                                  gboolean        enable);
void  foo_display_panel_edit_outputs             (FooDisplayPanel *self,
                                                  gboolean        enable);

GnomeRRConfig foo_display_panel_get_configuration (FooDisplayPanel *self);

void foo_display_panel_set_output (FooDisplayPanel *self,
                                   GnomeRROutput   *output);

gint foo_display_panel_get_selected_monitor      (FooDisplayPanel *self);

G_END_DECLS

#endif /* _FOO_DISPLAY_PANEL_H */
