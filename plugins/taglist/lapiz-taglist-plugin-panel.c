/*
 * lapiz-taglist-plugin-panel.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "lapiz-taglist-plugin-panel.h"
#include "lapiz-taglist-plugin-parser.h"

#include <lapiz/lapiz-utils.h>
#include <lapiz/lapiz-debug.h>

#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>
#include <glib/gi18n.h>

enum
{
	COLUMN_TAG_NAME,
	COLUMN_TAG_INDEX_IN_GROUP,
	NUM_COLUMNS
};

struct _LapizTaglistPluginPanelPrivate
{
	LapizWindow  *window;

	CtkWidget *tag_groups_combo;
	CtkWidget *tags_list;
	CtkWidget *preview;

	TagGroup *selected_tag_group;

	gchar *data_dir;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizTaglistPluginPanel,
                                lapiz_taglist_plugin_panel,
                                CTK_TYPE_BOX,
                                0,
                                G_ADD_PRIVATE_DYNAMIC(LapizTaglistPluginPanel))

enum
{
	PROP_0,
	PROP_WINDOW,
};

static void
set_window (LapizTaglistPluginPanel *panel,
	    LapizWindow             *window)
{
	g_return_if_fail (panel->priv->window == NULL);
	g_return_if_fail (LAPIZ_IS_WINDOW (window));

	panel->priv->window = window;

	/* TODO */
}

static void
lapiz_taglist_plugin_panel_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	LapizTaglistPluginPanel *panel = LAPIZ_TAGLIST_PLUGIN_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			set_window (panel, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_taglist_plugin_panel_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
	LapizTaglistPluginPanel *panel = LAPIZ_TAGLIST_PLUGIN_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			panel->priv = lapiz_taglist_plugin_panel_get_instance_private (panel);
			g_value_set_object (value, panel->priv->window);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_taglist_plugin_panel_finalize (GObject *object)
{
	LapizTaglistPluginPanel *panel = LAPIZ_TAGLIST_PLUGIN_PANEL (object);

	g_free (panel->priv->data_dir);

	G_OBJECT_CLASS (lapiz_taglist_plugin_panel_parent_class)->finalize (object);
}

static void
lapiz_taglist_plugin_panel_class_init (LapizTaglistPluginPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_taglist_plugin_panel_finalize;
	object_class->get_property = lapiz_taglist_plugin_panel_get_property;
	object_class->set_property = lapiz_taglist_plugin_panel_set_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							 "Window",
							 "The LapizWindow this LapizTaglistPluginPanel is associated with",
							 LAPIZ_TYPE_WINDOW,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT_ONLY));
}

static void
lapiz_taglist_plugin_panel_class_finalize (LapizTaglistPluginPanelClass *klass G_GNUC_UNUSED)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
insert_tag (LapizTaglistPluginPanel *panel,
	    Tag                     *tag,
	    gboolean                 grab_focus)
{
	LapizView *view;
	CtkTextBuffer *buffer;
	CtkTextIter start, end;
	CtkTextIter cursor;
	gboolean sel = FALSE;

	lapiz_debug (DEBUG_PLUGINS);

	view = lapiz_window_get_active_view (panel->priv->window);
	g_return_if_fail (view != NULL);

	buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (view));

	ctk_text_buffer_begin_user_action (buffer);

	/* always insert the begin tag at the beginning of the selection
	 * and the end tag at the end, if there is no selection they will
	 * be automatically inserted at the cursor position.
	 */

	if (tag->begin != NULL)
	{
		sel = ctk_text_buffer_get_selection_bounds (buffer,
						 	    &start,
						 	    &end);

		ctk_text_buffer_insert (buffer,
					&start,
					(gchar *)tag->begin,
					-1);

		/* get iterators again since they have been invalidated and move
		 * the cursor after the selection */
		ctk_text_buffer_get_selection_bounds (buffer,
						      &start,
						      &cursor);
	}

	if (tag->end != NULL)
	{
		sel = ctk_text_buffer_get_selection_bounds (buffer,
							    &start,
							    &end);

		ctk_text_buffer_insert (buffer,
					&end,
					(gchar *)tag->end,
					-1);

		/* if there is no selection and we have a paired tag, move the
		 * cursor between the pair, otherwise move it at the end */
		if (!sel)
		{
			gint offset;

			offset = ctk_text_iter_get_offset (&end) -
				 g_utf8_strlen ((gchar *)tag->end, -1);

			ctk_text_buffer_get_iter_at_offset (buffer,
							    &end,
							    offset);
		}

		cursor = end;
	}

	ctk_text_buffer_place_cursor (buffer, &cursor);

	ctk_text_buffer_end_user_action (buffer);

	if (grab_focus)
		ctk_widget_grab_focus (CTK_WIDGET (view));
}

