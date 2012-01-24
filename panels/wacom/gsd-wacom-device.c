/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>

#include <libwacom/libwacom.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "gsd-input-helper.h"

#include "gnome-settings-daemon/gsd-enums.h"
#include "gsd-wacom-device.h"

#define GSD_WACOM_STYLUS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_WACOM_STYLUS, GsdWacomStylusPrivate))

#define WACOM_TABLET_SCHEMA "org.gnome.settings-daemon.peripherals.wacom"
#define WACOM_DEVICE_CONFIG_BASE "/org/gnome/settings-daemon/peripherals/wacom/%s/"
#define WACOM_STYLUS_SCHEMA "org.gnome.settings-daemon.peripherals.wacom.stylus"
#define WACOM_ERASER_SCHEMA "org.gnome.settings-daemon.peripherals.wacom.eraser"

static WacomDeviceDatabase *db = NULL;

struct GsdWacomStylusPrivate
{
	GsdWacomDevice *device;
	int id;
	WacomStylusType type;
	char *name;
	const char *icon_name;
	GSettings *settings;
	gboolean has_eraser;
	int num_buttons;
};

static void     gsd_wacom_stylus_class_init  (GsdWacomStylusClass *klass);
static void     gsd_wacom_stylus_init        (GsdWacomStylus      *wacom_stylus);
static void     gsd_wacom_stylus_finalize    (GObject              *object);

G_DEFINE_TYPE (GsdWacomStylus, gsd_wacom_stylus, G_TYPE_OBJECT)

static void
gsd_wacom_stylus_class_init (GsdWacomStylusClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_wacom_stylus_finalize;

        g_type_class_add_private (klass, sizeof (GsdWacomStylusPrivate));
}

static void
gsd_wacom_stylus_init (GsdWacomStylus *stylus)
{
        stylus->priv = GSD_WACOM_STYLUS_GET_PRIVATE (stylus);
}

static void
gsd_wacom_stylus_finalize (GObject *object)
{
        GsdWacomStylus *stylus;
        GsdWacomStylusPrivate *p;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_WACOM_STYLUS (object));

        stylus = GSD_WACOM_STYLUS (object);

        g_return_if_fail (stylus->priv != NULL);

	p = stylus->priv;

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        g_free (p->name);
        p->name = NULL;

        G_OBJECT_CLASS (gsd_wacom_stylus_parent_class)->finalize (object);
}

static const char *
get_icon_name_from_type (WacomStylusType type)
{
	switch (type) {
	case WSTYLUS_INKING:
	case WSTYLUS_STROKE:
		/* The stroke pen is the same as the inking pen with
		 * a different nib */
		return "wacom-stylus-inking";
	case WSTYLUS_AIRBRUSH:
		return "wacom-stylus-airbrush";
	default:
		return "wacom-stylus";
	}
}

static GsdWacomStylus *
gsd_wacom_stylus_new (GsdWacomDevice    *device,
		      const WacomStylus *wstylus,
		      GSettings         *settings)
{
	GsdWacomStylus *stylus;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);
	g_return_val_if_fail (wstylus != NULL, NULL);

	stylus = GSD_WACOM_STYLUS (g_object_new (GSD_TYPE_WACOM_STYLUS,
						 NULL));
	stylus->priv->device = device;
	stylus->priv->id = libwacom_stylus_get_id (wstylus);
	stylus->priv->name = g_strdup (libwacom_stylus_get_name (wstylus));
	stylus->priv->settings = settings;
	stylus->priv->type = libwacom_stylus_get_type (wstylus);
	stylus->priv->icon_name = get_icon_name_from_type (stylus->priv->type);
	stylus->priv->has_eraser = libwacom_stylus_has_eraser (wstylus);
	stylus->priv->num_buttons = libwacom_stylus_get_num_buttons (wstylus);

	return stylus;
}

GSettings *
gsd_wacom_stylus_get_settings (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->settings;
}

const char *
gsd_wacom_stylus_get_name (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->name;
}

const char *
gsd_wacom_stylus_get_icon_name (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->icon_name;
}

GsdWacomDevice *
gsd_wacom_stylus_get_device (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), NULL);

	return stylus->priv->device;
}

gboolean
gsd_wacom_stylus_get_has_eraser (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), FALSE);

	return stylus->priv->has_eraser;
}

int
gsd_wacom_stylus_get_num_buttons (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), -1);

	return stylus->priv->num_buttons;
}

