/*
 * lapiz-panel.c
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

#include "lapiz-panel.h"

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdk.h>
#include <cdk/cdkkeysyms.h>

#include "lapiz-close-button.h"
#include "lapiz-window.h"
#include "lapiz-debug.h"

#define PANEL_ITEM_KEY "LapizPanelItemKey"

struct _LapizPanelPrivate
{
	CtkOrientation orientation;

	/* Title bar (vertical panel only) */
	CtkWidget *title_image;
	CtkWidget *title_label;

	/* Notebook */
	CtkWidget *notebook;
};

typedef struct _LapizPanelItem LapizPanelItem;

struct _LapizPanelItem
{
	gchar *name;
	CtkWidget *icon;
};

/* Properties */
enum {
	PROP_0,
	PROP_ORIENTATION
};

/* Signals */
enum {
	ITEM_ADDED,
	ITEM_REMOVED,
	CLOSE,
	FOCUS_DOCUMENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObject	*lapiz_panel_constructor	(GType type,
						 guint n_construct_properties,
						 GObjectConstructParam *construct_properties);


G_DEFINE_TYPE_WITH_PRIVATE (LapizPanel, lapiz_panel, CTK_TYPE_BOX)

static void
lapiz_panel_finalize (GObject *obj)
{
	if (G_OBJECT_CLASS (lapiz_panel_parent_class)->finalize)
		(*G_OBJECT_CLASS (lapiz_panel_parent_class)->finalize) (obj);
}

static void
lapiz_panel_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	LapizPanel *panel = LAPIZ_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			g_value_set_enum(value, panel->priv->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_panel_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	LapizPanel *panel = LAPIZ_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			panel->priv->orientation = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_panel_close (LapizPanel *panel)
{
	ctk_widget_hide (CTK_WIDGET (panel));
}

static void
lapiz_panel_focus_document (LapizPanel *panel)
{
	CtkWidget *toplevel = ctk_widget_get_toplevel (CTK_WIDGET (panel));
	if (ctk_widget_is_toplevel (toplevel) && LAPIZ_IS_WINDOW (toplevel))
	{
		LapizView *view;

		view = lapiz_window_get_active_view (LAPIZ_WINDOW (toplevel));
		if (view != NULL)
			ctk_widget_grab_focus (CTK_WIDGET (view));
	}
}

static void
lapiz_panel_grab_focus (CtkWidget *w)
{
	gint n;
	CtkWidget *tab;
	LapizPanel *panel = LAPIZ_PANEL (w);

	n = ctk_notebook_get_current_page (CTK_NOTEBOOK (panel->priv->notebook));
	if (n == -1)
		return;

	tab = ctk_notebook_get_nth_page (CTK_NOTEBOOK (panel->priv->notebook),
					 n);
	g_return_if_fail (tab != NULL);

	ctk_widget_grab_focus (tab);
}

static void
lapiz_panel_class_init (LapizPanelClass *klass)
{
	CtkBindingSet *binding_set;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

	object_class->constructor = lapiz_panel_constructor;
	object_class->finalize = lapiz_panel_finalize;
	object_class->get_property = lapiz_panel_get_property;
	object_class->set_property = lapiz_panel_set_property;

	g_object_class_install_property (object_class,
					 PROP_ORIENTATION,
					 g_param_spec_enum ("panel-orientation",
							    "Panel Orientation",
							    "The panel's orientation",
							    CTK_TYPE_ORIENTATION,
							    CTK_ORIENTATION_VERTICAL,
							    G_PARAM_WRITABLE |
							    G_PARAM_READABLE |
							    G_PARAM_CONSTRUCT_ONLY |
							    G_PARAM_STATIC_STRINGS));

	widget_class->grab_focus = lapiz_panel_grab_focus;

	klass->close = lapiz_panel_close;
	klass->focus_document = lapiz_panel_focus_document;

	signals[ITEM_ADDED] =
		g_signal_new ("item_added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizPanelClass, item_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      CTK_TYPE_WIDGET);
	signals[ITEM_REMOVED] =
		g_signal_new ("item_removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (LapizPanelClass, item_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      CTK_TYPE_WIDGET);

	/* Keybinding signals */
	signals[CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (LapizPanelClass, close),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[FOCUS_DOCUMENT] =
		g_signal_new ("focus_document",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (LapizPanelClass, focus_document),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	binding_set = ctk_binding_set_by_class (klass);

	ctk_binding_entry_add_signal (binding_set,
				      CDK_KEY_Escape,
				      0,
				      "close",
				      0);
	ctk_binding_entry_add_signal (binding_set,
				      CDK_KEY_Return,
				      CDK_CONTROL_MASK,
				      "focus_document",
				      0);
}

/* This is ugly, since it supports only known
 * storage types of CtkImage, otherwise fall back
 * to the empty icon.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=317520.
 */
static void
set_ctk_image_from_ctk_image (CtkImage *image,
			      CtkImage *source)
{
	switch (ctk_image_get_storage_type (source))
	{
	case CTK_IMAGE_EMPTY:
		ctk_image_clear (image);
		break;
	case CTK_IMAGE_PIXBUF:
		{
			GdkPixbuf *pb;

			pb = ctk_image_get_pixbuf (source);
			ctk_image_set_from_pixbuf (image, pb);
		}
		break;
	case CTK_IMAGE_ANIMATION:
		{
			GdkPixbufAnimation *a;

			a = ctk_image_get_animation (source);
			ctk_image_set_from_animation (image, a);
		}
		break;
	case CTK_IMAGE_ICON_NAME:
		{
			const gchar *n;
			CtkIconSize s;

			ctk_image_get_icon_name (source, &n, &s);
			ctk_image_set_from_icon_name (image, n, s);
		}
		break;
	default:
		ctk_image_set_from_icon_name (image,
		                              "text-x-generic",
		                              CTK_ICON_SIZE_MENU);
	}
}

static void
sync_title (LapizPanel     *panel,
	    LapizPanelItem *item)
{
	if (panel->priv->orientation != CTK_ORIENTATION_VERTICAL)
		return;

	if (item != NULL)
	{
		ctk_label_set_text (CTK_LABEL (panel->priv->title_label),
				    item->name);

		set_ctk_image_from_ctk_image (CTK_IMAGE (panel->priv->title_image),
					      CTK_IMAGE (item->icon));
	}
	else
	{
		ctk_label_set_text (CTK_LABEL (panel->priv->title_label),
				    _("Empty"));

		ctk_image_set_from_icon_name (CTK_IMAGE (panel->priv->title_image),
		                              "text-x-generic",
		                              CTK_ICON_SIZE_MENU);
	}
}

static void
notebook_page_changed (CtkNotebook     *notebook,
                       CtkWidget       *page G_GNUC_UNUSED,
                       guint            page_num,
                       LapizPanel      *panel)
{
	CtkWidget *item;
	LapizPanelItem *data;

	item = ctk_notebook_get_nth_page (notebook, page_num);
	g_return_if_fail (item != NULL);

	data = (LapizPanelItem *)g_object_get_data (G_OBJECT (item),
						    PANEL_ITEM_KEY);
	g_return_if_fail (data != NULL);

	sync_title (panel, data);
}

static void
panel_show (LapizPanel *panel,
	    gpointer    user_data G_GNUC_UNUSED)
{
	gint page;
	CtkNotebook *nb;

	nb = CTK_NOTEBOOK (panel->priv->notebook);

	page = ctk_notebook_get_current_page (nb);

	if (page != -1)
		notebook_page_changed (nb, NULL, page, panel);
}

static void
lapiz_panel_init (LapizPanel *panel)
{
	panel->priv = lapiz_panel_get_instance_private (panel);

	ctk_orientable_set_orientation (CTK_ORIENTABLE (panel), CTK_ORIENTATION_VERTICAL);
}

static void
close_button_clicked_cb (CtkWidget *widget G_GNUC_UNUSED,
			 CtkWidget *panel)
{
	ctk_widget_hide (panel);
}

static CtkWidget *
create_close_button (LapizPanel *panel)
{
	CtkWidget *button;

	button = lapiz_close_button_new ();

	ctk_widget_set_tooltip_text (button, _("Hide panel"));

	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (close_button_clicked_cb),
			  panel);

	return button;
}

static void
build_notebook_for_panel (LapizPanel *panel)
{
	/* Create the panel notebook */
	panel->priv->notebook = ctk_notebook_new ();

	ctk_notebook_set_tab_pos (CTK_NOTEBOOK (panel->priv->notebook),
				  CTK_POS_BOTTOM);
	ctk_notebook_set_scrollable (CTK_NOTEBOOK (panel->priv->notebook),
				     TRUE);
	ctk_notebook_popup_enable (CTK_NOTEBOOK (panel->priv->notebook));

	ctk_widget_show (CTK_WIDGET (panel->priv->notebook));

	g_signal_connect (panel->priv->notebook,
			  "switch-page",
			  G_CALLBACK (notebook_page_changed),
			  panel);
}

static void
build_horizontal_panel (LapizPanel *panel)
{
	CtkWidget *box;
	CtkWidget *sidebar;
	CtkWidget *close_button;

	box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

	ctk_box_pack_start (CTK_BOX (box),
			    panel->priv->notebook,
			    TRUE,
			    TRUE,
			    0);

	/* Toolbar, close button and first separator */
	sidebar = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
	ctk_container_set_border_width (CTK_CONTAINER (sidebar), 4);

	ctk_box_pack_start (CTK_BOX (box),
			    sidebar,
			    FALSE,
			    FALSE,
			    0);

	close_button = create_close_button (panel);

	ctk_box_pack_start (CTK_BOX (sidebar),
			    close_button,
			    FALSE,
			    FALSE,
			    0);

	ctk_widget_show_all (box);

	ctk_box_pack_start (CTK_BOX (panel),
			    box,
			    TRUE,
			    TRUE,
			    0);
}

static void
build_vertical_panel (LapizPanel *panel)
{
	CtkWidget *close_button;
	CtkWidget *title_hbox;
	CtkWidget *icon_name_hbox;
	CtkWidget *dummy_label;

	/* Create title hbox */
	title_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_container_set_border_width (CTK_CONTAINER (title_hbox), 5);

	ctk_box_pack_start (CTK_BOX (panel), title_hbox, FALSE, FALSE, 0);

	icon_name_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_box_pack_start (CTK_BOX (title_hbox),
			    icon_name_hbox,
			    TRUE,
			    TRUE,
			    0);

	panel->priv->title_image =
				ctk_image_new_from_icon_name ("text-x-generic",
				                              CTK_ICON_SIZE_MENU);
	ctk_box_pack_start (CTK_BOX (icon_name_hbox),
			    panel->priv->title_image,
			    FALSE,
			    TRUE,
			    0);

	dummy_label = ctk_label_new (" ");

	ctk_box_pack_start (CTK_BOX (icon_name_hbox),
			    dummy_label,
			    FALSE,
			    FALSE,
			    0);

	panel->priv->title_label = ctk_label_new (_("Empty"));
	ctk_label_set_xalign (CTK_LABEL (panel->priv->title_label), 0.0);
	ctk_label_set_ellipsize(CTK_LABEL (panel->priv->title_label), PANGO_ELLIPSIZE_END);

	ctk_box_pack_start (CTK_BOX (icon_name_hbox),
			    panel->priv->title_label,
			    TRUE,
			    TRUE,
			    0);

	close_button = create_close_button (panel);

	ctk_box_pack_start (CTK_BOX (title_hbox),
			    close_button,
			    FALSE,
			    FALSE,
			    0);

	ctk_widget_show_all (title_hbox);

	ctk_box_pack_start (CTK_BOX (panel),
			    panel->priv->notebook,
			    TRUE,
			    TRUE,
			    0);
}

static GObject *
lapiz_panel_constructor (GType type,
			 guint n_construct_properties,
			 GObjectConstructParam *construct_properties)
{

	/* Invoke parent constructor. */
	LapizPanelClass *klass = LAPIZ_PANEL_CLASS (g_type_class_peek (LAPIZ_TYPE_PANEL));
	GObjectClass *parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	GObject *obj = parent_class->constructor (type,
						  n_construct_properties,
						  construct_properties);

	/* Build the panel, now that we know the orientation
			   (_init has been called previously) */
	LapizPanel *panel = LAPIZ_PANEL (obj);

	build_notebook_for_panel (panel);
  	if (panel->priv->orientation == CTK_ORIENTATION_HORIZONTAL)
  		build_horizontal_panel (panel);
	else
		build_vertical_panel (panel);

	g_signal_connect (panel,
			  "show",
			  G_CALLBACK (panel_show),
			  NULL);

	return obj;
}

/**
 * lapiz_panel_new:
 * @orientation: a #CtkOrientation
 *
 * Creates a new #LapizPanel with the given @orientation. You shouldn't create
 * a new panel use lapiz_window_get_side_panel() or lapiz_window_get_bottom_panel()
 * instead.
 *
 * Returns: a new #LapizPanel object.
 */
CtkWidget *
lapiz_panel_new (CtkOrientation orientation)
{
	return CTK_WIDGET (g_object_new (LAPIZ_TYPE_PANEL, "panel-orientation", orientation, NULL));
}

static CtkWidget *
build_tab_label (LapizPanel  *panel,
		 CtkWidget   *item,
		 const gchar *name,
		 CtkWidget   *icon)
{
	CtkWidget *hbox, *label_hbox, *label_ebox;
	CtkWidget *label;

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 4);

	label_ebox = ctk_event_box_new ();
	ctk_event_box_set_visible_window (CTK_EVENT_BOX (label_ebox), FALSE);
	ctk_box_pack_start (CTK_BOX (hbox), label_ebox, TRUE, TRUE, 0);

	label_hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 4);
	ctk_container_add (CTK_CONTAINER (label_ebox), label_hbox);

	/* setup icon */
	ctk_box_pack_start (CTK_BOX (label_hbox), icon, FALSE, FALSE, 0);

	/* setup label */
	label = ctk_label_new (name);
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_widget_set_margin_start (label, 0);
	ctk_widget_set_margin_end (label, 0);
	ctk_widget_set_margin_top (label, 0);
	ctk_widget_set_margin_bottom (label, 0);
	ctk_box_pack_start (CTK_BOX (label_hbox), label, TRUE, TRUE, 0);

	ctk_widget_set_tooltip_text (label_ebox, name);

	ctk_widget_show_all (hbox);

	if (panel->priv->orientation == CTK_ORIENTATION_VERTICAL)
		ctk_widget_hide(label);

	g_object_set_data (G_OBJECT (item), "label", label);
	g_object_set_data (G_OBJECT (item), "hbox", hbox);

	return hbox;
}

/**
 * lapiz_panel_add_item:
 * @panel: a #LapizPanel
 * @item: the #CtkWidget to add to the @panel
 * @name: the name to be shown in the @panel
 * @image: the image to be shown in the @panel
 *
 * Adds a new item to the @panel.
 */
void
lapiz_panel_add_item (LapizPanel  *panel,
		      CtkWidget   *item,
		      const gchar *name,
		      CtkWidget   *image)
{
	LapizPanelItem *data;
	CtkWidget *tab_label;
	CtkWidget *menu_label;
	gint w, h;

	g_return_if_fail (LAPIZ_IS_PANEL (panel));
	g_return_if_fail (CTK_IS_WIDGET (item));
	g_return_if_fail (name != NULL);
	g_return_if_fail (image == NULL || CTK_IS_IMAGE (image));

	data = g_new (LapizPanelItem, 1);

	data->name = g_strdup (name);

	if (image == NULL)
	{
		/* default to empty */
		data->icon = ctk_image_new_from_icon_name ("text-x-generic",
		                                           CTK_ICON_SIZE_MENU);
	}
	else
	{
		data->icon = image;
	}

	ctk_icon_size_lookup (CTK_ICON_SIZE_MENU, &w, &h);
	ctk_widget_set_size_request (data->icon, w, h);

	g_object_set_data (G_OBJECT (item),
		           PANEL_ITEM_KEY,
		           data);

	tab_label = build_tab_label (panel, item, data->name, data->icon);

	menu_label = ctk_label_new (name);
	ctk_label_set_xalign (CTK_LABEL (menu_label), 0.0);

	if (!ctk_widget_get_visible (item))
		ctk_widget_show (item);

	ctk_notebook_append_page_menu (CTK_NOTEBOOK (panel->priv->notebook),
				       item,
				       tab_label,
				       menu_label);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_ADDED], 0, item);
}

/**
 * lapiz_panel_add_item_with_icon:
 * @panel: a #LapizPanel
 * @item: the #CtkWidget to add to the @panel
 * @name: the name to be shown in the @panel
 * @icon_name: a icon name
 *
 * Same as lapiz_panel_add_item() but using an image from icon name.
 */
void
lapiz_panel_add_item_with_icon (LapizPanel  *panel,
				CtkWidget   *item,
				const gchar *name,
				const gchar *icon_name)
{
	CtkWidget *icon = NULL;

	if (icon_name != NULL)
	{
		icon = ctk_image_new_from_icon_name (icon_name,
		                                     CTK_ICON_SIZE_MENU);
	}

	lapiz_panel_add_item (panel, item, name, icon);
}

/**
 * lapiz_panel_remove_item:
 * @panel: a #LapizPanel
 * @item: the item to be removed from the panel
 *
 * Removes the widget @item from the panel if it is in the @panel and returns
 * %TRUE if there was not any problem.
 *
 * Returns: %TRUE if it was well removed.
 */
gboolean
lapiz_panel_remove_item (LapizPanel *panel,
			 CtkWidget  *item)
{
	LapizPanelItem *data;
	gint page_num;

	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (CTK_IS_WIDGET (item), FALSE);

	page_num = ctk_notebook_page_num (CTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	data = (LapizPanelItem *)g_object_get_data (G_OBJECT (item),
					            PANEL_ITEM_KEY);
	g_return_val_if_fail (data != NULL, FALSE);

	g_free (data->name);
	g_free (data);

	g_object_set_data (G_OBJECT (item),
		           PANEL_ITEM_KEY,
		           NULL);

	/* ref the item to keep it alive during signal emission */
	g_object_ref (G_OBJECT (item));

	ctk_notebook_remove_page (CTK_NOTEBOOK (panel->priv->notebook),
				  page_num);

	/* if we removed all the pages, reset the title */
	if (ctk_notebook_get_n_pages (CTK_NOTEBOOK (panel->priv->notebook)) == 0)
		sync_title (panel, NULL);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_REMOVED], 0, item);

