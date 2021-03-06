/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "um-strength-bar.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_STRENGTH_BAR, UmStrengthBarPrivate))

struct _UmStrengthBarPrivate {
        gdouble strength;
};

enum {
        PROP_0,
        PROP_STRENGTH
};

G_DEFINE_TYPE (UmStrengthBar, um_strength_bar, GTK_TYPE_WIDGET);


static void
um_strength_bar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        UmStrengthBar *bar = UM_STRENGTH_BAR (object);

        switch (prop_id) {
        case PROP_STRENGTH:
                um_strength_bar_set_strength (bar, g_value_get_double (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_strength_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        UmStrengthBar *bar = UM_STRENGTH_BAR (object);

        switch (prop_id) {
        case PROP_STRENGTH:
                g_value_set_double (value, um_strength_bar_get_strength (bar));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
curved_rectangle (cairo_t *cr,
                  double   x0,
                  double   y0,
                  double   width,
                  double   height,
                  double   radius)
{
        double x1;
        double y1;

        x1 = x0 + width;
        y1 = y0 + height;

        if (!width || !height) {
                return;
        }

        if (width / 2 < radius) {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, x0, (y0 + y1) / 2);
                        cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1) / 2, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
                        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
                } else {
                        cairo_move_to  (cr, x0, y0 + radius);
                        cairo_curve_to (cr, x0, y0, x0, y0, (x0 + x1) / 2, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
                        cairo_line_to (cr, x1, y1 - radius);
                        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
                }
        } else {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, x0, (y0 + y1) / 2);
                        cairo_curve_to (cr, x0, y0, x0 , y0, x0 + radius, y0);
                        cairo_line_to (cr, x1 - radius, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
                        cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
                        cairo_line_to (cr, x0 + radius, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
                } else {
                        cairo_move_to  (cr, x0, y0 + radius);
                        cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
                        cairo_line_to (cr, x1 - radius, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
                        cairo_line_to (cr, x1, y1 - radius);
                        cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
                        cairo_line_to (cr, x0 + radius, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
                }
        }

        cairo_close_path (cr);
}

GdkRGBA color1 = { 213.0/255.0,   4.0/255.0,   4.0/255.0, 1.0 };
GdkRGBA color2 = { 234.0/255.0, 236.0/255.0,  31.0/255.0, 1.0 };
GdkRGBA color3 = { 141.0/255.0, 133.0/255.0, 241.0/255.0, 1.0 };
GdkRGBA color4 = {  99.0/255.0, 251.0/255.0, 107.0/255.0, 1.0 };

static void
get_color (gdouble value, GdkRGBA *color)
{
        if (value < 0.50) {
                *color = color1;
        }
        else if (value < 0.75) {
                *color = color2;
        }
        else if (value < 0.90) {
                *color = color3;
        }
        else {
                *color = color4;
        }
}

static gboolean
um_strength_bar_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
        UmStrengthBar *bar = UM_STRENGTH_BAR (widget);
        GdkRGBA color;
        gint width, height;
        GtkStyleContext *context;
        GtkStateFlags state;
        GdkRGBA border_color;

        context = gtk_widget_get_style_context (widget);
        state = gtk_widget_get_state_flags (widget);
        gtk_style_context_get_border_color (context, state, &border_color);

        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

        cairo_save (cr);

        cairo_set_line_width (cr, 1);

        cairo_save (cr);

        cairo_rectangle (cr,
                         0, 0,
                         bar->priv->strength * width, height);
        cairo_clip (cr);

        curved_rectangle (cr,
                          0.5, 0.5,
                          width - 1, height - 1,
                          4);
        get_color (bar->priv->strength, &color);
        gdk_cairo_set_source_rgba (cr, &color);
        cairo_fill_preserve (cr);

        cairo_restore (cr); /* undo clip */

        gdk_cairo_set_source_rgba (cr, &border_color);
        cairo_stroke (cr);

        cairo_restore (cr);

        return FALSE;
}

static void
um_strength_bar_class_init (UmStrengthBarClass *class)
{
        GObjectClass *gobject_class;
        GtkWidgetClass *widget_class;

        gobject_class = (GObjectClass*)class;
        widget_class = (GtkWidgetClass*)class;

        gobject_class->set_property = um_strength_bar_set_property;
        gobject_class->get_property = um_strength_bar_get_property;

        widget_class->draw = um_strength_bar_draw;

         g_object_class_install_property (gobject_class,
                                          PROP_STRENGTH,
                                          g_param_spec_double ("strength",
                                                               "Strength",
                                                               "Strength",
                                                               0.0, 1.0, 0.0,
                                                               G_PARAM_READWRITE));

        g_type_class_add_private (class, sizeof (UmStrengthBarPrivate));
}

static void
um_strength_bar_init (UmStrengthBar *bar)
{
        gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
        gtk_widget_set_size_request (GTK_WIDGET (bar), 120, 8);

        bar->priv = GET_PRIVATE (bar);
        bar->priv->strength = 0.0;
}

GtkWidget *
um_strength_bar_new (void)
{
        return (GtkWidget*) g_object_new (UM_TYPE_STRENGTH_BAR, NULL);
}

void
um_strength_bar_set_strength (UmStrengthBar *bar,
                              gdouble        strength)
{
        bar->priv->strength = strength;

        g_object_notify (G_OBJECT (bar), "strength");

        if (gtk_widget_is_drawable (GTK_WIDGET (bar))) {
                gtk_widget_queue_draw (GTK_WIDGET (bar));
        }
}

gdouble
um_strength_bar_get_strength (UmStrengthBar *bar)
{
        return bar->priv->strength;
}