GsdWacomStylusType
gsd_wacom_stylus_get_stylus_type (GsdWacomStylus *stylus)
{
	g_return_val_if_fail (GSD_IS_WACOM_STYLUS (stylus), WACOM_STYLUS_TYPE_UNKNOWN);

	switch (stylus->priv->type) {
	case WSTYLUS_UNKNOWN:
		return WACOM_STYLUS_TYPE_UNKNOWN;
	case WSTYLUS_GENERAL:
		return WACOM_STYLUS_TYPE_GENERAL;
	case WSTYLUS_INKING:
		return WACOM_STYLUS_TYPE_INKING;
	case WSTYLUS_AIRBRUSH:
		return WACOM_STYLUS_TYPE_AIRBRUSH;
	case WSTYLUS_CLASSIC:
		return WACOM_STYLUS_TYPE_CLASSIC;
	case WSTYLUS_MARKER:
		return WACOM_STYLUS_TYPE_MARKER;
	case WSTYLUS_STROKE:
		return WACOM_STYLUS_TYPE_STROKE;
	default:
		g_assert_not_reached ();
	}

	return WACOM_STYLUS_TYPE_UNKNOWN;
}

#define GSD_WACOM_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_WACOM_DEVICE, GsdWacomDevicePrivate))

/* we support two types of settings:
 * Tablet-wide settings: applied to each tool on the tablet. e.g. rotation
 * Tool-specific settings: applied to one tool only.
 */
#define SETTINGS_WACOM_DIR         "org.gnome.settings-daemon.peripherals.wacom"
#define SETTINGS_STYLUS_DIR        "stylus"
#define SETTINGS_ERASER_DIR        "eraser"

struct GsdWacomDevicePrivate
{
	GdkDevice *gdk_device;
	int device_id;
	int opcode;

	GsdWacomDeviceType type;
	char *name;
	char *icon_name;
	char *tool_name;
	gboolean reversible;
	gboolean is_screen_tablet;
	GList *styli;
	GsdWacomStylus *last_stylus;
	GSettings *wacom_settings;
};

enum {
	PROP_0,
	PROP_GDK_DEVICE,
	PROP_LAST_STYLUS
};

static void     gsd_wacom_device_class_init  (GsdWacomDeviceClass *klass);
static void     gsd_wacom_device_init        (GsdWacomDevice      *wacom_device);
static void     gsd_wacom_device_finalize    (GObject              *object);

G_DEFINE_TYPE (GsdWacomDevice, gsd_wacom_device, G_TYPE_OBJECT)

static GdkFilterReturn
filter_events (XEvent         *xevent,
               GdkEvent       *event,
               GsdWacomDevice *device)
{
	XIEvent             *xiev;
	XIPropertyEvent     *pev;
	XGenericEventCookie *cookie;
	char                *name;
	int                  tool_id;

        /* verify we have a property event */
	if (xevent->type != GenericEvent)
		return GDK_FILTER_CONTINUE;

	cookie = &xevent->xcookie;
	if (cookie->extension != device->priv->opcode)
		return GDK_FILTER_CONTINUE;

	xiev = (XIEvent *) xevent->xcookie.data;

	if (xiev->evtype != XI_PropertyEvent)
		return GDK_FILTER_CONTINUE;

	pev = (XIPropertyEvent *) xiev;

	/* Is the event for us? */
	if (pev->deviceid != device->priv->device_id)
		return GDK_FILTER_CONTINUE;

	name = XGetAtomName (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), pev->property);
	if (name == NULL ||
	    g_strcmp0 (name, WACOM_SERIAL_IDS_PROP) != 0) {
		return GDK_FILTER_CONTINUE;
	}
	XFree (name);

	tool_id = xdevice_get_last_tool_id (device->priv->device_id);
	gsd_wacom_device_set_current_stylus (device, tool_id);

	return GDK_FILTER_CONTINUE;
}

static gboolean
setup_property_notify (GsdWacomDevice *device)
{
	Display *dpy;
	XIEventMask evmask;
	unsigned char bitmask[2] = { 0 };
	int tool_id;

	XISetMask (bitmask, XI_PropertyEvent);

	evmask.deviceid = device->priv->device_id;
	evmask.mask_len = sizeof (bitmask);
	evmask.mask = bitmask;

	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	XISelectEvents (dpy, DefaultRootWindow(dpy), &evmask, 1);

	gdk_window_add_filter (NULL,
			       (GdkFilterFunc) filter_events,
			       device);

	tool_id = xdevice_get_last_tool_id (device->priv->device_id);
	gsd_wacom_device_set_current_stylus (device, tool_id);

	return TRUE;
}