	g_object_unref (G_OBJECT (item));

	return TRUE;
}

/**
 * lapiz_panel_activate_item:
 * @panel: a #LapizPanel
 * @item: the item to be activated
 *
 * Switches to the page that contains @item.
 *
 * Returns: %TRUE if it was activated
 */
gboolean
lapiz_panel_activate_item (LapizPanel *panel,
			   CtkWidget  *item)
{
	gint page_num;

	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (CTK_IS_WIDGET (item), FALSE);

	page_num = ctk_notebook_page_num (CTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	ctk_notebook_set_current_page (CTK_NOTEBOOK (panel->priv->notebook),
				       page_num);

	return TRUE;
}

/**
 * lapiz_panel_item_is_active:
 * @panel: a #LapizPanel
 * @item: a #CtkWidget
 *
 * Returns whether @item is the active widget in @panel
 *
 * Returns: %TRUE if @item is the active widget
 */
gboolean
lapiz_panel_item_is_active (LapizPanel *panel,
			    CtkWidget  *item)
{
	gint cur_page;
	gint page_num;

	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (CTK_IS_WIDGET (item), FALSE);

	page_num = ctk_notebook_page_num (CTK_NOTEBOOK (panel->priv->notebook),
					  item);

	if (page_num == -1)
		return FALSE;

	cur_page = ctk_notebook_get_current_page (
				CTK_NOTEBOOK (panel->priv->notebook));

	return (page_num == cur_page);
}

/**
 * lapiz_panel_get_orientation:
 * @panel: a #LapizPanel
 *
 * Gets the orientation of the @panel.
 *
 * Returns: the #CtkOrientation of #LapizPanel
 */
CtkOrientation
lapiz_panel_get_orientation (LapizPanel *panel)
{
	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), CTK_ORIENTATION_VERTICAL);

	return panel->priv->orientation;
}

