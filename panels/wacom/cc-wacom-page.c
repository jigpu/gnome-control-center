/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#include "cc-wacom-page.h"
#include "cc-wacom-nav-button.h"
#include "cc-wacom-stylus-page.h"
#include "cc-wacom-mapping-panel.h"
#include "gui_gtk.h"
#include <gtk/gtk.h>

#include <string.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (priv->builder, x)

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

#define WACOM_PAGE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_PAGE, CcWacomPagePrivate))

#define THRESHOLD_MISCLICK	15
#define THRESHOLD_DOUBLECLICK	7

struct _CcWacomPagePrivate
{
	GsdWacomDevice *stylus, *eraser;
	GtkBuilder     *builder;
	GtkWidget      *nav;
	GtkWidget      *notebook;
	CalibArea      *area;
	GSettings      *wacom_settings;
	GtkWidget      *mapper;
	/* The UI doesn't support cursor/pad at the moment */
};

/* Button combo box storage columns */
enum {
	BUTTONNUMBER_COLUMN,
	BUTTONNAME_COLUMN,
	N_BUTTONCOLUMNS
};

/* Tablet mode combo box storage columns */
enum {
	MODENUMBER_COLUMN,
	MODELABEL_COLUMN,
	N_MODECOLUMNS
};

/* Tablet mode options - keep in sync with .ui */
enum {
	MODE_ABSOLUTE, /* stylus + eraser absolute */
	MODE_RELATIVE, /* stylus + eraser relative */
};

static void
set_calibration (gint      *cal,
                 gsize      ncal,
                 GSettings *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	GVariant   **tmp;
	gsize        nvalues;
	int          i;

	current = g_settings_get_value (settings, "area");
	g_variant_get_fixed_array (current, &nvalues, sizeof (gint32));
	if ((ncal != 4) || (nvalues != 4)) {
		g_warning("Unable set set device calibration property. Got %"G_GSIZE_FORMAT" items to put in %"G_GSIZE_FORMAT" slots; expected %d items.\n", ncal, nvalues, 4);
		return;
	}

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_int32 (cal[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_INT32, tmp, nvalues);
	g_settings_set_value (settings, "area", array);

	g_free (tmp);
	g_variant_unref (array);
}

static void
finish_calibration (CalibArea *area,
		    gpointer   user_data)
{
	CcWacomPage *page = (CcWacomPage *) user_data;
	CcWacomPagePrivate *priv = page->priv;
	XYinfo axis;
	gboolean swap_xy;
	int cal[4];

	if (calib_area_finish (area, &axis, &swap_xy)) {
		cal[0] = axis.x_min;
		cal[1] = axis.y_min;
		cal[2] = axis.x_max;
		cal[3] = axis.y_max;

		set_calibration(cal, 4, page->priv->wacom_settings);
	}

	calib_area_free (area);
	page->priv->area = NULL;
	gtk_widget_set_sensitive (WID ("button-calibrate"), TRUE);
}

static gboolean
run_calibration (CcWacomPage *page,
		 gint        *cal,
		 gint         monitor)
{
	XYinfo old_axis;

	g_assert (page->priv->area == NULL);

	old_axis.x_min = cal[0];
	old_axis.y_min = cal[1];
	old_axis.x_max = cal[2];
	old_axis.y_max = cal[3];

	page->priv->area = calib_area_new (NULL,
					   monitor,
					   finish_calibration,
					   page,
					   &old_axis,
					   THRESHOLD_MISCLICK,
					   THRESHOLD_DOUBLECLICK);

	return FALSE;
}

static void
calibrate_button_clicked_cb (GtkButton   *button,
			     CcWacomPage *page)
{
	int i, calibration[4];
	GVariant *variant;
	int *current;
	gsize ncal;
	gint monitor;

	monitor = gsd_wacom_device_get_display_monitor (page->priv->stylus);
	if (monitor < 0) {
		/* The display the tablet should be mapped to could not be located.
		 * This shouldn't happen if the EDID data is good...
		 */
		g_critical("Output associated with the tablet is not connected. Unable to calibrate.");
		return;
	}

	variant = g_settings_get_value (page->priv->wacom_settings, "area");
	current = (int *) g_variant_get_fixed_array (variant, &ncal, sizeof (gint32));

	if (ncal != 4) {
		g_warning("Device calibration property has wrong length. Got %"G_GSIZE_FORMAT" items; expected %d.\n", ncal, 4);
		g_free (current);
		return;
	}

	for (i = 0; i < 4; i++)
		calibration[i] = current[i];

	if (calibration[0] == -1 &&
	    calibration[1] == -1 &&
	    calibration[2] == -1 &&
	    calibration[3] == -1) {
		gint *device_cal;
		device_cal = gsd_wacom_device_get_area (page->priv->stylus);
		for (i = 0; i < 4; i++)
			calibration[i] = device_cal[i];
		g_free (device_cal);
	}

	run_calibration (page, calibration, monitor);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
}

static void
tabletmode_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	CcWacomPagePrivate	*priv	= CC_WACOM_PAGE(user_data)->priv;
	GtkListStore		*liststore;
	GtkTreeIter		iter;
	gint			mode;
	gboolean		is_absolute;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	liststore = GTK_LIST_STORE (WID ("liststore-tabletmode"));
	gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
			    MODENUMBER_COLUMN, &mode,
			    -1);

	is_absolute = (mode == MODE_ABSOLUTE);
	g_settings_set_boolean (priv->wacom_settings, "is-absolute", is_absolute);
}