static void
tag_list_row_activated_cb (CtkTreeView             *tag_list,
			   CtkTreePath             *path,
			   CtkTreeViewColumn       *column G_GNUC_UNUSED,
			   LapizTaglistPluginPanel *panel)
{
	CtkTreeIter iter;
	CtkTreeModel *model;
	gint index;

	lapiz_debug (DEBUG_PLUGINS);

	model = ctk_tree_view_get_model (tag_list);

	ctk_tree_model_get_iter (model, &iter, path);
	g_return_if_fail (&iter != NULL);

	ctk_tree_model_get (model, &iter, COLUMN_TAG_INDEX_IN_GROUP, &index, -1);

	lapiz_debug_message (DEBUG_PLUGINS, "Index: %d", index);

	insert_tag (panel,
		    (Tag*)g_list_nth_data (panel->priv->selected_tag_group->tags, index),
		    TRUE);
}

static gboolean
tag_list_key_press_event_cb (CtkTreeView             *tag_list,
			     CdkEventKey             *event,
			     LapizTaglistPluginPanel *panel)
{
	gboolean grab_focus;

	grab_focus = (event->state & CDK_CONTROL_MASK) != 0;

	if (event->keyval == CDK_KEY_Return)
	{
		CtkTreeModel *model;
		CtkTreeSelection *selection;
		CtkTreeIter iter;
		gint index;

		lapiz_debug_message (DEBUG_PLUGINS, "RETURN Pressed");

		model = ctk_tree_view_get_model (tag_list);

		selection = ctk_tree_view_get_selection (tag_list);

		if (ctk_tree_selection_get_selected (selection, NULL, &iter))
		{
			ctk_tree_model_get (model, &iter, COLUMN_TAG_INDEX_IN_GROUP, &index, -1);

			lapiz_debug_message (DEBUG_PLUGINS, "Index: %d", index);

			insert_tag (panel,
				    (Tag*)g_list_nth_data (panel->priv->selected_tag_group->tags, index),
				    grab_focus);
		}

		return TRUE;
	}

	return FALSE;
}

static CtkTreeModel*
create_model (LapizTaglistPluginPanel *panel)
{
	gint i = 0;
	CtkListStore *store;
	CtkTreeIter iter;
	GList *list;

	lapiz_debug (DEBUG_PLUGINS);

	/* create list store */
	store = ctk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

	/* add data to the list store */
	list = panel->priv->selected_tag_group->tags;

	while (list != NULL)
	{
		const gchar* tag_name;

		tag_name = (gchar *)((Tag*)list->data)->name;

		lapiz_debug_message (DEBUG_PLUGINS, "%d : %s", i, tag_name);

		ctk_list_store_append (store, &iter);
		ctk_list_store_set (store, &iter,
				    COLUMN_TAG_NAME, tag_name,
				    COLUMN_TAG_INDEX_IN_GROUP, i,
				    -1);
		++i;

		list = g_list_next (list);
	}

	lapiz_debug_message (DEBUG_PLUGINS, "Rows: %d ",
			     ctk_tree_model_iter_n_children (CTK_TREE_MODEL (store), NULL));

	return CTK_TREE_MODEL (store);
}

static void
populate_tags_list (LapizTaglistPluginPanel *panel)
{
	CtkTreeModel* model;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_if_fail (taglist != NULL);

	model = create_model (panel);
	ctk_tree_view_set_model (CTK_TREE_VIEW (panel->priv->tags_list),
			         model);
	g_object_unref (model);
}

static TagGroup *
find_tag_group (const gchar *name)
{
	GList *l;

	lapiz_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (taglist != NULL, NULL);

	for (l = taglist->tag_groups; l != NULL; l = g_list_next (l))
	{
		if (strcmp (name, (gchar *)((TagGroup*)l->data)->name) == 0)
			return (TagGroup*)l->data;
	}

	return NULL;
}