static GsdWacomDeviceType
get_device_type (XDeviceInfo *dev)
{
	GsdWacomDeviceType ret;
        static Atom stylus, cursor, eraser, pad, prop;
        XDevice *device;
        Atom realtype;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        int rc;

        ret = WACOM_TYPE_INVALID;

        if ((dev->use == IsXPointer) || (dev->use == IsXKeyboard))
                return ret;

        if (!stylus)
                stylus = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "STYLUS", False);
        if (!eraser)
                eraser = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "ERASER", False);
        if (!cursor)
                cursor = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "CURSOR", False);
        if (!pad)
                pad = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "PAD", False);
        /* FIXME: Add touch type? */
        if (!prop)
		prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Wacom Tool Type", False);

	if (dev->type == stylus)
		ret = WACOM_TYPE_STYLUS;
	if (dev->type == eraser)
		ret = WACOM_TYPE_ERASER;
	if (dev->type == cursor)
		ret = WACOM_TYPE_CURSOR;
	if (dev->type == pad)
		ret = WACOM_TYPE_PAD;

	if (ret == WACOM_TYPE_INVALID)
		return ret;

        /* There is currently no good way of detecting the driver for a device
         * other than checking for a driver-specific property.
         * Wacom Tool Type exists on all tools
         */
        gdk_error_trap_push ();
        device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), dev->id);
        if (gdk_error_trap_pop () || (device == NULL))
                return ret;

        gdk_error_trap_push ();

        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 device, prop, 0, 1, False,
                                 XA_ATOM, &realtype, &realformat, &nitems,
                                 &bytes_after, &data);
        if (gdk_error_trap_pop () || rc != Success || realtype == None) {
                XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                ret = WACOM_TYPE_INVALID;
        }

        XFree (data);

	return ret;
}

static char *
get_device_name (WacomDevice *device)
{
	return g_strdup_printf ("%s %s",
				libwacom_get_vendor (device),
				libwacom_get_product (device));
}

/* Finds an output which matches the given EDID information. Any NULL
 * parameter will be interpreted to match any value.
 */
static GnomeRROutputInfo*
find_output_by_edid (const gchar *vendor, const gchar *product, const gchar *serial)
{
	GError *error = NULL;
	GnomeRRScreen *rr_screen;
	GnomeRRConfig *rr_config;
	GnomeRROutputInfo **rr_output_info;

	/* TODO: Check the value of 'error' */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
	rr_config = gnome_rr_config_new_current (rr_screen, &error);
	rr_output_info = gnome_rr_config_get_outputs (rr_config);

	for (; *rr_output_info != NULL; rr_output_info++) {
		gchar *o_vendor;
		gchar *o_product;
		gchar *o_serial;
		gboolean match;

		o_vendor = g_malloc0 (4);
		gnome_rr_output_info_get_vendor (*rr_output_info, o_vendor);
		o_product = g_strdup_printf ("%d", gnome_rr_output_info_get_product (*rr_output_info));
		o_serial  = g_strdup_printf ("%d", gnome_rr_output_info_get_serial  (*rr_output_info));

		g_debug ("Checking for match between '%s','%s','%s' and '%s','%s','%s'", \
		         vendor,product,serial, o_vendor,o_product,o_serial);

		match = (vendor  == NULL || g_strcmp0 (vendor,  o_vendor)  == 0) && \
		        (product == NULL || g_strcmp0 (product, o_product) == 0) && \
		        (serial  == NULL || g_strcmp0 (serial,  o_serial)  == 0);

		g_free (o_vendor);
		g_free (o_product);
		g_free (o_serial);

		if (match)
			return *rr_output_info;
	}
	return NULL;
}

static GnomeRROutputInfo*
find_output_by_heuristic (GsdWacomDevice *device)
{
	GnomeRROutputInfo *rr_output_info;

	/* TODO: This heuristic will fail for non-Wacom display
	 * tablets and may give the wrong result if multiple Wacom
	 * display tablets are connected.
	 */
	rr_output_info = find_output_by_edid("WAC", NULL, NULL);
	return rr_output_info;
}

