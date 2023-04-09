/*
 * lapiz-progress-message-area.c
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
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

/*
 * Modified by the lapiz Team, 2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

 /* TODO: add properties */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <gdk/gdk.h>

#include "lapiz-progress-message-area.h"

enum {
	PROP_0,
	PROP_HAS_CANCEL_BUTTON
};

struct _LapizProgressMessageAreaPrivate
{
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *progress;
};

G_DEFINE_TYPE_WITH_PRIVATE (LapizProgressMessageArea, lapiz_progress_message_area, CTK_TYPE_INFO_BAR)

static void
lapiz_progress_message_area_set_has_cancel_button (LapizProgressMessageArea *area,
						   gboolean                  has_button)
{
	if (has_button)
		ctk_button_set_image (CTK_BUTTON (ctk_info_bar_add_button (CTK_INFO_BAR (area),
									   _("_Cancel"),
									   CTK_RESPONSE_CANCEL)),
				      ctk_image_new_from_icon_name ("process-stop", CTK_ICON_SIZE_BUTTON));

	g_object_notify (G_OBJECT (area), "has-cancel-button");
}

static void
lapiz_progress_message_area_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec)
{
	LapizProgressMessageArea *area;

	area = LAPIZ_PROGRESS_MESSAGE_AREA (object);

	switch (prop_id)
	{
	case PROP_HAS_CANCEL_BUTTON:
		lapiz_progress_message_area_set_has_cancel_button (area,
								   g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
lapiz_progress_message_area_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec)
{
	switch (prop_id)
	{
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
lapiz_progress_message_area_class_init (LapizProgressMessageAreaClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = lapiz_progress_message_area_set_property;
	gobject_class->get_property = lapiz_progress_message_area_get_property;

	g_object_class_install_property (gobject_class,
					 PROP_HAS_CANCEL_BUTTON,
					 g_param_spec_boolean ("has-cancel-button",
							       "Has Cancel Button",
							       "If the message area has a cancel button",
							       TRUE,
							       G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_STATIC_STRINGS));
}

static void
lapiz_progress_message_area_init (LapizProgressMessageArea *area)
{
	GtkWidget *vbox;
	GtkWidget *hbox;

	area->priv = lapiz_progress_message_area_get_instance_private (area);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_widget_show (vbox);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 4);
	ctk_widget_show (hbox);
	ctk_box_pack_start (CTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	area->priv->image = ctk_image_new_from_icon_name ("image-missing",
							  CTK_ICON_SIZE_SMALL_TOOLBAR);
	ctk_widget_show (area->priv->image);
	ctk_widget_set_halign (area->priv->image, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (area->priv->image, CTK_ALIGN_CENTER);
	ctk_box_pack_start (CTK_BOX (hbox), area->priv->image, FALSE, FALSE, 4);

	area->priv->label = ctk_label_new ("");
	ctk_widget_show (area->priv->label);
	ctk_box_pack_start (CTK_BOX (hbox), area->priv->label, TRUE, TRUE, 0);
	ctk_label_set_use_markup (CTK_LABEL (area->priv->label), TRUE);
	ctk_label_set_xalign (CTK_LABEL (area->priv->label), 0.0);
	ctk_label_set_ellipsize (CTK_LABEL (area->priv->label),
				 PANGO_ELLIPSIZE_END);

	area->priv->progress = ctk_progress_bar_new ();
	ctk_widget_show (area->priv->progress);
	ctk_box_pack_start (CTK_BOX (vbox), area->priv->progress, TRUE, FALSE, 0);
	ctk_widget_set_size_request (area->priv->progress, -1, 15);

	GtkWidget *content;

	content = ctk_info_bar_get_content_area (CTK_INFO_BAR (area));
	ctk_container_add (CTK_CONTAINER (content), vbox);
}

GtkWidget *
lapiz_progress_message_area_new (const gchar *image_id,
				 const gchar *markup,
				 gboolean     has_cancel)
{
	LapizProgressMessageArea *area;

	g_return_val_if_fail (image_id != NULL, NULL);
	g_return_val_if_fail (markup != NULL, NULL);

	area = LAPIZ_PROGRESS_MESSAGE_AREA (g_object_new (LAPIZ_TYPE_PROGRESS_MESSAGE_AREA,
							  "has-cancel-button", has_cancel,
							  NULL));

	lapiz_progress_message_area_set_image (area,
					       image_id);

	lapiz_progress_message_area_set_markup (area,
						markup);

	return CTK_WIDGET (area);
}

void
lapiz_progress_message_area_set_image (LapizProgressMessageArea *area,
				       const gchar              *image_id)
{
	g_return_if_fail (LAPIZ_IS_PROGRESS_MESSAGE_AREA (area));
	g_return_if_fail (image_id != NULL);

	ctk_image_set_from_icon_name (CTK_IMAGE (area->priv->image),
				      image_id,
				      CTK_ICON_SIZE_SMALL_TOOLBAR);
}

void
lapiz_progress_message_area_set_markup (LapizProgressMessageArea *area,
					const gchar              *markup)
{
	g_return_if_fail (LAPIZ_IS_PROGRESS_MESSAGE_AREA (area));
	g_return_if_fail (markup != NULL);

	ctk_label_set_markup (CTK_LABEL (area->priv->label),
			      markup);
}

void
lapiz_progress_message_area_set_text (LapizProgressMessageArea *area,
				      const gchar              *text)
{
	g_return_if_fail (LAPIZ_IS_PROGRESS_MESSAGE_AREA (area));
	g_return_if_fail (text != NULL);

	ctk_label_set_text (CTK_LABEL (area->priv->label),
			    text);
}

void
lapiz_progress_message_area_set_fraction (LapizProgressMessageArea *area,
					  gdouble                   fraction)
{
	g_return_if_fail (LAPIZ_IS_PROGRESS_MESSAGE_AREA (area));

	ctk_progress_bar_set_fraction (CTK_PROGRESS_BAR (area->priv->progress),
				       fraction);
}

void
lapiz_progress_message_area_pulse (LapizProgressMessageArea *area)
{
	g_return_if_fail (LAPIZ_IS_PROGRESS_MESSAGE_AREA (area));

	ctk_progress_bar_pulse (CTK_PROGRESS_BAR (area->priv->progress));
}