static void
populate_tag_groups_combo (LapizTaglistPluginPanel *panel)
{
	GList *l;
	CtkComboBox *combo;
	CtkComboBoxText *combotext;

	lapiz_debug (DEBUG_PLUGINS);

	combo = CTK_COMBO_BOX (panel->priv->tag_groups_combo);
	combotext = CTK_COMBO_BOX_TEXT (panel->priv->tag_groups_combo);

	if (taglist == NULL)
		return;

	for (l = taglist->tag_groups; l != NULL; l = g_list_next (l))
	{
		ctk_combo_box_text_append_text (combotext,
					   (gchar *)((TagGroup*)l->data)->name);
	}

	ctk_combo_box_set_active (combo, 0);

	return;
}

static void
selected_group_changed (CtkComboBox             *combo,
			LapizTaglistPluginPanel *panel)
{
	gchar* group_name;

	lapiz_debug (DEBUG_PLUGINS);

	group_name = ctk_combo_box_text_get_active_text (CTK_COMBO_BOX_TEXT (combo));

	if ((group_name == NULL) || (strlen (group_name) <= 0))
	{
		g_free (group_name);
		return;
	}

	if ((panel->priv->selected_tag_group == NULL) ||
	    (strcmp (group_name, (gchar *)panel->priv->selected_tag_group->name) != 0))
	{
		panel->priv->selected_tag_group = find_tag_group (group_name);
		g_return_if_fail (panel->priv->selected_tag_group != NULL);

		lapiz_debug_message (DEBUG_PLUGINS,
				     "New selected group: %s",
				     panel->priv->selected_tag_group->name);

		populate_tags_list (panel);
	}

	/* Clean up preview */
	ctk_label_set_text (CTK_LABEL (panel->priv->preview),
			    "");

	g_free (group_name);
}

static gchar *
create_preview_string (Tag *tag)
{
	GString *str;

	str = g_string_new ("<tt><small>");

	if (tag->begin != NULL)
	{
		gchar *markup;

		markup = g_markup_escape_text ((gchar *)tag->begin, -1);
		g_string_append (str, markup);
		g_free (markup);
	}

	if (tag->end != NULL)
	{
		gchar *markup;

		markup = g_markup_escape_text ((gchar *)tag->end, -1);
		g_string_append (str, markup);
		g_free (markup);
	}

	g_string_append (str, "</small></tt>");

	return g_string_free (str, FALSE);
}

static void
update_preview (LapizTaglistPluginPanel *panel,
		Tag                     *tag)
{
	gchar *str;

	str = create_preview_string (tag);

	ctk_label_set_markup (CTK_LABEL (panel->priv->preview),
			      str);

	g_free (str);
}

static void
tag_list_cursor_changed_cb (CtkTreeView *tag_list,
			    gpointer     data)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	CtkTreeIter iter;
	gint index;

	LapizTaglistPluginPanel *panel = (LapizTaglistPluginPanel *)data;

	model = ctk_tree_view_get_model (tag_list);

	selection = ctk_tree_view_get_selection (tag_list);

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
	{
		ctk_tree_model_get (model, &iter, COLUMN_TAG_INDEX_IN_GROUP, &index, -1);

		lapiz_debug_message (DEBUG_PLUGINS, "Index: %d", index);

		update_preview (panel,
			        (Tag*)g_list_nth_data (panel->priv->selected_tag_group->tags, index));
	}
}

static gboolean
tags_list_query_tooltip_cb (CtkWidget               *widget,
			    gint                     x,
			    gint                     y,
			    gboolean                 keyboard_tip,
			    CtkTooltip              *tooltip,
			    LapizTaglistPluginPanel *panel)
{
	CtkTreeIter iter;
	CtkTreeModel *model;
	CtkTreePath *path = NULL;
	gint index;
	Tag *tag;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (widget));

	if (keyboard_tip)
	{
		ctk_tree_view_get_cursor (CTK_TREE_VIEW (widget),
					  &path,
					  NULL);

		if (path == NULL)
		{
			return FALSE;
		}
	}
	else
	{
		gint bin_x, bin_y;

		ctk_tree_view_convert_widget_to_bin_window_coords (CTK_TREE_VIEW (widget),
								   x, y,
								   &bin_x, &bin_y);

		if (!ctk_tree_view_get_path_at_pos (CTK_TREE_VIEW (widget),
						    bin_x, bin_y,
						    &path,
						    NULL, NULL, NULL))
		{
			return FALSE;
		}
	}

	ctk_tree_model_get_iter (model, &iter, path);
	ctk_tree_model_get (model, &iter,
			    COLUMN_TAG_INDEX_IN_GROUP, &index,
			    -1);

	tag = g_list_nth_data (panel->priv->selected_tag_group->tags, index);
	if (tag != NULL)
	{
		gchar *tip;

		tip = create_preview_string (tag);
		ctk_tooltip_set_markup (tooltip, tip);
		g_free (tip);
		ctk_tree_path_free (path);

		return TRUE;
	}

	ctk_tree_path_free (path);

	return FALSE;
}