/**
 * lapiz_panel_get_n_items:
 * @panel: a #LapizPanel
 *
 * Gets the number of items in a @panel.
 *
 * Returns: the number of items contained in #LapizPanel
 */
gint
lapiz_panel_get_n_items (LapizPanel *panel)
{
	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), -1);

	return ctk_notebook_get_n_pages (CTK_NOTEBOOK (panel->priv->notebook));
}

gint
_lapiz_panel_get_active_item_id (LapizPanel *panel)
{
	gint cur_page;
	CtkWidget *item;
	LapizPanelItem *data;

	g_return_val_if_fail (LAPIZ_IS_PANEL (panel), 0);

	cur_page = ctk_notebook_get_current_page (
				CTK_NOTEBOOK (panel->priv->notebook));
	if (cur_page == -1)
		return 0;

	item = ctk_notebook_get_nth_page (
				CTK_NOTEBOOK (panel->priv->notebook),
				cur_page);

	/* FIXME: for now we use as the hash of the name as id.
	 * However the name is not guaranteed to be unique and
	 * it is a translated string, so it's subotimal, but should
	 * be good enough for now since we don't want to add an
	 * ad hoc id argument.
	 */

	data = (LapizPanelItem *)g_object_get_data (G_OBJECT (item),
					            PANEL_ITEM_KEY);
	g_return_val_if_fail (data != NULL, 0);

	return g_str_hash (data->name);
}

void
_lapiz_panel_set_active_item_by_id (LapizPanel *panel,
				    gint        id)
{
	gint n, i;

	g_return_if_fail (LAPIZ_IS_PANEL (panel));

	if (id == 0)
		return;

	n = ctk_notebook_get_n_pages (
				CTK_NOTEBOOK (panel->priv->notebook));

	for (i = 0; i < n; i++)
	{
		CtkWidget *item;
		LapizPanelItem *data;

		item = ctk_notebook_get_nth_page (
				CTK_NOTEBOOK (panel->priv->notebook), i);

		data = (LapizPanelItem *)g_object_get_data (G_OBJECT (item),
						            PANEL_ITEM_KEY);
		g_return_if_fail (data != NULL);

		if (g_str_hash (data->name) == id)
		{
			ctk_notebook_set_current_page (
				CTK_NOTEBOOK (panel->priv->notebook), i);

			return;
		}
	}
}