static GnomeRROutputInfo*
find_output_by_display (GsdWacomDevice *device)
{
	gsize n;
	GSettings *tablet;
	GVariant *display;
	const gchar **edid;

	if (device == NULL)
		return NULL;

	tablet   = device->priv->wacom_settings;
	display  = g_settings_get_value (tablet, "display");
	edid     = g_variant_get_strv (display, &n);

	if (n != 3) {
		g_critical ("Expected 'display' key to store %d values; got %"G_GSIZE_FORMAT".", 3, n);
		return NULL;
	}

	if (strlen(edid[0]) == 0 || strlen(edid[1]) == 0 || strlen(edid[2]) == 0) {
		g_warning ("EDID not completely defined.");
		return NULL;
	}

	return find_output_by_edid (edid[0], edid[1], edid[2]);
}

static GnomeRROutputInfo*
find_output_by_monitor (GdkScreen *screen, gint monitor)
{
	GError *error = NULL;
	GnomeRRScreen *rr_screen;
	GnomeRRConfig *rr_config;
	GnomeRROutputInfo **rr_output_info;

	if (monitor >= 0)
	{
		/* TODO: Check the value of 'error' */
		rr_screen = gnome_rr_screen_new (screen, &error);
		rr_config = gnome_rr_config_new_current (rr_screen, &error);
		rr_output_info = gnome_rr_config_get_outputs (rr_config);

		for (; *rr_output_info != NULL; rr_output_info++) {
			int x, y, w, h;

			if (!gnome_rr_output_info_is_active (*rr_output_info))
				continue;

			gnome_rr_output_info_get_geometry (*rr_output_info, &x, &y, &w, &h);
			if (monitor == gdk_screen_get_monitor_at_point (screen, x, y))
				return *rr_output_info;
		}
	}

	g_warning ("No output found for monitor %d.", monitor);
	return NULL;
}

static void
set_display_by_output (GsdWacomDevice    *device,
                       GnomeRROutputInfo *rr_output_info)
{
	GSettings   *tablet;
	GVariant    *c_array;
	GVariant    *n_array;
	gsize        nvalues;
	gchar       *o_vendor, *o_product, *o_serial;
	const gchar *values[3];

	tablet  = gsd_wacom_device_get_settings (device);
	c_array = g_settings_get_value (tablet, "display");
	g_variant_get_strv (c_array, &nvalues);
	if (nvalues != 3) {
		g_warning("Unable set set display property. Got %"G_GSIZE_FORMAT" items; expected %d items.\n", nvalues, 4);
		return;
	}

	if (rr_output_info == NULL)
	{
		o_vendor  = g_strdup("");
		o_product = g_strdup("");
		o_serial  = g_strdup("");
	}
	else
	{
		o_vendor = g_malloc0 (4);
		gnome_rr_output_info_get_vendor (rr_output_info, o_vendor);
		o_product = g_strdup_printf ("%d", gnome_rr_output_info_get_product (rr_output_info));
		o_serial  = g_strdup_printf ("%d", gnome_rr_output_info_get_serial  (rr_output_info));
	}

	values[0] = o_vendor;
	values[1] = o_product;
	values[2] = o_serial;
	n_array = g_variant_new_strv((const gchar * const *) &values, 3);
	g_settings_set_value (tablet, "display", n_array);

	g_free (o_vendor);
	g_free (o_product);
	g_free (o_serial);
}


void
gsd_wacom_device_set_display (GsdWacomDevice *device,
                              gint            monitor)
{
	GnomeRROutputInfo *output;

	output = find_output_by_monitor (gdk_screen_get_default (), monitor);
	set_display_by_output (device, output);
}

static GnomeRROutputInfo*
find_output (GsdWacomDevice *device)
{
	GnomeRROutputInfo *rr_output_info;

	rr_output_info = find_output_by_display(device);

	if (rr_output_info == NULL)
	{
		g_warning ("No strict EDID match was found.");

		if (gsd_wacom_device_is_screen_tablet (device))
		{
			rr_output_info = find_output_by_heuristic (device);
			if (rr_output_info == NULL)
			{
				g_warning ("No fuzzy match based on heuristics was found.");
			}
			else
			{
				g_warning("Automatically mapping tablet to heuristically-found display.");
				set_display_by_output (device, rr_output_info);
			}
		}
	}

	return rr_output_info;
}