static gboolean
draw_event_cb (CtkWidget *panel,
               cairo_t   *cr G_GNUC_UNUSED,
               gpointer   user_data G_GNUC_UNUSED)
{
	LapizTaglistPluginPanel *ppanel = LAPIZ_TAGLIST_PLUGIN_PANEL (panel);

	lapiz_debug (DEBUG_PLUGINS);

	/* If needed load taglists from files at the first expose */
	if (taglist == NULL)
		create_taglist (ppanel->priv->data_dir);

	/* And populate combo box */
	populate_tag_groups_combo (LAPIZ_TAGLIST_PLUGIN_PANEL (panel));

	/* We need to manage only the first draw -> disconnect */
	g_signal_handlers_disconnect_by_func (panel, draw_event_cb, NULL);

	return FALSE;
}

static void
set_combo_tooltip (CtkWidget *widget,
		   gpointer   data G_GNUC_UNUSED)
{
	if (CTK_IS_BUTTON (widget))
	{
		ctk_widget_set_tooltip_text (widget,
					     _("Select the group of tags you want to use"));
	}
}

static void
realize_tag_groups_combo (CtkWidget *combo,
			  gpointer   data G_GNUC_UNUSED)
{
	ctk_container_forall (CTK_CONTAINER (combo),
			      set_combo_tooltip,
			      NULL);
}

static void
add_preview_widget (LapizTaglistPluginPanel *panel)
{
	CtkWidget *expander;
	CtkWidget *frame;

	expander = ctk_expander_new_with_mnemonic (_("_Preview"));

	panel->priv->preview = 	ctk_label_new (NULL);
	ctk_widget_set_size_request (panel->priv->preview, -1, 80);

	ctk_label_set_line_wrap	(CTK_LABEL (panel->priv->preview), TRUE);
	ctk_label_set_use_markup (CTK_LABEL (panel->priv->preview), TRUE);
	ctk_widget_set_halign (panel->priv->preview, CTK_ALIGN_START);
	ctk_widget_set_valign (panel->priv->preview, CTK_ALIGN_START);
	ctk_widget_set_margin_start (panel->priv->preview, 6);
	ctk_widget_set_margin_end (panel->priv->preview, 6);
	ctk_widget_set_margin_top (panel->priv->preview, 6);
	ctk_widget_set_margin_bottom (panel->priv->preview, 6);
	ctk_label_set_selectable (CTK_LABEL (panel->priv->preview), TRUE);
	ctk_label_set_selectable (CTK_LABEL (panel->priv->preview), TRUE);
	ctk_label_set_ellipsize  (CTK_LABEL (panel->priv->preview),
				  PANGO_ELLIPSIZE_END);

	frame = ctk_frame_new (0);
	ctk_frame_set_shadow_type (CTK_FRAME (frame), CTK_SHADOW_IN);

	ctk_container_add (CTK_CONTAINER (frame),
			   panel->priv->preview);

	ctk_container_add (CTK_CONTAINER (expander),
			   frame);

	ctk_box_pack_start (CTK_BOX (panel), expander, FALSE, FALSE, 0);

	ctk_widget_show_all (expander);
}

