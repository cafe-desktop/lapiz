/*
 * lapiz-status-combo-box.c
 * This file is part of lapiz
 *
 * Copyright (C) 2008 - Jesse van den Kieboom
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "lapiz-status-combo-box.h"

#define COMBO_BOX_TEXT_DATA "LapizStatusComboBoxTextData"

struct _LapizStatusComboBoxPrivate
{
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *item;
	GtkWidget *arrow;

	GtkWidget *menu;
	GtkWidget *current_item;
};

/* Signals */
enum
{
	CHANGED,
	NUM_SIGNALS
};

/* Properties */
enum
{
	PROP_0,

	PROP_LABEL
};

static guint signals[NUM_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (LapizStatusComboBox, lapiz_status_combo_box, CTK_TYPE_EVENT_BOX)

static void
lapiz_status_combo_box_finalize (GObject *object)
{
	G_OBJECT_CLASS (lapiz_status_combo_box_parent_class)->finalize (object);
}

static void
lapiz_status_combo_box_get_property (GObject    *object,
			             guint       prop_id,
			             GValue     *value,
			             GParamSpec *pspec)
{
	LapizStatusComboBox *obj = LAPIZ_STATUS_COMBO_BOX (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			g_value_set_string (value, lapiz_status_combo_box_get_label (obj));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_status_combo_box_set_property (GObject      *object,
			             guint         prop_id,
			             const GValue *value,
			             GParamSpec   *pspec)
{
	LapizStatusComboBox *obj = LAPIZ_STATUS_COMBO_BOX (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			lapiz_status_combo_box_set_label (obj, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_status_combo_box_constructed (GObject *object)
{
	LapizStatusComboBox *combo = LAPIZ_STATUS_COMBO_BOX (object);
	GtkStyleContext *context;
	GtkCssProvider *css;
	GError *error = NULL;
	const gchar style[] =
		"* {\n"
		"	padding: 0;\n"
		"}";

	/* make it as small as possible */
	css = ctk_css_provider_new ();
	if (!ctk_css_provider_load_from_data (css, style, -1, &error))
	{
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	context = ctk_widget_get_style_context (CTK_WIDGET (combo));
	ctk_style_context_add_provider (context, CTK_STYLE_PROVIDER (css),
	                                CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (css);
}

static void
lapiz_status_combo_box_changed (LapizStatusComboBox *combo,
				GtkMenuItem         *item)
{
	const gchar *text;

	text = g_object_get_data (G_OBJECT (item), COMBO_BOX_TEXT_DATA);

	if (text != NULL)
	{
		ctk_label_set_markup (CTK_LABEL (combo->priv->item), text);
		combo->priv->current_item = CTK_WIDGET (item);
	}
}

static void
lapiz_status_combo_box_class_init (LapizStatusComboBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_status_combo_box_finalize;
	object_class->get_property = lapiz_status_combo_box_get_property;
	object_class->set_property = lapiz_status_combo_box_set_property;
	object_class->constructed = lapiz_status_combo_box_constructed;

	klass->changed = lapiz_status_combo_box_changed;

	signals[CHANGED] =
	    g_signal_new ("changed",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizStatusComboBoxClass,
					   changed), NULL, NULL,
			  g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
			  CTK_TYPE_MENU_ITEM);

	g_object_class_install_property (object_class, PROP_LABEL,
					 g_param_spec_string ("label",
					 		      "LABEL",
					 		      "The label",
					 		      NULL,
					 		      G_PARAM_READWRITE));
}

static void
menu_deactivate (GtkMenu             *menu,
		 LapizStatusComboBox *combo)
{
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (combo->priv->button), FALSE);
}

static void
button_press_event (GtkWidget           *widget,
		    GdkEventButton      *event,
		    LapizStatusComboBox *combo)
{
	GtkRequisition request;
	GtkAllocation allocation;
	gint max_height;

	ctk_widget_get_preferred_size (combo->priv->menu, NULL, &request);
	ctk_widget_get_allocation (CTK_WIDGET (combo), &allocation);

	/* do something relative to our own height here, maybe we can do better */
	max_height = allocation.height * 20;

	if (request.height > max_height)
	{
		ctk_widget_set_size_request (combo->priv->menu, -1, max_height);
		ctk_widget_set_size_request (ctk_widget_get_toplevel (combo->priv->menu), -1, max_height);
	}

	ctk_menu_popup_at_widget (CTK_MENU (combo->priv->menu),
				  ctk_widget_get_parent (widget),
				  GDK_GRAVITY_NORTH_WEST,
				  GDK_GRAVITY_SOUTH_WEST,
				  (const GdkEvent*) event);

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (combo->priv->button), TRUE);

	if (combo->priv->current_item)
	{
		ctk_menu_shell_select_item (CTK_MENU_SHELL (combo->priv->menu),
					    combo->priv->current_item);
	}
}

static void
set_shadow_type (LapizStatusComboBox *combo)
{
	GtkStyleContext *context;
	GtkShadowType shadow_type;
	GtkWidget *statusbar;

	/* This is a hack needed to use the shadow type of a statusbar */
	statusbar = ctk_statusbar_new ();
	context = ctk_widget_get_style_context (statusbar);

	ctk_style_context_get_style (context, "shadow-type", &shadow_type, NULL);
	ctk_frame_set_shadow_type (CTK_FRAME (combo->priv->frame), shadow_type);

	ctk_widget_destroy (statusbar);
}

static void
lapiz_status_combo_box_init (LapizStatusComboBox *self)
{
	self->priv = lapiz_status_combo_box_get_instance_private (self);

	ctk_event_box_set_visible_window (CTK_EVENT_BOX (self), TRUE);

	self->priv->frame = ctk_frame_new (NULL);
	ctk_widget_show (self->priv->frame);

	self->priv->button = ctk_toggle_button_new ();
	ctk_widget_set_name (self->priv->button, "lapiz-status-combo-button");
	ctk_button_set_relief (CTK_BUTTON (self->priv->button), CTK_RELIEF_NONE);
	ctk_widget_show (self->priv->button);

	set_shadow_type (self);

	self->priv->hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 3);
	ctk_widget_show (self->priv->hbox);

	ctk_container_add (CTK_CONTAINER (self), self->priv->frame);
	ctk_container_add (CTK_CONTAINER (self->priv->frame), self->priv->button);
	ctk_container_add (CTK_CONTAINER (self->priv->button), self->priv->hbox);

	self->priv->label = ctk_label_new ("");
	ctk_widget_show (self->priv->label);

	ctk_label_set_single_line_mode (CTK_LABEL (self->priv->label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (self->priv->label), 0.0);

	ctk_box_pack_start (CTK_BOX (self->priv->hbox), self->priv->label, FALSE, TRUE, 0);

	self->priv->item = ctk_label_new ("");
	ctk_widget_show (self->priv->item);

	ctk_label_set_single_line_mode (CTK_LABEL (self->priv->item), TRUE);
	ctk_widget_set_halign (self->priv->item, CTK_ALIGN_START);

	ctk_box_pack_start (CTK_BOX (self->priv->hbox), self->priv->item, TRUE, TRUE, 0);

	self->priv->arrow = ctk_image_new_from_icon_name ("pan-down-symbolic", CTK_ICON_SIZE_BUTTON);
	ctk_widget_show (self->priv->arrow);
	ctk_widget_set_halign (self->priv->arrow, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (self->priv->arrow, CTK_ALIGN_CENTER);

	ctk_box_pack_start (CTK_BOX (self->priv->hbox), self->priv->arrow, FALSE, TRUE, 0);

	self->priv->menu = ctk_menu_new ();
	g_object_ref_sink (self->priv->menu);

	g_signal_connect (self->priv->button,
			  "button-press-event",
			  G_CALLBACK (button_press_event),
			  self);
	g_signal_connect (self->priv->menu,
			  "deactivate",
			  G_CALLBACK (menu_deactivate),
			  self);
}

/* public functions */

/**
 * lapiz_status_combo_box_new:
 * @label: (allow-none):
 */
GtkWidget *
lapiz_status_combo_box_new (const gchar *label)
{
	return g_object_new (LAPIZ_TYPE_STATUS_COMBO_BOX, "label", label, NULL);
}

/**
 * lapiz_status_combo_box_set_label:
 * @combo:
 * @label: (allow-none):
 */
void
lapiz_status_combo_box_set_label (LapizStatusComboBox *combo,
				  const gchar         *label)
{
	gchar *text;

	g_return_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo));

	text = g_strconcat ("  ", label, ": ", NULL);
	ctk_label_set_markup (CTK_LABEL (combo->priv->label), text);
	g_free (text);
}

const gchar *
lapiz_status_combo_box_get_label (LapizStatusComboBox *combo)
{
	g_return_val_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo), NULL);

	return ctk_label_get_label (CTK_LABEL (combo->priv->label));
}

static void
item_activated (GtkMenuItem         *item,
		LapizStatusComboBox *combo)
{
	lapiz_status_combo_box_set_item (combo, item);
}

/**
 * lapiz_status_combo_box_add_item:
 * @combo:
 * @item:
 * @text: (allow-none):
 */
void
lapiz_status_combo_box_add_item (LapizStatusComboBox *combo,
				 GtkMenuItem         *item,
				 const gchar         *text)
{
	g_return_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo));
	g_return_if_fail (CTK_IS_MENU_ITEM (item));

	ctk_menu_shell_append (CTK_MENU_SHELL (combo->priv->menu), CTK_WIDGET (item));

	lapiz_status_combo_box_set_item_text (combo, item, text);
	g_signal_connect (item, "activate", G_CALLBACK (item_activated), combo);
}