static void
calculate_transformation_matrix (const GdkRectangle mapped, const GdkRectangle desktop, float matrix[NUM_ELEMS_MATRIX])
{
	float x_scale = (float)mapped.x / desktop.width;
	float y_scale = (float)mapped.y / desktop.height;
	float width_scale  = (float)mapped.width / desktop.width;
	float height_scale = (float)mapped.height / desktop.height;

	matrix[0] = width_scale;
	matrix[1] = 0.0f;
	matrix[2] = x_scale;

	matrix[3] = 0.0f;
	matrix[4] = height_scale;
	matrix[5] = y_scale;

	matrix[6] = 0.0f;
	matrix[7] = 0.0f;
	matrix[8] = 1.0f;

	g_debug ("Matrix is %f,%f,%f,%f,%f,%f,%f,%f,%f.",
	         matrix[0], matrix[1], matrix[2],
	         matrix[3], matrix[4], matrix[5],
	         matrix[6], matrix[7], matrix[8]);

	return;
}

gint
gsd_wacom_device_get_display_monitor (GsdWacomDevice *device)
{
	gint area[4];
	GnomeRROutputInfo *rr_output_info;

	rr_output_info = find_output(device);
	if (rr_output_info == NULL)
		return -1;

	if (!gnome_rr_output_info_is_active (rr_output_info))
	{
		g_warning ("Output is not active.");
		return -1;
	}

	gnome_rr_output_info_get_geometry (rr_output_info, &area[0], &area[1], &area[2], &area[3]);
	if (area[2] <= 0 || area[3] <= 0)
	{
		g_warning ("Output has non-positive area.");
		return -1;
	}

	g_debug ("Area: %d,%d %dx%d", area[0], area[1], area[2], area[3]);
	return gdk_screen_get_monitor_at_point (gdk_screen_get_default (), area[0], area[1]);
}

gboolean
gsd_wacom_device_get_display_matrix (GsdWacomDevice *device, float matrix[NUM_ELEMS_MATRIX])
{
	int monitor;
	GdkRectangle display;
	GdkRectangle desktop;
	GdkScreen *screen = gdk_screen_get_default ();

	matrix[0] = 1.0f;
	matrix[1] = 0.0f;
	matrix[2] = 0.0f;
	matrix[3] = 0.0f;
	matrix[4] = 1.0f;
	matrix[5] = 0.0f;
	matrix[6] = 0.0f;
	matrix[7] = 0.0f;
	matrix[8] = 1.0f;

	monitor = gsd_wacom_device_get_display_monitor (device);
	if (monitor < 0)
		return FALSE;

	desktop.x = 0;
	desktop.y = 0;
	desktop.width = gdk_screen_get_width (screen);
	desktop.height = gdk_screen_get_height (screen);

	gdk_screen_get_monitor_geometry (screen, monitor, &display);
	calculate_transformation_matrix (display, desktop, matrix);
	return TRUE;
}

static void
add_stylus_to_device (GsdWacomDevice *device,
		      const char     *settings_path,
		      int             id)
{
	const WacomStylus *wstylus;

	wstylus = libwacom_stylus_get_for_id (db, id);
	if (wstylus) {
		GsdWacomStylus *stylus;
		char *stylus_settings_path;
		GSettings *settings;

		if (device->priv->type == WACOM_TYPE_STYLUS &&
		    libwacom_stylus_is_eraser (wstylus))
			return;
		if (device->priv->type == WACOM_TYPE_ERASER &&
		    libwacom_stylus_is_eraser (wstylus) == FALSE)
			return;

		stylus_settings_path = g_strdup_printf ("%s0x%x/", settings_path, id);
		if (device->priv->type == WACOM_TYPE_STYLUS) {
			settings = g_settings_new_with_path (WACOM_STYLUS_SCHEMA, stylus_settings_path);
			stylus = gsd_wacom_stylus_new (device, wstylus, settings);
		} else {
			settings = g_settings_new_with_path (WACOM_ERASER_SCHEMA, stylus_settings_path);
			stylus = gsd_wacom_stylus_new (device, wstylus, settings);
		}
		g_free (stylus_settings_path);
		device->priv->styli = g_list_prepend (device->priv->styli, stylus);
	}
}