static void
lapiz_taglist_plugin_panel_init (LapizTaglistPluginPanel *panel)
{
	CtkWidget *sw;
	CtkTreeViewColumn *column;
	CtkCellRenderer *cell;
	GList *focus_chain = NULL;

	lapiz_debug (DEBUG_PLUGINS);

	panel->priv = lapiz_taglist_plugin_panel_get_instance_private (panel);
	panel->priv->data_dir = NULL;

	ctk_orientable_set_orientation (CTK_ORIENTABLE (panel),
									CTK_ORIENTATION_VERTICAL);

	/* Build the window content */
	panel->priv->tag_groups_combo = ctk_combo_box_text_new ();
	ctk_box_pack_start (CTK_BOX (panel),
			    panel->priv->tag_groups_combo,
			    FALSE,
			    TRUE,
			    0);

	g_signal_connect (panel->priv->tag_groups_combo,
			  "realize",
			  G_CALLBACK (realize_tag_groups_combo),
			  panel);

	sw = ctk_scrolled_window_new (NULL, NULL);

	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
					CTK_POLICY_AUTOMATIC,
					CTK_POLICY_AUTOMATIC);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (sw),
                                             CTK_SHADOW_IN);
	ctk_box_pack_start (CTK_BOX (panel), sw, TRUE, TRUE, 0);

	/* Create tree view */
	panel->priv->tags_list = ctk_tree_view_new ();

	lapiz_utils_set_atk_name_description (panel->priv->tag_groups_combo,
					      _("Available Tag Lists"),
					      NULL);
	lapiz_utils_set_atk_name_description (panel->priv->tags_list,
					      _("Tags"),
					      NULL);
	lapiz_utils_set_atk_relation (panel->priv->tag_groups_combo,
				      panel->priv->tags_list,
				      ATK_RELATION_CONTROLLER_FOR);
	lapiz_utils_set_atk_relation (panel->priv->tags_list,
				      panel->priv->tag_groups_combo,
				      ATK_RELATION_CONTROLLED_BY);

	ctk_tree_view_set_headers_visible (CTK_TREE_VIEW (panel->priv->tags_list), FALSE);

	g_object_set (panel->priv->tags_list, "has-tooltip", TRUE, NULL);

	/* Add the tags column */
	cell = ctk_cell_renderer_text_new ();
	column = ctk_tree_view_column_new_with_attributes (_("Tags"),
							   cell,
							   "text",
							   COLUMN_TAG_NAME,
							   NULL);
	ctk_tree_view_append_column (CTK_TREE_VIEW (panel->priv->tags_list),
				     column);

	ctk_tree_view_set_search_column (CTK_TREE_VIEW (panel->priv->tags_list),
					 COLUMN_TAG_NAME);

	ctk_container_add (CTK_CONTAINER (sw), panel->priv->tags_list);

	focus_chain = g_list_prepend (focus_chain, panel->priv->tags_list);
	focus_chain = g_list_prepend (focus_chain, panel->priv->tag_groups_combo);

	ctk_container_set_focus_chain (CTK_CONTAINER (panel),
				       focus_chain);
	g_list_free (focus_chain);

	add_preview_widget (panel);

	ctk_widget_show_all (CTK_WIDGET (sw));
	ctk_widget_show (CTK_WIDGET (panel->priv->tag_groups_combo));

	g_signal_connect_after (panel->priv->tags_list,
				"row_activated",
				G_CALLBACK (tag_list_row_activated_cb),
				panel);
	g_signal_connect (panel->priv->tags_list,
			  "key_press_event",
			  G_CALLBACK (tag_list_key_press_event_cb),
			  panel);
	g_signal_connect (panel->priv->tags_list,
			  "query-tooltip",
			  G_CALLBACK (tags_list_query_tooltip_cb),
			  panel);
	g_signal_connect (panel->priv->tags_list,
			  "cursor_changed",
			  G_CALLBACK (tag_list_cursor_changed_cb),
			  panel);
	g_signal_connect (panel->priv->tag_groups_combo,
			  "changed",
			  G_CALLBACK (selected_group_changed),
			  panel);
	g_signal_connect (panel,
			  "draw",
			  G_CALLBACK (draw_event_cb),
			  NULL);
}

CtkWidget *
lapiz_taglist_plugin_panel_new (LapizWindow *window,
				const gchar *data_dir)
{
	LapizTaglistPluginPanel *panel;

	g_return_val_if_fail (LAPIZ_IS_WINDOW (window), NULL);

	panel = g_object_new (LAPIZ_TYPE_TAGLIST_PLUGIN_PANEL,
			      "window", window,
			      NULL);

	panel->priv->data_dir = g_strdup (data_dir);

	return CTK_WIDGET (panel);
}

void
_lapiz_taglist_plugin_panel_register_type (GTypeModule *type_module)
{
	lapiz_taglist_plugin_panel_register_type (type_module);
}