void
lapiz_status_combo_box_remove_item (LapizStatusComboBox *combo,
				    GtkMenuItem         *item)
{
	g_return_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo));
	g_return_if_fail (CTK_IS_MENU_ITEM (item));

	ctk_container_remove (CTK_CONTAINER (combo->priv->menu),
			      CTK_WIDGET (item));
}

/**
 * lapiz_status_combo_box_get_items:
 * @combo:
 *
 * Returns: (element-type Gtk.Widget) (transfer container):
 */
GList *
lapiz_status_combo_box_get_items (LapizStatusComboBox *combo)
{
	g_return_val_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo), NULL);

	return ctk_container_get_children (CTK_CONTAINER (combo->priv->menu));
}

const gchar *
lapiz_status_combo_box_get_item_text (LapizStatusComboBox *combo,
				      GtkMenuItem	  *item)
{
	const gchar *ret = NULL;

	g_return_val_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo), NULL);
	g_return_val_if_fail (CTK_IS_MENU_ITEM (item), NULL);

	ret = g_object_get_data (G_OBJECT (item), COMBO_BOX_TEXT_DATA);

	return ret;
}

/**
 * lapiz_status_combo_box_set_item_text:
 * @combo:
 * @item:
 * @text: (allow-none):
 */
void
lapiz_status_combo_box_set_item_text (LapizStatusComboBox *combo,
				      GtkMenuItem	  *item,
				      const gchar         *text)
{
	g_return_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo));
	g_return_if_fail (CTK_IS_MENU_ITEM (item));

	g_object_set_data_full (G_OBJECT (item),
				COMBO_BOX_TEXT_DATA,
				g_strdup (text),
				(GDestroyNotify)g_free);
}

void
lapiz_status_combo_box_set_item (LapizStatusComboBox *combo,
				 GtkMenuItem         *item)
{
	g_return_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo));
	g_return_if_fail (CTK_IS_MENU_ITEM (item));

	g_signal_emit (combo, signals[CHANGED], 0, item, NULL);
}

GtkLabel *
lapiz_status_combo_box_get_item_label (LapizStatusComboBox *combo)
{
	g_return_val_if_fail (LAPIZ_IS_STATUS_COMBO_BOX (combo), NULL);

	return CTK_LABEL (combo->priv->item);
}