static void
gsd_wacom_device_update_from_db (GsdWacomDevice *device,
				 WacomDevice    *wacom_device,
				 const char     *identifier)
{
	char *settings_path;

	settings_path = g_strdup_printf (WACOM_DEVICE_CONFIG_BASE, libwacom_get_match (wacom_device));
	device->priv->wacom_settings = g_settings_new_with_path (WACOM_TABLET_SCHEMA,
								 settings_path);

	device->priv->name = get_device_name (wacom_device);
	device->priv->reversible = libwacom_is_reversible (wacom_device);
	device->priv->is_screen_tablet = libwacom_is_builtin (wacom_device);
	if (device->priv->is_screen_tablet) {
		if (libwacom_get_class (wacom_device) == WCLASS_CINTIQ)
			device->priv->icon_name = g_strdup ("wacom-tablet-cintiq");
		else
			device->priv->icon_name = g_strdup ("wacom-tablet-pc");
	} else {
		device->priv->icon_name = g_strdup ("wacom-tablet");
	}

	if (device->priv->type == WACOM_TYPE_STYLUS ||
	    device->priv->type == WACOM_TYPE_ERASER) {
		int *ids;
		int num_styli;
		guint i;

		ids = libwacom_get_supported_styli(wacom_device, &num_styli);
		for (i = 0; i < num_styli; i++)
			add_stylus_to_device (device, settings_path, ids[i]);
		/* Create a fallback stylus if we don't have one */
		if (num_styli == 0)
			add_stylus_to_device (device, settings_path,
					      device->priv->type == WACOM_TYPE_STYLUS ?
					      WACOM_STYLUS_FALLBACK_ID : WACOM_ERASER_FALLBACK_ID);

		device->priv->styli = g_list_reverse (device->priv->styli);
	}
	g_free (settings_path);
}

static GObject *
gsd_wacom_device_constructor (GType                     type,
                              guint                      n_construct_properties,
                              GObjectConstructParam     *construct_properties)
{
        GsdWacomDevice *device;
        GdkDeviceManager *device_manager;
        XDeviceInfo *device_info;
        WacomDevice *wacom_device;
        int n_devices;
        guint i;
        char *path;

        device = GSD_WACOM_DEVICE (G_OBJECT_CLASS (gsd_wacom_device_parent_class)->constructor (type,
												n_construct_properties,
												construct_properties));

	if (device->priv->gdk_device == NULL)
		return G_OBJECT (device);

	device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
	g_object_get (device_manager, "opcode", &device->priv->opcode, NULL);

        g_object_get (device->priv->gdk_device, "device-id", &device->priv->device_id, NULL);

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL) {
		g_warning ("Could not list any input devices through XListInputDevices()");
		goto end;
	}

        for (i = 0; i < n_devices; i++) {
		if (device_info[i].id == device->priv->device_id) {
			device->priv->type = get_device_type (&device_info[i]);
			device->priv->tool_name = g_strdup (device_info[i].name);
			break;
		}
	}

	XFreeDeviceList (device_info);

	if (device->priv->type == WACOM_TYPE_INVALID)
		goto end;

	path = xdevice_get_device_node (device->priv->device_id);
	if (path == NULL) {
		g_warning ("Could not get the device node path for ID '%d'", device->priv->device_id);
		device->priv->type = WACOM_TYPE_INVALID;
		goto end;
	}

	if (db == NULL)
		db = libwacom_database_new ();

	wacom_device = libwacom_new_from_path (db, path, FALSE, NULL);
	if (!wacom_device) {
		WacomError *wacom_error;

		g_debug ("Creating fallback driver for wacom tablet '%s' ('%s')",
			 gdk_device_get_name (device->priv->gdk_device),
			 path);

		wacom_error = libwacom_error_new ();
		wacom_device = libwacom_new_from_path (db, path, TRUE, wacom_error);
		if (wacom_device == NULL) {
			g_warning ("Failed to create fallback wacom device for '%s': %s (%d)",
				   path,
				   libwacom_error_get_message (wacom_error),
				   libwacom_error_get_code (wacom_error));
			g_free (path);
			libwacom_error_free (&wacom_error);
			device->priv->type = WACOM_TYPE_INVALID;
			goto end;
		}
	}

	gsd_wacom_device_update_from_db (device, wacom_device, path);
	libwacom_destroy (wacom_device);
	g_free (path);

	if (device->priv->type == WACOM_TYPE_STYLUS ||
	    device->priv->type == WACOM_TYPE_ERASER) {
		setup_property_notify (device);
	}

end:
        return G_OBJECT (device);
}