static void
left_handed_toggled_cb (GtkSwitch *sw, GParamSpec *pspec, gpointer *user_data)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(user_data)->priv;
	const gchar*		rotation;

	rotation = gtk_switch_get_active (sw) ? "half" : "none";

	g_settings_set_string (priv->wacom_settings, "rotation", rotation);
}

static void
set_left_handed_from_gsettings (CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = CC_WACOM_PAGE(page)->priv;
	const gchar*		rotation;

	rotation = g_settings_get_string (priv->wacom_settings, "rotation");
	if (strcmp (rotation, "half") == 0)
		gtk_switch_set_active (GTK_SWITCH (WID ("switch-left-handed")), TRUE);
}

static void
set_mode_from_gsettings (GtkComboBox *combo, CcWacomPage *page)
{
	CcWacomPagePrivate	*priv = page->priv;
	gboolean		is_absolute;

	is_absolute = g_settings_get_boolean (priv->wacom_settings, "is-absolute");

	/* this must be kept in sync with the .ui file */
	gtk_combo_box_set_active (combo, is_absolute ? MODE_ABSOLUTE : MODE_RELATIVE);
}

static void
combobox_text_cellrenderer (GtkComboBox *combo, int name_column)
{
	GtkCellRenderer	*renderer;

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", BUTTONNAME_COLUMN, NULL);
}

/* Boilerplate code goes below */

static void
cc_wacom_page_get_property (GObject    *object,
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
cc_wacom_page_set_property (GObject      *object,
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
cc_wacom_page_dispose (GObject *object)
{
	CcWacomPagePrivate *priv = CC_WACOM_PAGE (object)->priv;

	if (priv->area) {
		calib_area_free (priv->area);
		priv->area = NULL;
	}

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}


	G_OBJECT_CLASS (cc_wacom_page_parent_class)->dispose (object);
}

static void
cc_wacom_page_class_init (CcWacomPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomPagePrivate));

	object_class->get_property = cc_wacom_page_get_property;
	object_class->set_property = cc_wacom_page_set_property;
	object_class->dispose = cc_wacom_page_dispose;
}

static void
cc_wacom_page_init (CcWacomPage *self)
{
	CcWacomPagePrivate *priv;
	GError *error = NULL;
	GtkComboBox *combo;
	GtkWidget *box;
	GtkSwitch *sw;
	char *objects[] = {
		"main-grid",
		"liststore-tabletmode",
		"liststore-buttons",
		"adjustment-tip-feel",
		"adjustment-eraser-feel",
		NULL
	};

	priv = self->priv = WACOM_PAGE_PRIVATE (self);

	priv->builder = gtk_builder_new ();

	gtk_builder_add_objects_from_file (priv->builder,
					   GNOMECC_UI_DIR "/gnome-wacom-properties.ui",
					   objects,
					   &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_object_unref (priv->builder);
		g_error_free (error);
		return;
	}

	box = WID ("main-grid");
	gtk_container_add (GTK_CONTAINER (self), box);
	gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);

	/* TODO: When finished, the output mapping panel should be an independent
	 * dialog; not crammed in with the rest of the tablet UI.*/
	priv->mapper = cc_wacom_mapping_panel_new ();
	gtk_grid_insert_row (GTK_GRID (box), 2);
	gtk_grid_attach_next_to (GTK_GRID (box), priv->mapper, WID ("combo-tabletmode"), GTK_POS_BOTTOM, 1, 1);
	gtk_widget_set_hexpand (priv->mapper, TRUE);

	self->priv->notebook = WID ("stylus-notebook");

	g_signal_connect (WID ("button-calibrate"), "clicked",
			  G_CALLBACK (calibrate_button_clicked_cb), self);

	combo = GTK_COMBO_BOX (WID ("combo-tabletmode"));
	combobox_text_cellrenderer (combo, MODELABEL_COLUMN);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (tabletmode_changed_cb), self);

	sw = GTK_SWITCH (WID ("switch-left-handed"));
	g_signal_connect (G_OBJECT (sw), "notify::active",
			  G_CALLBACK (left_handed_toggled_cb), self);

	priv->nav = cc_wacom_nav_button_new ();
	gtk_grid_attach (GTK_GRID (box), priv->nav, 0, 0, 1, 1);
}

