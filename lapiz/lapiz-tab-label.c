/*
 * lapiz-tab-label.c
 * This file is part of lapiz
 *
 * Copyright (C) 2010 - Paolo Borelli
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include "lapiz-tab-label.h"
#include "lapiz-close-button.h"

/* Signals */
enum
{
	CLOSE_CLICKED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_TAB
};

struct _LapizTabLabelPrivate
{
	LapizTab *tab;

	CtkWidget *ebox;
	CtkWidget *close_button;
	CtkWidget *spinner;
	CtkWidget *icon;
	CtkWidget *label;

	gboolean close_button_sensitive;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (LapizTabLabel, lapiz_tab_label, CTK_TYPE_BOX)

static void
lapiz_tab_label_finalize (GObject *object)
{
	G_OBJECT_CLASS (lapiz_tab_label_parent_class)->finalize (object);
}

static void
lapiz_tab_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	LapizTabLabel *tab_label = LAPIZ_TAB_LABEL (object);

	switch (prop_id)
	{
		case PROP_TAB:
			tab_label->priv->tab = LAPIZ_TAB (g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_tab_label_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	LapizTabLabel *tab_label = LAPIZ_TAB_LABEL (object);

	switch (prop_id)
	{
		case PROP_TAB:
			g_value_set_object (value, tab_label->priv->tab);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
close_button_clicked_cb (CtkWidget     *widget,
			 LapizTabLabel *tab_label)
{
	g_signal_emit (tab_label, signals[CLOSE_CLICKED], 0, NULL);
}

static void
sync_tip (LapizTab *tab, LapizTabLabel *tab_label)
{
	gchar *str;

	str = _lapiz_tab_get_tooltips (tab);
	g_return_if_fail (str != NULL);

	ctk_widget_set_tooltip_markup (tab_label->priv->ebox, str);
	g_free (str);
}

static void
sync_name (LapizTab *tab, GParamSpec *pspec, LapizTabLabel *tab_label)
{
	gchar *str;

	g_return_if_fail (tab == tab_label->priv->tab);

	str = _lapiz_tab_get_name (tab);
	g_return_if_fail (str != NULL);

	ctk_label_set_text (CTK_LABEL (tab_label->priv->label), str);
	g_free (str);

	sync_tip (tab, tab_label);
}

static void
sync_state (LapizTab *tab, GParamSpec *pspec, LapizTabLabel *tab_label)
{
	LapizTabState  state;

	g_return_if_fail (tab == tab_label->priv->tab);

	state = lapiz_tab_get_state (tab);

	ctk_widget_set_sensitive (tab_label->priv->close_button,
				  tab_label->priv->close_button_sensitive &&
				  (state != LAPIZ_TAB_STATE_CLOSING) &&
				  (state != LAPIZ_TAB_STATE_SAVING)  &&
				  (state != LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				  (state != LAPIZ_TAB_STATE_SAVING_ERROR));

	if ((state == LAPIZ_TAB_STATE_LOADING)   ||
	    (state == LAPIZ_TAB_STATE_SAVING)    ||
	    (state == LAPIZ_TAB_STATE_REVERTING))
	{
		ctk_widget_hide (tab_label->priv->icon);

		ctk_widget_show (tab_label->priv->spinner);
		ctk_spinner_start (CTK_SPINNER (tab_label->priv->spinner));
	}
	else
	{
		GdkPixbuf *pixbuf;

		pixbuf = _lapiz_tab_get_icon (tab);
		ctk_image_set_from_pixbuf (CTK_IMAGE (tab_label->priv->icon), pixbuf);

		if (pixbuf != NULL)
			g_object_unref (pixbuf);

		ctk_widget_show (tab_label->priv->icon);

		ctk_widget_hide (tab_label->priv->spinner);
		ctk_spinner_stop (CTK_SPINNER (tab_label->priv->spinner));
	}

	/* sync tip since encoding is known only after load/save end */
	sync_tip (tab, tab_label);
}

static void
lapiz_tab_label_constructed (GObject *object)
{
	LapizTabLabel *tab_label = LAPIZ_TAB_LABEL (object);

	if (!tab_label->priv->tab)
	{
		g_critical ("The tab label was not properly constructed");
		return;
	}

	sync_name (tab_label->priv->tab, NULL, tab_label);
	sync_state (tab_label->priv->tab, NULL, tab_label);

	g_signal_connect_object (tab_label->priv->tab,
				 "notify::name",
				 G_CALLBACK (sync_name),
				 tab_label,
				 0);

	g_signal_connect_object (tab_label->priv->tab,
				 "notify::state",
				 G_CALLBACK (sync_state),
				 tab_label,
				 0);
}

static void
lapiz_tab_label_class_init (LapizTabLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_tab_label_finalize;
	object_class->set_property = lapiz_tab_label_set_property;
	object_class->get_property = lapiz_tab_label_get_property;
	object_class->constructed = lapiz_tab_label_constructed;

	signals[CLOSE_CLICKED] =
		g_signal_new ("close-clicked",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (LapizTabLabelClass, close_clicked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_TAB,
					 g_param_spec_object ("tab",
							      "Tab",
							      "The LapizTab",
							      LAPIZ_TYPE_TAB,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static void
lapiz_tab_label_init (LapizTabLabel *tab_label)
{
	CtkWidget *ebox;
	CtkWidget *hbox;
	CtkWidget *close_button;
	CtkWidget *spinner;
	CtkWidget *icon;
	CtkWidget *label;
	CtkWidget *dummy_label;

	tab_label->priv = lapiz_tab_label_get_instance_private (tab_label);

	tab_label->priv->close_button_sensitive = TRUE;

	ctk_orientable_set_orientation (CTK_ORIENTABLE (tab_label),
	                                CTK_ORIENTATION_HORIZONTAL);

	ebox = ctk_event_box_new ();
	ctk_widget_add_events (ebox, GDK_SCROLL_MASK);
	ctk_event_box_set_visible_window (CTK_EVENT_BOX (ebox), FALSE);
	ctk_box_pack_start (CTK_BOX (tab_label), ebox, TRUE, TRUE, 0);
	tab_label->priv->ebox = ebox;

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 4);
	ctk_container_add (CTK_CONTAINER (ebox), hbox);

	close_button = lapiz_close_button_new ();
	ctk_widget_add_events (close_button, GDK_SCROLL_MASK);
	ctk_widget_set_tooltip_text (close_button, _("Close document"));
	ctk_box_pack_start (CTK_BOX (tab_label), close_button, FALSE, FALSE, 0);
	tab_label->priv->close_button = close_button;

	g_signal_connect (close_button,
			  "clicked",
			  G_CALLBACK (close_button_clicked_cb),
			  tab_label);

	spinner = ctk_spinner_new ();
	ctk_box_pack_start (CTK_BOX (hbox), spinner, FALSE, FALSE, 0);
	tab_label->priv->spinner = spinner;

	/* setup icon, empty by default */
	icon = ctk_image_new ();
	ctk_box_pack_start (CTK_BOX (hbox), icon, FALSE, FALSE, 0);
	tab_label->priv->icon = icon;

	label = ctk_label_new ("");
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_widget_set_margin_start (label, 0);
	ctk_widget_set_margin_end (label, 0);
	ctk_widget_set_margin_top (label, 0);
	ctk_widget_set_margin_bottom (label, 0);
	ctk_box_pack_start (CTK_BOX (hbox), label, FALSE, FALSE, 0);
	tab_label->priv->label = label;

	dummy_label = ctk_label_new ("");
	ctk_box_pack_start (CTK_BOX (hbox), dummy_label, TRUE, TRUE, 0);

	ctk_widget_show (ebox);
	ctk_widget_show (hbox);
	ctk_widget_show (close_button);
	ctk_widget_show (icon);
	ctk_widget_show (label);
	ctk_widget_show (dummy_label);
}

void
lapiz_tab_label_set_close_button_sensitive (LapizTabLabel *tab_label,
					    gboolean       sensitive)
{
	LapizTabState state;

	g_return_if_fail (LAPIZ_IS_TAB_LABEL (tab_label));

	sensitive = (sensitive != FALSE);

	if (sensitive == tab_label->priv->close_button_sensitive)
		return;

	tab_label->priv->close_button_sensitive = sensitive;

	state = lapiz_tab_get_state (tab_label->priv->tab);

	ctk_widget_set_sensitive (tab_label->priv->close_button,
				  tab_label->priv->close_button_sensitive &&
				  (state != LAPIZ_TAB_STATE_CLOSING) &&
				  (state != LAPIZ_TAB_STATE_SAVING)  &&
				  (state != LAPIZ_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				  (state != LAPIZ_TAB_STATE_PRINTING) &&
				  (state != LAPIZ_TAB_STATE_PRINT_PREVIEWING) &&
				  (state != LAPIZ_TAB_STATE_SAVING_ERROR));
}

LapizTab *
lapiz_tab_label_get_tab (LapizTabLabel *tab_label)
{
	g_return_val_if_fail (LAPIZ_IS_TAB_LABEL (tab_label), NULL);

	return tab_label->priv->tab;
}

CtkWidget *
lapiz_tab_label_new (LapizTab *tab)
{
	LapizTabLabel *tab_label;

	tab_label = g_object_new (LAPIZ_TYPE_TAB_LABEL,
				  "homogeneous", FALSE,
				  "tab", tab,
				  NULL);

	return CTK_WIDGET (tab_label);
}