static void
gsd_wacom_device_set_property (GObject        *object,
                               guint           prop_id,
                               const GValue   *value,
                               GParamSpec     *pspec)
{
        GsdWacomDevice *device;

        device = GSD_WACOM_DEVICE (object);

        switch (prop_id) {
	case PROP_GDK_DEVICE:
		device->priv->gdk_device = g_value_get_pointer (value);
		break;
	case PROP_LAST_STYLUS:
		device->priv->last_stylus = g_value_get_pointer (value);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_wacom_device_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        GsdWacomDevice *device;

        device = GSD_WACOM_DEVICE (object);

        switch (prop_id) {
	case PROP_GDK_DEVICE:
		g_value_set_pointer (value, device->priv->gdk_device);
		break;
	case PROP_LAST_STYLUS:
		g_value_set_pointer (value, device->priv->last_stylus);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_wacom_device_class_init (GsdWacomDeviceClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gsd_wacom_device_constructor;
        object_class->finalize = gsd_wacom_device_finalize;
        object_class->set_property = gsd_wacom_device_set_property;
        object_class->get_property = gsd_wacom_device_get_property;

        g_type_class_add_private (klass, sizeof (GsdWacomDevicePrivate));

	g_object_class_install_property (object_class, PROP_GDK_DEVICE,
					 g_param_spec_pointer ("gdk-device", "gdk-device", "gdk-device",
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_LAST_STYLUS,
					 g_param_spec_pointer ("last-stylus", "last-stylus", "last-stylus",
							       G_PARAM_READWRITE));
}

static void
gsd_wacom_device_init (GsdWacomDevice *device)
{
        device->priv = GSD_WACOM_DEVICE_GET_PRIVATE (device);
        device->priv->type = WACOM_TYPE_INVALID;
}

static void
gsd_wacom_device_finalize (GObject *object)
{
        GsdWacomDevice *device;
        GsdWacomDevicePrivate *p;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_WACOM_DEVICE (object));

        device = GSD_WACOM_DEVICE (object);

        g_return_if_fail (device->priv != NULL);

	p = device->priv;

        if (p->wacom_settings != NULL) {
                g_object_unref (p->wacom_settings);
                p->wacom_settings = NULL;
        }

        g_free (p->name);
        p->name = NULL;

        g_free (p->tool_name);
        p->tool_name = NULL;

        g_free (p->icon_name);
        p->icon_name = NULL;

	gdk_window_remove_filter (NULL,
				  (GdkFilterFunc) filter_events,
				  device);

        G_OBJECT_CLASS (gsd_wacom_device_parent_class)->finalize (object);
}

GsdWacomDevice *
gsd_wacom_device_new (GdkDevice *device)
{
	return GSD_WACOM_DEVICE (g_object_new (GSD_TYPE_WACOM_DEVICE,
					       "gdk-device", device,
					       NULL));
}

GList *
gsd_wacom_device_list_styli (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return g_list_copy (device->priv->styli);
}

GsdWacomStylus *
gsd_wacom_device_get_stylus_for_type (GsdWacomDevice     *device,
				      GsdWacomStylusType  type)
{
	GList *l;

	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	for (l = device->priv->styli; l != NULL; l = l->next) {
		GsdWacomStylus *stylus = l->data;

		if (gsd_wacom_stylus_get_stylus_type (stylus) == type)
			return stylus;
	}
	return NULL;
}

const char *
gsd_wacom_device_get_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->name;
}

const char *
gsd_wacom_device_get_icon_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->icon_name;
}

const char *
gsd_wacom_device_get_tool_name (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->tool_name;
}

gboolean
gsd_wacom_device_reversible (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), FALSE);

	return device->priv->reversible;
}

gboolean
gsd_wacom_device_is_screen_tablet (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), FALSE);

	return device->priv->is_screen_tablet;
}

GSettings *
gsd_wacom_device_get_settings (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), NULL);

	return device->priv->wacom_settings;
}

void
gsd_wacom_device_set_current_stylus (GsdWacomDevice *device,
				     int             stylus_id)
{
	GList *l;

	g_return_if_fail (GSD_IS_WACOM_DEVICE (device));

	/* Don't change anything if the stylus is already set */
	if (device->priv->last_stylus != NULL) {
		GsdWacomStylus *stylus = device->priv->last_stylus;
		if (stylus->priv->id == stylus_id)
			return;
	}

	for (l = device->priv->styli; l; l = l->next) {
		GsdWacomStylus *stylus = l->data;

		/* Set a nice default if 0x0 */
		if (stylus_id == 0x0 &&
		    stylus->priv->type == WSTYLUS_GENERAL) {
			g_object_set (device, "last-stylus", stylus, NULL);
			return;
		}

		if (stylus->priv->id == stylus_id) {
			g_object_set (device, "last-stylus", stylus, NULL);
			return;
		}
	}

	g_warning ("Could not find stylus ID 0x%x for tablet '%s'", stylus_id, device->priv->name);
}