static void
set_icon_name (CcWacomPage *page,
	       const char  *widget_name,
	       const char  *icon_name)
{
	CcWacomPagePrivate *priv;
	char *filename, *path;

	priv = page->priv;

	filename = g_strdup_printf ("%s.svg", icon_name);
	path = g_build_filename (PIXMAP_DIR, filename, NULL);
	g_free (filename);

	gtk_image_set_from_file (GTK_IMAGE (WID (widget_name)), path);
	g_free (path);
}

typedef struct {
	GsdWacomStylus *stylus;
	GsdWacomStylus *eraser;
} StylusPair;

static void
add_styli (CcWacomPage *page)
{
	GList *styli, *l;
	CcWacomPagePrivate *priv;

	priv = page->priv;

	styli = gsd_wacom_device_list_styli (priv->stylus);

	for (l = styli; l; l = l->next) {
		GsdWacomStylus *stylus, *eraser;
		GtkWidget *page;

		stylus = l->data;
		if (gsd_wacom_stylus_get_has_eraser (stylus)) {
			GsdWacomDeviceType type;
			type = gsd_wacom_stylus_get_stylus_type (stylus);
			eraser = gsd_wacom_device_get_stylus_for_type (priv->eraser, type);
		} else {
			eraser = NULL;
		}

		page = cc_wacom_stylus_page_new (stylus, eraser);
		cc_wacom_stylus_page_set_navigation (CC_WACOM_STYLUS_PAGE (page), GTK_NOTEBOOK (priv->notebook));
		gtk_widget_show (page);
		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), page, NULL);
	}
	g_list_free (styli);

	/*FIXME: Set the page with the last used item */
}

GtkWidget *
cc_wacom_page_new (GsdWacomDevice *stylus,
		   GsdWacomDevice *eraser)
{
	CcWacomPage *page;
	CcWacomPagePrivate *priv;

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (stylus), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (stylus) == WACOM_TYPE_STYLUS, NULL);

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (eraser), NULL);
	g_return_val_if_fail (gsd_wacom_device_get_device_type (eraser) == WACOM_TYPE_ERASER, NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	priv = page->priv;
	priv->stylus = stylus;
	priv->eraser = eraser;
	cc_wacom_mapping_panel_set_device(CC_WACOM_MAPPING_PANEL (priv->mapper), priv->stylus);

	/* FIXME move this to construct */
	priv->wacom_settings  = gsd_wacom_device_get_settings (stylus);
	set_mode_from_gsettings (GTK_COMBO_BOX (WID ("combo-tabletmode")), page);

	/* Tablet name */
	gtk_label_set_text (GTK_LABEL (WID ("label-tabletmodel")), gsd_wacom_device_get_name (stylus));

	/* Left-handedness */
	if (gsd_wacom_device_reversible (stylus) == FALSE) {
		gtk_widget_hide (WID ("label-left-handed"));
		gtk_widget_hide (WID ("switch-left-handed"));
	} else {
		set_left_handed_from_gsettings (page);
	}

	/* Calibration for screen tablets */
	if (gsd_wacom_device_is_screen_tablet (stylus) == TRUE) {
		gtk_widget_show (WID ("button-calibrate"));
	}

	/* Tablet icon */
	set_icon_name (page, "image-tablet", gsd_wacom_device_get_icon_name (stylus));

	/* Add styli */
	add_styli (page);

	return GTK_WIDGET (page);
}

void
cc_wacom_page_set_navigation (CcWacomPage *page,
			      GtkNotebook *notebook,
			      gboolean     ignore_first_page)
{
	CcWacomPagePrivate *priv;

	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	priv = page->priv;

	g_object_set (G_OBJECT (priv->nav),
		      "notebook", notebook,
		      "ignore-first", ignore_first_page,
		      NULL);
}