GsdWacomDeviceType
gsd_wacom_device_get_device_type (GsdWacomDevice *device)
{
	g_return_val_if_fail (GSD_IS_WACOM_DEVICE (device), WACOM_TYPE_INVALID);

	return device->priv->type;
}

gint *
gsd_wacom_device_get_area (GsdWacomDevice *device)
{
	int i, id;
	XDevice *xdevice;
	Atom area, realtype;
	int rc, realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;
	gint *device_area;

	g_object_get (device->priv->gdk_device, "device-id", &id, NULL);

	area = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Wacom Tablet Area", False);

	gdk_error_trap_push ();
	xdevice = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), id);
	if (gdk_error_trap_pop () || (device == NULL))
		return NULL;

	gdk_error_trap_push ();
	rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
				 xdevice, area, 0, 4, False,
				 XA_INTEGER, &realtype, &realformat, &nitems,
				 &bytes_after, &data);
	if (gdk_error_trap_pop () || rc != Success || realtype == None || bytes_after != 0 || nitems != 4) {
		XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdevice);
		return NULL;
	}

	device_area = g_new0 (int, nitems);
	for (i = 0; i < nitems; i++)
		device_area[i] = ((long*)data)[i];

	XFree (data);
	XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdevice);

	return device_area;
}

const char *
gsd_wacom_device_type_to_string (GsdWacomDeviceType type)
{
	switch (type) {
	case WACOM_TYPE_INVALID:
		return "Invalid";
	case WACOM_TYPE_STYLUS:
		return "Stylus";
	case WACOM_TYPE_ERASER:
		return "Eraser";
	case WACOM_TYPE_CURSOR:
		return "Cursor";
	case WACOM_TYPE_PAD:
		return "Pad";
	default:
		return "Unknown type";
	}
}

GsdWacomDevice *
gsd_wacom_device_create_fake (GsdWacomDeviceType  type,
			      const char         *name,
			      const char         *tool_name)
{
	GsdWacomDevice *device;
	GsdWacomDevicePrivate *priv;
	WacomDevice *wacom_device;

	device = GSD_WACOM_DEVICE (g_object_new (GSD_TYPE_WACOM_DEVICE, NULL));

	if (db == NULL)
		db = libwacom_database_new ();

	wacom_device = libwacom_new_from_name (db, name, NULL);
	if (wacom_device == NULL)
		return NULL;

	priv = device->priv;
	priv->type = type;
	priv->tool_name = g_strdup (tool_name);
	gsd_wacom_device_update_from_db (device, wacom_device, name);
	libwacom_destroy (wacom_device);

	return device;
}

GList *
gsd_wacom_device_create_fake_cintiq (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 stylus");
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 eraser");
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_PAD,
					       "Cintiq 21UX2",
					       "Wacom Cintiq 21UX2 pad");
	devices = g_list_prepend (devices, device);

	return devices;
}

GList *
gsd_wacom_device_create_fake_bt (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Graphire Wireless",
					       "Graphire Wireless stylus");
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Graphire Wireless",
					       "Graphire Wireless eraser");
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_PAD,
					       "Graphire Wireless",
					       "Graphire Wireless pad");
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_CURSOR,
					       "Graphire Wireless",
					       "Graphire Wireless cursor");
	devices = g_list_prepend (devices, device);

	return devices;
}

GList *
gsd_wacom_device_create_fake_x201 (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Serial Tablet WACf004",
					       "Serial Tablet WACf004 stylus");
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Serial Tablet WACf004",
					       "Serial Tablet WACf004 eraser");
	devices = g_list_prepend (devices, device);

	return devices;
}

GList *
gsd_wacom_device_create_fake_intuos4 (void)
{
	GsdWacomDevice *device;
	GList *devices;

	device = gsd_wacom_device_create_fake (WACOM_TYPE_STYLUS,
					       "Intuos 4 M 6x9",
					       "Wacom Intuos4 6x9 stylus");
	devices = g_list_prepend (NULL, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_ERASER,
					       "Intuos 4 M 6x9",
					       "Wacom Intuos4 6x9 eraser");
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_PAD,
					       "Intuos 4 M 6x9",
					       "Wacom Intuos4 6x9 pad");
	devices = g_list_prepend (devices, device);

	device = gsd_wacom_device_create_fake (WACOM_TYPE_CURSOR,
					       "Intuos 4 M 6x9",
					       "Wacom Intuos4 6x9 cursor");
	devices = g_list_prepend (devices, device);

	return devices;
}
