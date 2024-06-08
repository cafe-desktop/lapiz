/*
 * lapiz-file-browser-widget.c - Lapiz plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <ctk/ctk.h>
#include <cdk/cdkkeysyms.h>

#include <lapiz/lapiz-utils.h>

#include "lapiz-file-browser-utils.h"
#include "lapiz-file-browser-error.h"
#include "lapiz-file-browser-widget.h"
#include "lapiz-file-browser-view.h"
#include "lapiz-file-browser-store.h"
#include "lapiz-file-bookmarks-store.h"
#include "lapiz-file-browser-marshal.h"
#include "lapiz-file-browser-enum-types.h"

#define XML_UI_FILE "lapiz-file-browser-widget-ui.xml"
#define LOCATION_DATA_KEY "lapiz-file-browser-widget-location"

enum
{
	BOOKMARKS_ID,
	SEPARATOR_CUSTOM_ID,
	SEPARATOR_ID,
	PATH_ID,
	NUM_DEFAULT_IDS
};

enum
{
	COLUMN_INDENT,
	COLUMN_ICON,
	COLUMN_NAME,
	COLUMN_FILE,
	COLUMN_ID,
	N_COLUMNS
};

/* Properties */
enum
{
	PROP_0,

	PROP_FILTER_PATTERN,
	PROP_ENABLE_DELETE
};

/* Signals */
enum
{
	URI_ACTIVATED,
	ERROR,
	CONFIRM_DELETE,
	CONFIRM_NO_TRASH,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0 };

typedef struct _SignalNode
{
	GObject *object;
	gulong id;
} SignalNode;

typedef struct
{
	gulong id;
	LapizFileBrowserWidgetFilterFunc func;
	gpointer user_data;
	GDestroyNotify destroy_notify;
} FilterFunc;

typedef struct
{
	GFile *root;
	GFile *virtual_root;
} Location;

typedef struct
{
	gchar *name;
	GdkPixbuf *icon;
} NameIcon;

struct _LapizFileBrowserWidgetPrivate
{
	LapizFileBrowserView *treeview;
	LapizFileBrowserStore *file_store;
	LapizFileBookmarksStore *bookmarks_store;

	GHashTable *bookmarks_hash;

	CtkWidget *combo;
	CtkTreeStore *combo_model;

	CtkWidget *filter_expander;
	CtkWidget *filter_entry;

	CtkUIManager *manager;
	CtkActionGroup *action_group;
	CtkActionGroup *action_group_selection;
	CtkActionGroup *action_group_file_selection;
	CtkActionGroup *action_group_single_selection;
	CtkActionGroup *action_group_single_most_selection;
	CtkActionGroup *action_group_sensitive;
	CtkActionGroup *bookmark_action_group;

	GSList *signal_pool;

	GSList *filter_funcs;
	gulong filter_id;
	gulong glob_filter_id;
	GPatternSpec *filter_pattern;
	gchar *filter_pattern_str;

	GList *locations;
	GList *current_location;
	gboolean changing_location;
	CtkWidget *location_previous_menu;
	CtkWidget *location_next_menu;
	CtkWidget *current_location_menu_item;

	gboolean enable_delete;

	GCancellable *cancellable;

	CdkCursor *busy_cursor;
};

static void set_enable_delete		       (LapizFileBrowserWidget *obj,
						gboolean enable);
static void on_model_set                       (GObject * gobject,
						GParamSpec * arg1,
						LapizFileBrowserWidget * obj);
static void on_treeview_error                  (LapizFileBrowserView * tree_view,
						guint code,
						gchar * message,
						LapizFileBrowserWidget * obj);
static void on_file_store_error                (LapizFileBrowserStore * store,
						guint code,
						gchar * message,
						LapizFileBrowserWidget * obj);
static gboolean on_file_store_no_trash 	       (LapizFileBrowserStore * store,
						GList * files,
						LapizFileBrowserWidget * obj);
static void on_combo_changed                   (CtkComboBox * combo,
						LapizFileBrowserWidget * obj);
static gboolean on_treeview_popup_menu         (LapizFileBrowserView * treeview,
						LapizFileBrowserWidget * obj);
static gboolean on_treeview_button_press_event (LapizFileBrowserView * treeview,
						CdkEventButton * event,
						LapizFileBrowserWidget * obj);
static gboolean on_treeview_key_press_event    (LapizFileBrowserView * treeview,
						CdkEventKey * event,
						LapizFileBrowserWidget * obj);
static void on_selection_changed               (CtkTreeSelection * selection,
						LapizFileBrowserWidget * obj);

static void on_virtual_root_changed            (LapizFileBrowserStore * model,
						GParamSpec *param,
						LapizFileBrowserWidget * obj);

static gboolean on_entry_filter_activate       (LapizFileBrowserWidget * obj);
static void on_location_jump_activate          (CtkMenuItem * item,
						LapizFileBrowserWidget * obj);
static void on_bookmarks_row_changed           (CtkTreeModel * model,
                                                CtkTreePath * path,
                                                CtkTreeIter * iter,
                                                LapizFileBrowserWidget * obj);
static void on_bookmarks_row_deleted           (CtkTreeModel * model,
                                                CtkTreePath * path,
                                                LapizFileBrowserWidget * obj);
static void on_filter_mode_changed	       (LapizFileBrowserStore * model,
                                                GParamSpec * param,
                                                LapizFileBrowserWidget * obj);
static void on_action_directory_previous       (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_directory_next           (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_directory_up             (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_directory_new            (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_file_open                (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_file_new                 (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_file_rename              (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_file_delete              (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_file_move_to_trash       (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_directory_refresh        (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_directory_open           (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_filter_hidden            (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_filter_binary            (CtkAction * action,
						LapizFileBrowserWidget * obj);
static void on_action_bookmark_open            (CtkAction * action,
						LapizFileBrowserWidget * obj);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizFileBrowserWidget,
                                lapiz_file_browser_widget,
                                CTK_TYPE_BOX,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizFileBrowserWidget))

static void
free_name_icon (gpointer data)
{
	NameIcon * item;

	if (data == NULL)
		return;

	item = (NameIcon *)(data);

	g_free (item->name);

	if (item->icon)
		g_object_unref (item->icon);

	g_free (item);
}

static FilterFunc *
filter_func_new (LapizFileBrowserWidget * obj,
		 LapizFileBrowserWidgetFilterFunc func,
		 gpointer user_data,
		 GDestroyNotify notify)
{
	FilterFunc *result;

	result = g_new (FilterFunc, 1);

	result->id = ++obj->priv->filter_id;
	result->func = func;
	result->user_data = user_data;
	result->destroy_notify = notify;
	return result;
}

static void
location_free (Location * loc)
{
	if (loc->root)
		g_object_unref (loc->root);

	if (loc->virtual_root)
		g_object_unref (loc->virtual_root);

	g_free (loc);
}

static gboolean
combo_find_by_id (LapizFileBrowserWidget * obj, guint id,
		  CtkTreeIter * iter)
{
	guint checkid;
	CtkTreeModel *model = CTK_TREE_MODEL (obj->priv->combo_model);

	if (iter == NULL)
		return FALSE;

	if (ctk_tree_model_get_iter_first (model, iter)) {
		do {
			ctk_tree_model_get (model, iter, COLUMN_ID,
					    &checkid, -1);

			if (checkid == id)
				return TRUE;
		} while (ctk_tree_model_iter_next (model, iter));
	}

	return FALSE;
}

static void
remove_path_items (LapizFileBrowserWidget * obj)
{
	CtkTreeIter iter;

	while (combo_find_by_id (obj, PATH_ID, &iter))
		ctk_tree_store_remove (obj->priv->combo_model, &iter);
}

static void
cancel_async_operation (LapizFileBrowserWidget *widget)
{
	if (!widget->priv->cancellable)
		return;

	g_cancellable_cancel (widget->priv->cancellable);
	g_object_unref (widget->priv->cancellable);

	widget->priv->cancellable = NULL;
}

static void
lapiz_file_browser_widget_finalize (GObject * object)
{
	LapizFileBrowserWidget *obj = LAPIZ_FILE_BROWSER_WIDGET (object);
	GList *loc;

	remove_path_items (obj);
	lapiz_file_browser_store_set_filter_func (obj->priv->file_store,
						  NULL, NULL);

	g_object_unref (obj->priv->manager);
	g_object_unref (obj->priv->file_store);
	g_object_unref (obj->priv->bookmarks_store);
	g_object_unref (obj->priv->combo_model);

	g_slist_foreach (obj->priv->filter_funcs, (GFunc) g_free, NULL);
	g_slist_free (obj->priv->filter_funcs);

	for (loc = obj->priv->locations; loc; loc = loc->next)
		location_free ((Location *) (loc->data));

	if (obj->priv->current_location_menu_item)
		g_object_unref (obj->priv->current_location_menu_item);

	g_list_free (obj->priv->locations);

	g_hash_table_destroy (obj->priv->bookmarks_hash);

	cancel_async_operation (obj);

	g_object_unref (obj->priv->busy_cursor);

	G_OBJECT_CLASS (lapiz_file_browser_widget_parent_class)->finalize (object);
}

static void
lapiz_file_browser_widget_get_property (GObject    *object,
			               guint       prop_id,
			               GValue     *value,
			               GParamSpec *pspec)
{
	LapizFileBrowserWidget *obj = LAPIZ_FILE_BROWSER_WIDGET (object);

	switch (prop_id)
	{
		case PROP_FILTER_PATTERN:
			g_value_set_string (value, obj->priv->filter_pattern_str);
			break;
		case PROP_ENABLE_DELETE:
			g_value_set_boolean (value, obj->priv->enable_delete);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_file_browser_widget_set_property (GObject      *object,
			               guint         prop_id,
			               const GValue *value,
			               GParamSpec   *pspec)
{
	LapizFileBrowserWidget *obj = LAPIZ_FILE_BROWSER_WIDGET (object);

	switch (prop_id)
	{
		case PROP_FILTER_PATTERN:
			lapiz_file_browser_widget_set_filter_pattern (obj,
			                                              g_value_get_string (value));
			break;
		case PROP_ENABLE_DELETE:
			set_enable_delete (obj, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_file_browser_widget_class_init (LapizFileBrowserWidgetClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = lapiz_file_browser_widget_finalize;

	object_class->get_property = lapiz_file_browser_widget_get_property;
	object_class->set_property = lapiz_file_browser_widget_set_property;

	g_object_class_install_property (object_class, PROP_FILTER_PATTERN,
					 g_param_spec_string ("filter-pattern",
					 		      "Filter Pattern",
					 		      "The filter pattern",
					 		      NULL,
					 		      G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ENABLE_DELETE,
					 g_param_spec_boolean ("enable-delete",
					 		       "Enable delete",
					 		       "Enable permanently deleting items",
					 		       TRUE,
					 		       G_PARAM_READWRITE |
					 		       G_PARAM_CONSTRUCT));

	signals[URI_ACTIVATED] =
	    g_signal_new ("uri-activated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserWidgetClass,
					   uri_activated), NULL, NULL,
			  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
			  G_TYPE_STRING);
	signals[ERROR] =
	    g_signal_new ("error", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (LapizFileBrowserWidgetClass,
					   error), NULL, NULL,
			  lapiz_file_browser_marshal_VOID__UINT_STRING,
			  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);

	signals[CONFIRM_DELETE] =
	    g_signal_new ("confirm-delete", G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (LapizFileBrowserWidgetClass,
	                                   confirm_delete),
	                  g_signal_accumulator_true_handled,
	                  NULL,
	                  lapiz_file_browser_marshal_BOOLEAN__OBJECT_POINTER,
	                  G_TYPE_BOOLEAN,
	                  2,
	                  G_TYPE_OBJECT,
	                  G_TYPE_POINTER);

	signals[CONFIRM_NO_TRASH] =
	    g_signal_new ("confirm-no-trash", G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (LapizFileBrowserWidgetClass,
	                                   confirm_no_trash),
	                  g_signal_accumulator_true_handled,
	                  NULL,
	                  lapiz_file_browser_marshal_BOOLEAN__POINTER,
	                  G_TYPE_BOOLEAN,
	                  1,
	                  G_TYPE_POINTER);
}

static void
lapiz_file_browser_widget_class_finalize (LapizFileBrowserWidgetClass *klass G_GNUC_UNUSED)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
add_signal (LapizFileBrowserWidget * obj, gpointer object, gulong id)
{
	SignalNode *node = g_new (SignalNode, 1);

	node->object = G_OBJECT (object);
	node->id = id;

	obj->priv->signal_pool =
	    g_slist_prepend (obj->priv->signal_pool, node);
}

static void
clear_signals (LapizFileBrowserWidget * obj)
{
	GSList *item;
	SignalNode *node;

	for (item = obj->priv->signal_pool; item; item = item->next) {
		node = (SignalNode *) (item->data);

		g_signal_handler_disconnect (node->object, node->id);
		g_free (item->data);
	}

	g_slist_free (obj->priv->signal_pool);
	obj->priv->signal_pool = NULL;
}

static gboolean
separator_func (CtkTreeModel *model,
		CtkTreeIter  *iter,
		gpointer      data G_GNUC_UNUSED)
{
	guint id;

	ctk_tree_model_get (model, iter, COLUMN_ID, &id, -1);

	return (id == SEPARATOR_ID);
}

static gboolean
get_from_bookmark_file (LapizFileBrowserWidget * obj, GFile * file,
		       gchar ** name, GdkPixbuf ** icon)
{
	gpointer data;
	NameIcon * item;

	data = g_hash_table_lookup (obj->priv->bookmarks_hash, file);

	if (data == NULL)
		return FALSE;

	item = (NameIcon *)data;

	*name = g_strdup (item->name);
	*icon = item->icon;

	if (item->icon != NULL)
	{
		g_object_ref (item->icon);
	}

	return TRUE;
}

static void
insert_path_item (LapizFileBrowserWidget * obj,
                  GFile * file,
		  CtkTreeIter * after,
		  CtkTreeIter * iter,
		  guint indent)
{
	gchar * unescape;
	GdkPixbuf * icon = NULL;

	/* Try to get the icon and name from the bookmarks hash */
	if (!get_from_bookmark_file (obj, file, &unescape, &icon)) {
		/* It's not a bookmark, fetch the name and the icon ourselves */
		unescape = lapiz_file_browser_utils_file_basename (file);

		/* Get the icon */
		icon = lapiz_file_browser_utils_pixbuf_from_file (file, CTK_ICON_SIZE_MENU);
	}

	ctk_tree_store_insert_after (obj->priv->combo_model, iter, NULL,
				     after);

	ctk_tree_store_set (obj->priv->combo_model,
	                    iter,
	                    COLUMN_INDENT, indent,
	                    COLUMN_ICON, icon,
	                    COLUMN_NAME, unescape,
	                    COLUMN_FILE, file,
			    COLUMN_ID, PATH_ID,
			    -1);

	if (icon)
		g_object_unref (icon);

	g_free (unescape);
}

static void
insert_separator_item (LapizFileBrowserWidget * obj)
{
	CtkTreeIter iter;

	ctk_tree_store_insert (obj->priv->combo_model, &iter, NULL, 1);
	ctk_tree_store_set (obj->priv->combo_model, &iter,
			    COLUMN_ICON, NULL,
			    COLUMN_NAME, NULL,
			    COLUMN_ID, SEPARATOR_ID, -1);
}

static void
combo_set_active_by_id (LapizFileBrowserWidget * obj, guint id)
{
	CtkTreeIter iter;

	if (combo_find_by_id (obj, id, &iter))
		ctk_combo_box_set_active_iter (CTK_COMBO_BOX
					       (obj->priv->combo), &iter);
}

static guint
uri_num_parents (GFile * from, GFile * to)
{
	/* Determine the number of 'levels' to get from #from to #to. */
	guint parents = 0;
	GFile * parent;

	if (from == NULL)
		return 0;

	g_object_ref (from);

	while ((parent = g_file_get_parent (from)) && !(to && g_file_equal (from, to))) {
		g_object_unref (from);
		from = parent;

		++parents;
	}

	g_object_unref (from);
	return parents;
}

static void
insert_location_path (LapizFileBrowserWidget * obj)
{
	Location *loc;
	GFile *current = NULL;
	GFile * tmp;
	CtkTreeIter separator;
	CtkTreeIter iter;
	guint indent;

	if (!obj->priv->current_location) {
		g_message ("insert_location_path: no current location");
		return;
	}

	loc = (Location *) (obj->priv->current_location->data);

	current = loc->virtual_root;
	combo_find_by_id (obj, SEPARATOR_ID, &separator);

	indent = uri_num_parents (loc->virtual_root, loc->root);

	while (current != NULL) {
		insert_path_item (obj, current, &separator, &iter, indent--);

		if (current == loc->virtual_root) {
			g_signal_handlers_block_by_func (obj->priv->combo,
							 on_combo_changed,
							 obj);
			ctk_combo_box_set_active_iter (CTK_COMBO_BOX
						       (obj->priv->combo),
						       &iter);
			g_signal_handlers_unblock_by_func (obj->priv->
							   combo,
							   on_combo_changed,
							   obj);
		}

		if (g_file_equal (current, loc->root) || !lapiz_utils_file_has_parent (current)) {
			if (current != loc->virtual_root)
				g_object_unref (current);
			break;
		}

		tmp = g_file_get_parent (current);

		if (current != loc->virtual_root)
			g_object_unref (current);

		current = tmp;
	}
}

static void
check_current_item (LapizFileBrowserWidget * obj, gboolean show_path)
{
	CtkTreeIter separator;
	gboolean has_sep;

	remove_path_items (obj);
	has_sep = combo_find_by_id (obj, SEPARATOR_ID, &separator);

	if (show_path) {
		if (!has_sep)
			insert_separator_item (obj);

		insert_location_path (obj);
	} else if (has_sep)
		ctk_tree_store_remove (obj->priv->combo_model, &separator);
}

static void
fill_combo_model (LapizFileBrowserWidget * obj)
{
	CtkTreeStore *store = obj->priv->combo_model;
	CtkTreeIter iter;
	GdkPixbuf *icon;

	icon = lapiz_file_browser_utils_pixbuf_from_theme ("go-home", CTK_ICON_SIZE_MENU);

	ctk_tree_store_append (store, &iter, NULL);
	ctk_tree_store_set (store, &iter,
			    COLUMN_ICON, icon,
			    COLUMN_NAME, _("Bookmarks"),
			    COLUMN_ID, BOOKMARKS_ID, -1);
	g_object_unref (icon);

	ctk_combo_box_set_row_separator_func (CTK_COMBO_BOX (obj->priv->combo),
					      separator_func, obj, NULL);
	ctk_combo_box_set_active (CTK_COMBO_BOX (obj->priv->combo), 0);
}

static void
indent_cell_data_func (CtkCellLayout   *cell_layout G_GNUC_UNUSED,
		       CtkCellRenderer *cell,
		       CtkTreeModel    *model,
		       CtkTreeIter     *iter,
		       gpointer         data G_GNUC_UNUSED)
{
	gchar * indent;
	guint num;

	ctk_tree_model_get (model, iter, COLUMN_INDENT, &num, -1);

	if (num == 0)
		g_object_set (cell, "text", "", NULL);
	else {
		indent = g_strnfill (num * 2, ' ');

		g_object_set (cell, "text", indent, NULL);
		g_free (indent);
	}
}

static void
create_combo (LapizFileBrowserWidget * obj)
{
	CtkCellRenderer *renderer;

	obj->priv->combo_model = ctk_tree_store_new (N_COLUMNS,
						     G_TYPE_UINT,
						     GDK_TYPE_PIXBUF,
						     G_TYPE_STRING,
						     G_TYPE_FILE,
						     G_TYPE_UINT);
	obj->priv->combo =
	    ctk_combo_box_new_with_model (CTK_TREE_MODEL
					  (obj->priv->combo_model));

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (obj->priv->combo),
	                            renderer, FALSE);
	ctk_cell_layout_set_cell_data_func (CTK_CELL_LAYOUT
					    (obj->priv->combo), renderer,
					    indent_cell_data_func, obj, NULL);


	renderer = ctk_cell_renderer_pixbuf_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (obj->priv->combo),
				    renderer, FALSE);
	ctk_cell_layout_add_attribute (CTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "pixbuf", COLUMN_ICON);

	renderer = ctk_cell_renderer_text_new ();
	ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (obj->priv->combo),
				    renderer, TRUE);
	ctk_cell_layout_add_attribute (CTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "text", COLUMN_NAME);

	g_object_set (renderer, "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	ctk_box_pack_start (CTK_BOX (obj), CTK_WIDGET (obj->priv->combo),
			    FALSE, FALSE, 0);

	fill_combo_model (obj);
	g_signal_connect (obj->priv->combo, "changed",
			  G_CALLBACK (on_combo_changed), obj);

	ctk_widget_show (obj->priv->combo);
}

static CtkActionEntry toplevel_actions[] =
{
	{"FilterMenuAction", NULL, N_("_Filter")}
};

static const CtkActionEntry tree_actions_selection[] =
{
	{"FileMoveToTrash", "cafe-stock-trash", N_("_Move to Trash"), NULL,
	 N_("Move selected file or folder to trash"),
	 G_CALLBACK (on_action_file_move_to_trash)},
	{"FileDelete", "edit-delete", N_("_Delete"), NULL,
	 N_("Delete selected file or folder"),
	 G_CALLBACK (on_action_file_delete)}
};

static const CtkActionEntry tree_actions_file_selection[] =
{
	{"FileOpen", "document-open", N_("_Open"), NULL,
	 N_("Open selected file"),
	 G_CALLBACK (on_action_file_open)}
};

static const CtkActionEntry tree_actions[] =
{
	{"DirectoryUp", "go-up", N_("Up"), NULL,
	 N_("Open the parent folder"), G_CALLBACK (on_action_directory_up)}
};

static const CtkActionEntry tree_actions_single_most_selection[] =
{
	{"DirectoryNew", "list-add", N_("_New Folder"), NULL,
	 N_("Add new empty folder"),
	 G_CALLBACK (on_action_directory_new)},
	{"FileNew", "document-new", N_("New F_ile"), NULL,
	 N_("Add new empty file"), G_CALLBACK (on_action_file_new)}
};

static const CtkActionEntry tree_actions_single_selection[] =
{
	{"FileRename", NULL, N_("_Rename"), NULL,
	 N_("Rename selected file or folder"),
	 G_CALLBACK (on_action_file_rename)}
};

static const CtkActionEntry tree_actions_sensitive[] =
{
	{"DirectoryPrevious", "go-previous", N_("_Previous Location"),
	 NULL,
	 N_("Go to the previous visited location"),
	 G_CALLBACK (on_action_directory_previous)},
	{"DirectoryNext", "go-next", N_("_Next Location"), NULL,
	 N_("Go to the next visited location"), G_CALLBACK (on_action_directory_next)},
	{"DirectoryRefresh", "view-refresh", N_("Re_fresh View"), NULL,
	 N_("Refresh the view"), G_CALLBACK (on_action_directory_refresh)},
	{"DirectoryOpen", "document-open", N_("_View Folder"), NULL,
	 N_("View folder in file manager"),
	 G_CALLBACK (on_action_directory_open)}
};

static const CtkToggleActionEntry tree_actions_toggle[] =
{
	{"FilterHidden", "dialog-password",
	 N_("Show _Hidden"), NULL,
	 N_("Show hidden files and folders"),
	 G_CALLBACK (on_action_filter_hidden), FALSE},
	{"FilterBinary", NULL, N_("Show _Binary"), NULL,
	 N_("Show binary files"), G_CALLBACK (on_action_filter_binary),
	 FALSE}
};

static const CtkActionEntry bookmark_actions[] =
{
	{"BookmarkOpen", "document-open", N_("_View Folder"), NULL,
	 N_("View folder in file manager"), G_CALLBACK (on_action_bookmark_open)}
};

static void
create_toolbar (LapizFileBrowserWidget * obj,
		const gchar *data_dir)
{
	CtkUIManager *manager;
	GError *error = NULL;
	CtkActionGroup *action_group;
	CtkWidget *toolbar;
	CtkWidget *widget;
	CtkAction *action;
	gchar *ui_file;

	manager = ctk_ui_manager_new ();
	obj->priv->manager = manager;

	ui_file = g_build_filename (data_dir, XML_UI_FILE, NULL);
	ctk_ui_manager_add_ui_from_file (manager, ui_file, &error);

	g_free (ui_file);

	if (error != NULL) {
		g_warning ("Error in adding ui from file %s: %s",
			   XML_UI_FILE, error->message);
		g_error_free (error);
		return;
	}

	action_group = ctk_action_group_new ("FileBrowserWidgetActionGroupToplevel");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      toplevel_actions,
				      G_N_ELEMENTS (toplevel_actions),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);

	action_group = ctk_action_group_new ("FileBrowserWidgetActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions,
				      G_N_ELEMENTS (tree_actions),
				      obj);
	ctk_action_group_add_toggle_actions (action_group,
					     tree_actions_toggle,
					     G_N_ELEMENTS (tree_actions_toggle),
					     obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetSelectionActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions_selection,
				      G_N_ELEMENTS (tree_actions_selection),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group_selection = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetFileSelectionActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions_file_selection,
				      G_N_ELEMENTS (tree_actions_file_selection),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group_file_selection = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetSingleSelectionActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions_single_selection,
				      G_N_ELEMENTS (tree_actions_single_selection),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group_single_selection = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetSingleMostSelectionActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions_single_most_selection,
				      G_N_ELEMENTS (tree_actions_single_most_selection),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group_single_most_selection = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetSensitiveActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      tree_actions_sensitive,
				      G_N_ELEMENTS (tree_actions_sensitive),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->action_group_sensitive = action_group;

	action_group = ctk_action_group_new ("FileBrowserWidgetBookmarkActionGroup");
	ctk_action_group_set_translation_domain (action_group, NULL);
	ctk_action_group_add_actions (action_group,
				      bookmark_actions,
				      G_N_ELEMENTS (bookmark_actions),
				      obj);
	ctk_ui_manager_insert_action_group (manager, action_group, 0);
	obj->priv->bookmark_action_group = action_group;

	action = ctk_action_group_get_action (obj->priv->action_group_sensitive,
					      "DirectoryPrevious");
	ctk_action_set_sensitive (action, FALSE);

	action = ctk_action_group_get_action (obj->priv->action_group_sensitive,
					      "DirectoryNext");
	ctk_action_set_sensitive (action, FALSE);

	toolbar = ctk_ui_manager_get_widget (manager, "/ToolBar");
	ctk_toolbar_set_style (CTK_TOOLBAR (toolbar), CTK_TOOLBAR_ICONS);
	ctk_toolbar_set_icon_size (CTK_TOOLBAR (toolbar), CTK_ICON_SIZE_MENU);

	/* Previous directory menu tool item */
	obj->priv->location_previous_menu = ctk_menu_new ();

	ctk_menu_set_reserve_toggle_size (CTK_MENU (obj->priv->location_previous_menu), FALSE);

	ctk_widget_show (obj->priv->location_previous_menu);

	widget = CTK_WIDGET (ctk_menu_tool_button_new (ctk_image_new_from_icon_name ("go-previous",
										     CTK_ICON_SIZE_MENU),
						       _("Previous location")));

	ctk_menu_tool_button_set_menu (CTK_MENU_TOOL_BUTTON (widget),
				       obj->priv->location_previous_menu);

	ctk_tool_item_set_tooltip_text (CTK_TOOL_ITEM (widget),
					_("Go to previous location"));
	ctk_menu_tool_button_set_arrow_tooltip_text (CTK_MENU_TOOL_BUTTON (widget),
						     _("Go to a previously opened location"));

	action = ctk_action_group_get_action (obj->priv->action_group_sensitive,
					      "DirectoryPrevious");
	g_object_set (action, "is_important", TRUE, "short_label",
		      _("Previous location"), NULL);
	ctk_activatable_set_related_action (CTK_ACTIVATABLE (widget), action);
	ctk_toolbar_insert (CTK_TOOLBAR (toolbar), CTK_TOOL_ITEM (widget), 0);

	/* Next directory menu tool item */
	obj->priv->location_next_menu = ctk_menu_new ();

	ctk_menu_set_reserve_toggle_size (CTK_MENU (obj->priv->location_next_menu), FALSE);

	ctk_widget_show (obj->priv->location_next_menu);

	widget = CTK_WIDGET (ctk_menu_tool_button_new (ctk_image_new_from_icon_name ("go-next",
										     CTK_ICON_SIZE_MENU),
						       _("Next location")));

	ctk_menu_tool_button_set_menu (CTK_MENU_TOOL_BUTTON (widget),
				       obj->priv->location_next_menu);

	ctk_tool_item_set_tooltip_text (CTK_TOOL_ITEM (widget),
					_("Go to next location"));
	ctk_menu_tool_button_set_arrow_tooltip_text (CTK_MENU_TOOL_BUTTON (widget),
						     _("Go to a previously opened location"));

	action = ctk_action_group_get_action (obj->priv->action_group_sensitive,
					      "DirectoryNext");
	g_object_set (action, "is_important", TRUE, "short_label",
		      _("Previous location"), NULL);
	ctk_activatable_set_related_action (CTK_ACTIVATABLE (widget), action);
	ctk_toolbar_insert (CTK_TOOLBAR (toolbar), CTK_TOOL_ITEM (widget), 1);

	ctk_box_pack_start (CTK_BOX (obj), toolbar, FALSE, FALSE, 0);
	ctk_widget_show (toolbar);

	set_enable_delete (obj, obj->priv->enable_delete);
}

static void
set_enable_delete (LapizFileBrowserWidget *obj,
		   gboolean enable)
{
	CtkAction *action;
	obj->priv->enable_delete = enable;

	if (obj->priv->action_group_selection == NULL)
		return;

	action =
	    ctk_action_group_get_action (obj->priv->action_group_selection,
					 "FileDelete");

	g_object_set (action, "visible", enable, "sensitive", enable, NULL);
}

static gboolean
filter_real (LapizFileBrowserStore * model, CtkTreeIter * iter,
	     LapizFileBrowserWidget * obj)
{
	GSList *item;
	FilterFunc *func;

	for (item = obj->priv->filter_funcs; item; item = item->next) {
		func = (FilterFunc *) (item->data);

		if (!func->func (obj, model, iter, func->user_data))
			return FALSE;
	}

	return TRUE;
}

static void
add_bookmark_hash (LapizFileBrowserWidget * obj,
                   CtkTreeIter * iter)
{
	CtkTreeModel *model;
	GdkPixbuf * pixbuf;
	gchar * name;
	gchar * uri;
	GFile * file;
	NameIcon * item;

	model = CTK_TREE_MODEL (obj->priv->bookmarks_store);

	uri = lapiz_file_bookmarks_store_get_uri (obj->priv->
						  bookmarks_store,
						  iter);

	if (uri == NULL)
		return;

	file = g_file_new_for_uri (uri);
	g_free (uri);

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_ICON,
			    &pixbuf,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_NAME,
			    &name, -1);

	item = g_new (NameIcon, 1);
	item->name = name;
	item->icon = pixbuf;

	g_hash_table_insert (obj->priv->bookmarks_hash,
			     file,
			     item);
}

static void
init_bookmarks_hash (LapizFileBrowserWidget * obj)
{
	CtkTreeIter iter;
	CtkTreeModel *model;

	model = CTK_TREE_MODEL (obj->priv->bookmarks_store);

	if (!ctk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		add_bookmark_hash (obj, &iter);
	} while (ctk_tree_model_iter_next (model, &iter));

	g_signal_connect (obj->priv->bookmarks_store,
		          "row-changed",
		          G_CALLBACK (on_bookmarks_row_changed),
		          obj);

	g_signal_connect (obj->priv->bookmarks_store,
		          "row-deleted",
		          G_CALLBACK (on_bookmarks_row_deleted),
		          obj);
}

static void
on_begin_loading (LapizFileBrowserStore  *model G_GNUC_UNUSED,
		  CtkTreeIter            *iter G_GNUC_UNUSED,
		  LapizFileBrowserWidget *obj)
{
	if (!CDK_IS_WINDOW (ctk_widget_get_window (CTK_WIDGET (obj->priv->treeview))))
		return;

	cdk_window_set_cursor (ctk_widget_get_window (CTK_WIDGET (obj)),
			       obj->priv->busy_cursor);
}

static void
on_end_loading (LapizFileBrowserStore  *model G_GNUC_UNUSED,
		CtkTreeIter            *iter G_GNUC_UNUSED,
		LapizFileBrowserWidget *obj)
{
	if (!CDK_IS_WINDOW (ctk_widget_get_window (CTK_WIDGET (obj->priv->treeview))))
		return;

	cdk_window_set_cursor (ctk_widget_get_window (CTK_WIDGET (obj)), NULL);
}

static void
create_tree (LapizFileBrowserWidget * obj)
{
	CtkWidget *sw;

	obj->priv->file_store = lapiz_file_browser_store_new (NULL);
	obj->priv->bookmarks_store = lapiz_file_bookmarks_store_new ();
	obj->priv->treeview =
	    LAPIZ_FILE_BROWSER_VIEW (lapiz_file_browser_view_new ());

	lapiz_file_browser_view_set_restore_expand_state (obj->priv->treeview, TRUE);

	lapiz_file_browser_store_set_filter_mode (obj->priv->file_store,
						  LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN
						  |
						  LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY);
	lapiz_file_browser_store_set_filter_func (obj->priv->file_store,
						  (LapizFileBrowserStoreFilterFunc)
						  filter_real, obj);

	sw = ctk_scrolled_window_new (NULL, NULL);
	ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (sw),
					     CTK_SHADOW_ETCHED_IN);
	ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (sw),
					CTK_POLICY_AUTOMATIC,
					CTK_POLICY_AUTOMATIC);

	ctk_container_add (CTK_CONTAINER (sw),
			   CTK_WIDGET (obj->priv->treeview));
	ctk_box_pack_start (CTK_BOX (obj), sw, TRUE, TRUE, 0);

	g_signal_connect (obj->priv->treeview, "notify::model",
			  G_CALLBACK (on_model_set), obj);
	g_signal_connect (obj->priv->treeview, "error",
			  G_CALLBACK (on_treeview_error), obj);
	g_signal_connect (obj->priv->treeview, "popup-menu",
			  G_CALLBACK (on_treeview_popup_menu), obj);
	g_signal_connect (obj->priv->treeview, "button-press-event",
			  G_CALLBACK (on_treeview_button_press_event),
			  obj);
	g_signal_connect (obj->priv->treeview, "key-press-event",
			  G_CALLBACK (on_treeview_key_press_event), obj);

	g_signal_connect (ctk_tree_view_get_selection
			  (CTK_TREE_VIEW (obj->priv->treeview)), "changed",
			  G_CALLBACK (on_selection_changed), obj);
	g_signal_connect (obj->priv->file_store, "notify::filter-mode",
			  G_CALLBACK (on_filter_mode_changed), obj);

	g_signal_connect (obj->priv->file_store, "notify::virtual-root",
			  G_CALLBACK (on_virtual_root_changed), obj);

	g_signal_connect (obj->priv->file_store, "begin-loading",
			  G_CALLBACK (on_begin_loading), obj);

	g_signal_connect (obj->priv->file_store, "end-loading",
			  G_CALLBACK (on_end_loading), obj);

	g_signal_connect (obj->priv->file_store, "error",
			  G_CALLBACK (on_file_store_error), obj);

	init_bookmarks_hash (obj);

	ctk_widget_show (sw);
	ctk_widget_show (CTK_WIDGET (obj->priv->treeview));
}

static void
create_filter (LapizFileBrowserWidget * obj)
{
	CtkWidget *expander;
	CtkWidget *vbox;
	CtkWidget *entry;

	expander = ctk_expander_new_with_mnemonic (_("_Match Filename"));
	ctk_widget_show (expander);
	ctk_box_pack_start (CTK_BOX (obj), expander, FALSE, FALSE, 0);

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 3);
	ctk_widget_show (vbox);

	obj->priv->filter_expander = expander;

	entry = ctk_entry_new ();
	ctk_widget_show (entry);

	obj->priv->filter_entry = entry;

	g_signal_connect_swapped (entry, "activate",
				  G_CALLBACK (on_entry_filter_activate),
				  obj);
	g_signal_connect_swapped (entry, "focus_out_event",
				  G_CALLBACK (on_entry_filter_activate),
				  obj);

	ctk_box_pack_start (CTK_BOX (vbox), entry, FALSE, FALSE, 0);
	ctk_container_add (CTK_CONTAINER (expander), vbox);
}

static void
lapiz_file_browser_widget_init (LapizFileBrowserWidget * obj)
{
	CdkDisplay *display;

	obj->priv = lapiz_file_browser_widget_get_instance_private (obj);

	obj->priv->bookmarks_hash = g_hash_table_new_full (g_file_hash,
			                                   (GEqualFunc)g_file_equal,
			                                   g_object_unref,
			                                   free_name_icon);

	ctk_box_set_spacing (CTK_BOX (obj), 3);
	ctk_orientable_set_orientation (CTK_ORIENTABLE (obj),
	                                CTK_ORIENTATION_VERTICAL);

	display = ctk_widget_get_display (CTK_WIDGET (obj));
	obj->priv->busy_cursor = cdk_cursor_new_for_display (display, CDK_WATCH);
}

/* Private */

static void
update_sensitivity (LapizFileBrowserWidget * obj)
{
	CtkTreeModel *model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	CtkAction *action;
	gint mode;

	if (LAPIZ_IS_FILE_BROWSER_STORE (model)) {
		ctk_action_group_set_sensitive (obj->priv->action_group,
						TRUE);
		ctk_action_group_set_sensitive (obj->priv->bookmark_action_group,
						FALSE);

		mode =
		    lapiz_file_browser_store_get_filter_mode
		    (LAPIZ_FILE_BROWSER_STORE (model));

		action =
		    ctk_action_group_get_action (obj->priv->action_group,
						 "FilterHidden");
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action),
					      !(mode &
						LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN));
	} else if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model)) {
		ctk_action_group_set_sensitive (obj->priv->action_group,
						FALSE);
		ctk_action_group_set_sensitive (obj->priv->bookmark_action_group,
						TRUE);

		/* Set the filter toggle to normal up state, just for visual pleasure */
		action =
		    ctk_action_group_get_action (obj->priv->action_group,
						 "FilterHidden");
		ctk_toggle_action_set_active (CTK_TOGGLE_ACTION (action),
					      FALSE);
	}

	on_selection_changed (ctk_tree_view_get_selection
			      (CTK_TREE_VIEW (obj->priv->treeview)), obj);
}

static gboolean
lapiz_file_browser_widget_get_first_selected (LapizFileBrowserWidget *obj,
					      CtkTreeIter *iter)
{
	CtkTreeSelection *selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));
	CtkTreeModel *model;
	GList *rows = ctk_tree_selection_get_selected_rows (selection, &model);
	gboolean result;

	if (!rows)
		return FALSE;

	result = ctk_tree_model_get_iter(model, iter, (CtkTreePath *)(rows->data));

	g_list_foreach (rows, (GFunc)ctk_tree_path_free, NULL);
	g_list_free (rows);

	return result;
}

static gboolean
popup_menu (LapizFileBrowserWidget * obj, CdkEventButton * event, CtkTreeModel * model)
{
	CtkWidget *menu;

	if (LAPIZ_IS_FILE_BROWSER_STORE (model))
		menu = ctk_ui_manager_get_widget (obj->priv->manager, "/FilePopup");
	else if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model))
		menu = ctk_ui_manager_get_widget (obj->priv->manager, "/BookmarkPopup");
	else
		return FALSE;

	g_return_val_if_fail (menu != NULL, FALSE);

	if (event != NULL) {
		CtkTreeSelection *selection;
		selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));

		if (ctk_tree_selection_count_selected_rows (selection) <= 1) {
			CtkTreePath *path;

			if (ctk_tree_view_get_path_at_pos (CTK_TREE_VIEW (obj->priv->treeview),
			                                   (gint)event->x, (gint)event->y,
			                                   &path, NULL, NULL, NULL))
			{
				ctk_tree_selection_unselect_all (selection);
				ctk_tree_selection_select_path (selection, path);
				ctk_tree_path_free (path);
			}
		}

		ctk_menu_popup_at_pointer (CTK_MENU (menu), NULL);
	} else {
		menu_popup_at_treeview_selection (menu, CTK_WIDGET (obj->priv->treeview));
		ctk_menu_shell_select_first (CTK_MENU_SHELL (menu), FALSE);
	}

	return TRUE;
}

static gboolean
filter_glob (LapizFileBrowserWidget *obj,
	     LapizFileBrowserStore  *store,
	     CtkTreeIter            *iter,
	     gpointer                user_data G_GNUC_UNUSED)
{
	gchar *name;
	gboolean result;
	guint flags;

	if (obj->priv->filter_pattern == NULL)
		return TRUE;

	ctk_tree_model_get (CTK_TREE_MODEL (store), iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_NAME, &name,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
			    -1);

	if (FILE_IS_DIR (flags) || FILE_IS_DUMMY (flags))
		result = TRUE;
	else
		result =
		    g_pattern_match_string (obj->priv->filter_pattern,
					    name);

	g_free (name);

	return result;
}

static void
rename_selected_file (LapizFileBrowserWidget * obj)
{
	CtkTreeModel *model;
	CtkTreeIter iter;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	if (lapiz_file_browser_widget_get_first_selected (obj, &iter))
		lapiz_file_browser_view_start_rename (obj->priv->treeview,
						      &iter);
}

static GList *
get_deletable_files (LapizFileBrowserWidget *obj) {
	CtkTreeSelection *selection;
	CtkTreeModel *model;
	GList *rows;
	GList *row;
	GList *paths = NULL;
	guint flags;
	CtkTreeIter iter;
	CtkTreePath *path;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	/* Get all selected files */
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));
	rows = ctk_tree_selection_get_selected_rows (selection, &model);

	for (row = rows; row; row = row->next) {
		path = (CtkTreePath *)(row->data);

		if (!ctk_tree_model_get_iter (model, &iter, path))
			continue;

		ctk_tree_model_get (model, &iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS,
				    &flags, -1);

		if (FILE_IS_DUMMY (flags))
			continue;

		paths = g_list_append (paths, ctk_tree_path_copy (path));
	}

	g_list_foreach (rows, (GFunc)ctk_tree_path_free, NULL);
	g_list_free (rows);

	return paths;
}

static gboolean
delete_selected_files (LapizFileBrowserWidget * obj, gboolean trash)
{
	CtkTreeModel *model;
	gboolean confirm;
	LapizFileBrowserStoreResult result;
	GList *rows;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return FALSE;

	rows = get_deletable_files (obj);

	if (!rows)
		return FALSE;

	if (!trash) {
		g_signal_emit (obj, signals[CONFIRM_DELETE], 0, model, rows, &confirm);

		if (!confirm)
			return FALSE;
	}

	result = lapiz_file_browser_store_delete_all (LAPIZ_FILE_BROWSER_STORE (model),
						      rows, trash);

	g_list_foreach (rows, (GFunc)ctk_tree_path_free, NULL);
	g_list_free (rows);

	return result == LAPIZ_FILE_BROWSER_STORE_RESULT_OK;
}

static gboolean
on_file_store_no_trash (LapizFileBrowserStore  *store G_GNUC_UNUSED,
			GList                  *files,
			LapizFileBrowserWidget *obj)
{
	gboolean confirm = FALSE;

	g_signal_emit (obj, signals[CONFIRM_NO_TRASH], 0, files, &confirm);

	return confirm;
}

static GFile *
get_topmost_file (GFile * file)
{
	GFile * tmp;
	GFile * current;

	current = g_object_ref (file);

	while ((tmp = g_file_get_parent (current)) != NULL) {
		g_object_unref (current);
		current = tmp;
	}

	return current;
}

static CtkWidget *
create_goto_menu_item (LapizFileBrowserWidget * obj, GList * item,
		       GdkPixbuf * icon)
{
	CtkWidget *result;
	gchar *unescape;
	GdkPixbuf *pixbuf = NULL;
	Location *loc;

	loc = (Location *) (item->data);

	if (!get_from_bookmark_file (obj, loc->virtual_root, &unescape, &pixbuf)) {
		unescape = lapiz_file_browser_utils_file_basename (loc->virtual_root);

		if (icon)
			pixbuf = g_object_ref (icon);
	}

	if (pixbuf) {
		result = lapiz_image_menu_item_new_from_pixbuf (pixbuf, unescape);
		g_object_unref (pixbuf);
	} else {
		result = ctk_menu_item_new_with_label (unescape);
	}

	g_object_set_data (G_OBJECT (result), LOCATION_DATA_KEY, item);
	g_signal_connect (result, "activate",
			  G_CALLBACK (on_location_jump_activate), obj);

	ctk_widget_show (result);

	g_free (unescape);

	return result;
}

static GList *
list_next_iterator (GList * list)
{
	if (!list)
		return NULL;

	return list->next;
}

static GList *
list_prev_iterator (GList * list)
{
	if (!list)
		return NULL;

	return list->prev;
}

static void
jump_to_location (LapizFileBrowserWidget * obj, GList * item,
		  gboolean previous)
{
	Location *loc;
	CtkWidget *widget;
	GList *children;
	GList *child;
	GList *(*iter_func) (GList *);
	CtkWidget *menu_from;
	CtkWidget *menu_to;
	gchar *root;
	gchar *virtual_root;

	if (!obj->priv->locations)
		return;

	if (previous) {
		iter_func = list_next_iterator;
		menu_from = obj->priv->location_previous_menu;
		menu_to = obj->priv->location_next_menu;
	} else {
		iter_func = list_prev_iterator;
		menu_from = obj->priv->location_next_menu;
		menu_to = obj->priv->location_previous_menu;
	}

	children = ctk_container_get_children (CTK_CONTAINER (menu_from));
	child = children;

	/* This is the menuitem for the current location, which is the first
	   to be added to the menu */
	widget = obj->priv->current_location_menu_item;

	while (obj->priv->current_location != item) {
		if (widget) {
			/* Prepend the menu item to the menu */
			ctk_menu_shell_prepend (CTK_MENU_SHELL (menu_to),
						widget);

			g_object_unref (widget);
		}

		widget = CTK_WIDGET (child->data);

		/* Make sure the widget isn't destroyed when removed */
		g_object_ref (widget);
		ctk_container_remove (CTK_CONTAINER (menu_from), widget);

		obj->priv->current_location_menu_item = widget;

		if (obj->priv->current_location == NULL) {
			obj->priv->current_location = obj->priv->locations;

			if (obj->priv->current_location == item)
				break;
		} else {
			obj->priv->current_location =
			    iter_func (obj->priv->current_location);
		}

		child = child->next;
	}

	g_list_free (children);

	obj->priv->changing_location = TRUE;

	g_assert (obj->priv->current_location != NULL);

	loc = (Location *) (obj->priv->current_location->data);

	/* Set the new root + virtual root */
	root = g_file_get_uri (loc->root);
	virtual_root = g_file_get_uri (loc->virtual_root);

	lapiz_file_browser_widget_set_root_and_virtual_root (obj,
							     root,
							     virtual_root);

	g_free (root);
	g_free (virtual_root);

	obj->priv->changing_location = FALSE;
}

static void
clear_next_locations (LapizFileBrowserWidget * obj)
{
	GList *children;
	GList *item;

	if (obj->priv->current_location == NULL)
		return;

	while (obj->priv->current_location->prev) {
		location_free ((Location *) (obj->priv->current_location->
					     prev->data));
		obj->priv->locations =
		    g_list_remove_link (obj->priv->locations,
					obj->priv->current_location->prev);
	}

	children =
	    ctk_container_get_children (CTK_CONTAINER
					(obj->priv->location_next_menu));

	for (item = children; item; item = item->next) {
		ctk_container_remove (CTK_CONTAINER
				      (obj->priv->location_next_menu),
				      CTK_WIDGET (item->data));
	}

	g_list_free (children);

	ctk_action_set_sensitive (ctk_action_group_get_action
				  (obj->priv->action_group_sensitive,
				   "DirectoryNext"), FALSE);
}

static void
update_filter_mode (LapizFileBrowserWidget * obj,
                    CtkAction * action,
                    LapizFileBrowserStoreFilterMode mode)
{
	gboolean active =
	    ctk_toggle_action_get_active (CTK_TOGGLE_ACTION (action));
	CtkTreeModel *model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	gint now;

	if (LAPIZ_IS_FILE_BROWSER_STORE (model)) {
		now =
		    lapiz_file_browser_store_get_filter_mode
		    (LAPIZ_FILE_BROWSER_STORE (model));

		if (active)
			now &= ~mode;
		else
			now |= mode;

		lapiz_file_browser_store_set_filter_mode
		    (LAPIZ_FILE_BROWSER_STORE (model), now);
	}
}

static void
set_filter_pattern_real (LapizFileBrowserWidget * obj,
                        gchar const * pattern,
                        gboolean update_entry)
{
	CtkTreeModel *model;

	model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (pattern != NULL && *pattern == '\0')
		pattern = NULL;

	if (pattern == NULL && obj->priv->filter_pattern_str == NULL)
		return;

	if (pattern != NULL && obj->priv->filter_pattern_str != NULL &&
	    strcmp (pattern, obj->priv->filter_pattern_str) == 0)
		return;

	/* Free the old pattern */
	g_free (obj->priv->filter_pattern_str);
	obj->priv->filter_pattern_str = g_strdup (pattern);

	if (obj->priv->filter_pattern) {
		g_pattern_spec_free (obj->priv->filter_pattern);
		obj->priv->filter_pattern = NULL;
	}

	if (pattern == NULL) {
		if (obj->priv->glob_filter_id != 0) {
			lapiz_file_browser_widget_remove_filter (obj,
								 obj->
								 priv->
								 glob_filter_id);
			obj->priv->glob_filter_id = 0;
		}
	} else {
		obj->priv->filter_pattern = g_pattern_spec_new (pattern);

		if (obj->priv->glob_filter_id == 0)
			obj->priv->glob_filter_id =
			    lapiz_file_browser_widget_add_filter (obj,
								  filter_glob,
								  NULL,
								  NULL);
	}

	if (update_entry) {
		if (obj->priv->filter_pattern_str == NULL)
			ctk_entry_set_text (CTK_ENTRY (obj->priv->filter_entry),
			                    "");
		else {
			ctk_entry_set_text (CTK_ENTRY (obj->priv->filter_entry),
			                    obj->priv->filter_pattern_str);

			ctk_expander_set_expanded (CTK_EXPANDER (obj->priv->filter_expander),
		        	                   TRUE);
		}
	}

	if (LAPIZ_IS_FILE_BROWSER_STORE (model))
		lapiz_file_browser_store_refilter (LAPIZ_FILE_BROWSER_STORE
						   (model));

	g_object_notify (G_OBJECT (obj), "filter-pattern");
}


/* Public */

CtkWidget *
lapiz_file_browser_widget_new (const gchar *data_dir)
{
	LapizFileBrowserWidget *obj =
	    g_object_new (LAPIZ_TYPE_FILE_BROWSER_WIDGET, NULL);

	create_toolbar (obj, data_dir);
	create_combo (obj);
	create_tree (obj);
	create_filter (obj);

	lapiz_file_browser_widget_show_bookmarks (obj);

	return CTK_WIDGET (obj);
}

void
lapiz_file_browser_widget_show_bookmarks (LapizFileBrowserWidget * obj)
{
	/* Select bookmarks in the combo box */
	g_signal_handlers_block_by_func (obj->priv->combo,
					 on_combo_changed, obj);
	combo_set_active_by_id (obj, BOOKMARKS_ID);
	g_signal_handlers_unblock_by_func (obj->priv->combo,
					   on_combo_changed, obj);

	check_current_item (obj, FALSE);

	lapiz_file_browser_view_set_model (obj->priv->treeview,
					   CTK_TREE_MODEL (obj->priv->
							   bookmarks_store));
}

static void
show_files_real (LapizFileBrowserWidget *obj,
		 gboolean                do_root_changed)
{
	lapiz_file_browser_view_set_model (obj->priv->treeview,
					   CTK_TREE_MODEL (obj->priv->
							   file_store));

	if (do_root_changed)
		on_virtual_root_changed (obj->priv->file_store, NULL, obj);
}

void
lapiz_file_browser_widget_show_files (LapizFileBrowserWidget * obj)
{
	show_files_real (obj, TRUE);
}

void
lapiz_file_browser_widget_set_root_and_virtual_root (LapizFileBrowserWidget *obj,
						     gchar const *root,
						     gchar const *virtual_root)
{
	LapizFileBrowserStoreResult result;

	if (!virtual_root)
		result =
		    lapiz_file_browser_store_set_root_and_virtual_root
		    (obj->priv->file_store, root, root);
	else
		result =
		    lapiz_file_browser_store_set_root_and_virtual_root
		    (obj->priv->file_store, root, virtual_root);

	if (result == LAPIZ_FILE_BROWSER_STORE_RESULT_NO_CHANGE)
		show_files_real (obj, TRUE);
}

void
lapiz_file_browser_widget_set_root (LapizFileBrowserWidget * obj,
				    gchar const *root,
				    gboolean virtual_root)
{
	GFile *file;
	GFile *parent;
	gchar *str;

	if (!virtual_root) {
		lapiz_file_browser_widget_set_root_and_virtual_root (obj,
								     root,
								     NULL);
		return;
	}

	if (!root)
		return;

	file = g_file_new_for_uri (root);
	parent = get_topmost_file (file);
	str = g_file_get_uri (parent);

	lapiz_file_browser_widget_set_root_and_virtual_root
	    (obj, str, root);

	g_free (str);

	g_object_unref (file);
	g_object_unref (parent);
}

LapizFileBrowserStore *
lapiz_file_browser_widget_get_browser_store (LapizFileBrowserWidget * obj)
{
	return obj->priv->file_store;
}

LapizFileBookmarksStore *
lapiz_file_browser_widget_get_bookmarks_store (LapizFileBrowserWidget * obj)
{
	return obj->priv->bookmarks_store;
}

LapizFileBrowserView *
lapiz_file_browser_widget_get_browser_view (LapizFileBrowserWidget * obj)
{
	return obj->priv->treeview;
}

CtkUIManager *
lapiz_file_browser_widget_get_ui_manager (LapizFileBrowserWidget * obj)
{
	return obj->priv->manager;
}

CtkWidget *
lapiz_file_browser_widget_get_filter_entry (LapizFileBrowserWidget * obj)
{
	return obj->priv->filter_entry;
}

gulong
lapiz_file_browser_widget_add_filter (LapizFileBrowserWidget * obj,
				      LapizFileBrowserWidgetFilterFunc func,
				      gpointer user_data,
				      GDestroyNotify notify)
{
	FilterFunc *f;
	CtkTreeModel *model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	f = filter_func_new (obj, func, user_data, notify);
	obj->priv->filter_funcs =
	    g_slist_append (obj->priv->filter_funcs, f);

	if (LAPIZ_IS_FILE_BROWSER_STORE (model))
		lapiz_file_browser_store_refilter (LAPIZ_FILE_BROWSER_STORE
						   (model));

	return f->id;
}

void
lapiz_file_browser_widget_remove_filter (LapizFileBrowserWidget * obj,
					 gulong id)
{
	GSList *item;
	FilterFunc *func;

	for (item = obj->priv->filter_funcs; item; item = item->next)
	{
		func = (FilterFunc *) (item->data);

		if (func->id == id)
		{
			if (func->destroy_notify)
				func->destroy_notify (func->user_data);

			obj->priv->filter_funcs =
			    g_slist_remove_link (obj->priv->filter_funcs,
						 item);
			g_free (func);
			break;
		}
	}
}

void
lapiz_file_browser_widget_set_filter_pattern (LapizFileBrowserWidget * obj,
                                              gchar const *pattern)
{
	set_filter_pattern_real (obj, pattern, TRUE);
}

gboolean
lapiz_file_browser_widget_get_selected_directory (LapizFileBrowserWidget * obj,
						  CtkTreeIter * iter)
{
	CtkTreeModel *model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	CtkTreeIter parent;
	guint flags;

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return FALSE;

	if (!lapiz_file_browser_widget_get_first_selected (obj, iter)) {
		if (!lapiz_file_browser_store_get_iter_virtual_root
		    (LAPIZ_FILE_BROWSER_STORE (model), iter))
			return FALSE;
	}

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
			    -1);

	if (!FILE_IS_DIR (flags)) {
		/* Get the parent, because the selection is a file */
		ctk_tree_model_iter_parent (model, &parent, iter);
		*iter = parent;
	}

	return TRUE;
}

static guint
lapiz_file_browser_widget_get_num_selected_files_or_directories (LapizFileBrowserWidget *obj,
								 guint                  *files,
								 guint                  *dirs)
{
	GList *rows, *row;
	CtkTreePath *path;
	CtkTreeIter iter;
	LapizFileBrowserStoreFlag flags;
	guint result = 0;
	CtkTreeSelection *selection;
	CtkTreeModel *model;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));
	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model))
		return 0;

	rows = ctk_tree_selection_get_selected_rows (selection, &model);

	for (row = rows; row; row = row->next) {
		path = (CtkTreePath *)(row->data);

		/* Get iter from path */
		if (!ctk_tree_model_get_iter (model, &iter, path))
			continue;

		ctk_tree_model_get (model, &iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
				    -1);

		if (!FILE_IS_DUMMY (flags)) {
			if (!FILE_IS_DIR (flags))
				++(*files);
			else
				++(*dirs);

			++result;
		}
	}

	g_list_foreach (rows, (GFunc)ctk_tree_path_free, NULL);
	g_list_free (rows);

	return result;
}

typedef struct
{
	LapizFileBrowserWidget *widget;
	GCancellable *cancellable;
} AsyncData;

static AsyncData *
async_data_new (LapizFileBrowserWidget *widget)
{
	AsyncData *ret;

	ret = g_new (AsyncData, 1);
	ret->widget = widget;

	cancel_async_operation (widget);
	widget->priv->cancellable = g_cancellable_new ();

	ret->cancellable = g_object_ref (widget->priv->cancellable);

	return ret;
}

static void
async_free (AsyncData *async)
{
	g_object_unref (async->cancellable);
	g_free (async);
}

static void
set_busy (LapizFileBrowserWidget *obj, gboolean busy)
{
	CdkWindow *window;

	window = ctk_widget_get_window (CTK_WIDGET (obj->priv->treeview));

	if (!CDK_IS_WINDOW (window))
		return;

	if (busy)
	{
		CdkDisplay *display;
		CdkCursor *cursor;

		display = ctk_widget_get_display (CTK_WIDGET (obj));
		cursor = cdk_cursor_new_for_display (display, CDK_WATCH);
		cdk_window_set_cursor (window, cursor);
		g_object_unref (obj->priv->busy_cursor);
	}
	else
	{
		cdk_window_set_cursor (window, NULL);
	}
}

static void try_mount_volume (LapizFileBrowserWidget *widget, GVolume *volume);

static void
activate_mount (LapizFileBrowserWidget *widget,
		GVolume		       *volume,
		GMount		       *mount)
{
	GFile *root;
	gchar *uri;

	if (!mount)
	{
		gchar *message;
		gchar *name;

		name = g_volume_get_name (volume);
		message = g_strdup_printf (_("No mount object for mounted volume: %s"), name);

		g_signal_emit (widget,
			       signals[ERROR],
			       0,
			       LAPIZ_FILE_BROWSER_ERROR_SET_ROOT,
			       message);

		g_free (name);
		g_free (message);
		return;
	}

	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	lapiz_file_browser_widget_set_root (widget, uri, FALSE);

	g_free (uri);
	g_object_unref (root);
}

static void
try_activate_drive (LapizFileBrowserWidget *widget,
		    GDrive 		   *drive)
{
	GList *volumes;
	GVolume *volume;
	GMount *mount;

	volumes = g_drive_get_volumes (drive);

	volume = G_VOLUME (volumes->data);
	mount = g_volume_get_mount (volume);

	if (mount)
	{
		/* try set the root of the mount */
		activate_mount (widget, volume, mount);
		g_object_unref (mount);
	}
	else
	{
		/* try to mount it then? */
		try_mount_volume (widget, volume);
	}

	g_list_foreach (volumes, (GFunc)g_object_unref, NULL);
	g_list_free (volumes);
}

static void
poll_for_media_cb (GDrive       *drive,
		   GAsyncResult *res,
		   AsyncData 	*async)
{
	GError *error = NULL;

	/* check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_free (async);
		return;
	}

	/* finish poll operation */
	set_busy (async->widget, FALSE);

	if (g_drive_poll_for_media_finish (drive, res, &error) &&
	    g_drive_has_media (drive) &&
	    g_drive_has_volumes (drive))
	{
		try_activate_drive (async->widget, drive);
	}
	else
	{
		gchar *message;
		gchar *name;

		name = g_drive_get_name (drive);
		message = g_strdup_printf (_("Could not open media: %s"), name);

		g_signal_emit (async->widget,
			       signals[ERROR],
			       0,
			       LAPIZ_FILE_BROWSER_ERROR_SET_ROOT,
			       message);

		g_free (name);
		g_free (message);

		g_error_free (error);
	}

	async_free (async);
}

static void
mount_volume_cb (GVolume      *volume,
		 GAsyncResult *res,
		 AsyncData    *async)
{
	GError *error = NULL;

	/* check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_free (async);
		return;
	}

	if (g_volume_mount_finish (volume, res, &error))
	{
		GMount *mount;

		mount = g_volume_get_mount (volume);
		activate_mount (async->widget, volume, mount);

		if (mount)
			g_object_unref (mount);
	}
	else
	{
		gchar *message;
		gchar *name;

		name = g_volume_get_name (volume);
		message = g_strdup_printf (_("Could not mount volume: %s"), name);

		g_signal_emit (async->widget,
			       signals[ERROR],
			       0,
			       LAPIZ_FILE_BROWSER_ERROR_SET_ROOT,
			       message);

		g_free (name);
		g_free (message);

		g_error_free (error);
	}

	set_busy (async->widget, FALSE);
	async_free (async);
}

static void
activate_drive (LapizFileBrowserWidget *obj,
		CtkTreeIter	       *iter)
{
	GDrive *drive;
	AsyncData *async;

	ctk_tree_model_get (CTK_TREE_MODEL (obj->priv->bookmarks_store), iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
			    &drive, -1);

	/* most common use case is a floppy drive, we'll poll for media and
	   go from there */
	async = async_data_new (obj);
	g_drive_poll_for_media (drive,
				async->cancellable,
				(GAsyncReadyCallback)poll_for_media_cb,
				async);

	g_object_unref (drive);
	set_busy (obj, TRUE);
}

static void
try_mount_volume (LapizFileBrowserWidget *widget,
		  GVolume 		 *volume)
{
	GMountOperation *operation;
	AsyncData *async;

	operation = ctk_mount_operation_new (CTK_WINDOW (ctk_widget_get_toplevel (CTK_WIDGET (widget))));
	async = async_data_new (widget);

	g_volume_mount (volume,
			G_MOUNT_MOUNT_NONE,
			operation,
			async->cancellable,
			(GAsyncReadyCallback)mount_volume_cb,
			async);

	g_object_unref (operation);
	set_busy (widget, TRUE);
}

static void
activate_volume (LapizFileBrowserWidget *obj,
		 CtkTreeIter	        *iter)
{
	GVolume *volume;

	ctk_tree_model_get (CTK_TREE_MODEL (obj->priv->bookmarks_store), iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
			    &volume, -1);

	/* see if we can mount the volume */
	try_mount_volume (obj, volume);
	g_object_unref (volume);
}

void
lapiz_file_browser_widget_refresh (LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model =
	    ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (LAPIZ_IS_FILE_BROWSER_STORE (model))
		lapiz_file_browser_store_refresh (LAPIZ_FILE_BROWSER_STORE
						  (model));
	else if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model)) {
		g_hash_table_ref (obj->priv->bookmarks_hash);
		g_hash_table_destroy (obj->priv->bookmarks_hash);

		lapiz_file_bookmarks_store_refresh
		    (LAPIZ_FILE_BOOKMARKS_STORE (model));
	}
}

void
lapiz_file_browser_widget_history_back (LapizFileBrowserWidget *obj)
{
	if (obj->priv->locations) {
		if (obj->priv->current_location)
			jump_to_location (obj,
					  obj->priv->current_location->
					  next, TRUE);
		else {
			jump_to_location (obj, obj->priv->locations, TRUE);
		}
	}
}

void
lapiz_file_browser_widget_history_forward (LapizFileBrowserWidget *obj)
{
	if (obj->priv->locations)
		jump_to_location (obj, obj->priv->current_location->prev,
				  FALSE);
}

static void
bookmark_open (LapizFileBrowserWidget *obj,
	       CtkTreeModel           *model,
	       CtkTreeIter            *iter)
{
	gchar *uri;
	gint flags;

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
			    &flags, -1);

	if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_DRIVE)
	{
		/* handle a drive node */
		lapiz_file_browser_store_cancel_mount_operation (obj->priv->file_store);
		activate_drive (obj, iter);
		return;
	}
	else if (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_VOLUME)
	{
		/* handle a volume node */
		lapiz_file_browser_store_cancel_mount_operation (obj->priv->file_store);
		activate_volume (obj, iter);
		return;
	}

	uri =
	    lapiz_file_bookmarks_store_get_uri
	    (LAPIZ_FILE_BOOKMARKS_STORE (model), iter);

	if (uri) {
		/* here we check if the bookmark is a mount point, or if it
		   is a remote bookmark. If that's the case, we will set the
		   root to the uri of the bookmark and not try to set the
		   topmost parent as root (since that may as well not be the
		   mount point anymore) */
		if ((flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_MOUNT) ||
		    (flags & LAPIZ_FILE_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK)) {
			lapiz_file_browser_widget_set_root (obj,
							    uri,
							    FALSE);
		} else {
			lapiz_file_browser_widget_set_root (obj,
							    uri,
							    TRUE);
		}
	} else {
		g_warning ("No uri!");
	}

	g_free (uri);
}

static void
file_open  (LapizFileBrowserWidget *obj,
	    CtkTreeModel           *model,
	    CtkTreeIter            *iter)
{
	gchar *uri;
	gint flags;

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI, &uri,
			    -1);

	if (!FILE_IS_DIR (flags) && !FILE_IS_DUMMY (flags)) {
		g_signal_emit (obj, signals[URI_ACTIVATED], 0, uri);
	}

	g_free (uri);
}

static gboolean
directory_open (LapizFileBrowserWidget *obj,
		CtkTreeModel           *model,
		CtkTreeIter            *iter)
{
	gboolean result = FALSE;
	GError *error = NULL;
	gchar *uri = NULL;
	LapizFileBrowserStoreFlag flags;

	ctk_tree_model_get (model, iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI, &uri,
			    -1);

	if (FILE_IS_DIR (flags)) {
		result = TRUE;

		if (!ctk_show_uri_on_window (NULL, uri, CDK_CURRENT_TIME, &error)) {
			g_signal_emit (obj, signals[ERROR], 0,
				       LAPIZ_FILE_BROWSER_ERROR_OPEN_DIRECTORY,
				       error->message);

			g_error_free (error);
			error = NULL;
		}
	}

	g_free (uri);

	return result;
}

static void
on_bookmark_activated (LapizFileBrowserView   *tree_view,
		       CtkTreeIter            *iter,
		       LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model = ctk_tree_view_get_model (CTK_TREE_VIEW (tree_view));

	bookmark_open (obj, model, iter);
}

static void
on_file_activated (LapizFileBrowserView   *tree_view,
		   CtkTreeIter            *iter,
		   LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model = ctk_tree_view_get_model (CTK_TREE_VIEW (tree_view));

	file_open (obj, model, iter);
}

static gboolean
virtual_root_is_root (LapizFileBrowserWidget *obj G_GNUC_UNUSED,
                      LapizFileBrowserStore  *model)
{
	CtkTreeIter root;
	CtkTreeIter virtual_root;

	if (!lapiz_file_browser_store_get_iter_root (model, &root))
		return TRUE;

	if (!lapiz_file_browser_store_get_iter_virtual_root (model, &virtual_root))
		return TRUE;

	return lapiz_file_browser_store_iter_equal (model, &root, &virtual_root);
}

static void
on_virtual_root_changed (LapizFileBrowserStore  *model,
			 GParamSpec             *param G_GNUC_UNUSED,
			 LapizFileBrowserWidget *obj)
{
	CtkTreeIter iter;
	gchar *uri;
	gchar *root_uri;
	CtkTreeIter root;
	CtkAction *action;
	Location *loc;
	GdkPixbuf *pixbuf;

	if (ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview)) !=
	    CTK_TREE_MODEL (obj->priv->file_store))
	{
		show_files_real (obj, FALSE);
	}

	if (lapiz_file_browser_store_get_iter_virtual_root (model, &iter)) {
		ctk_tree_model_get (CTK_TREE_MODEL (model), &iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
				    &uri, -1);

		if (lapiz_file_browser_store_get_iter_root (model, &root)) {
			if (!obj->priv->changing_location) {
				/* Remove all items from obj->priv->current_location on */
				if (obj->priv->current_location)
					clear_next_locations (obj);

				root_uri =
				    lapiz_file_browser_store_get_root
				    (model);

				loc = g_new (Location, 1);
				loc->root = g_file_new_for_uri (root_uri);
				loc->virtual_root = g_file_new_for_uri (uri);
				g_free (root_uri);

				if (obj->priv->current_location) {
					/* Add current location to the menu so we can go back
					   to it later */
					ctk_menu_shell_prepend
					    (CTK_MENU_SHELL
					     (obj->priv->
					      location_previous_menu),
					     obj->priv->
					     current_location_menu_item);
				}

				obj->priv->locations =
				    g_list_prepend (obj->priv->locations,
						    loc);

				ctk_tree_model_get (CTK_TREE_MODEL (model),
						    &iter,
						    LAPIZ_FILE_BROWSER_STORE_COLUMN_ICON,
						    &pixbuf, -1);

				obj->priv->current_location =
				    obj->priv->locations;
				obj->priv->current_location_menu_item =
				    create_goto_menu_item (obj,
							   obj->priv->
							   current_location,
							   pixbuf);

				g_object_ref_sink (obj->priv->
						   current_location_menu_item);

				if (pixbuf)
					g_object_unref (pixbuf);

			}

			action =
			    ctk_action_group_get_action (obj->priv->
			                                 action_group,
			                                 "DirectoryUp");
			ctk_action_set_sensitive (action,
			                          !virtual_root_is_root (obj, model));

			action =
			    ctk_action_group_get_action (obj->priv->
							 action_group_sensitive,
							 "DirectoryPrevious");
			ctk_action_set_sensitive (action,
						  obj->priv->
						  current_location != NULL
						  && obj->priv->
						  current_location->next !=
						  NULL);

			action =
			    ctk_action_group_get_action (obj->priv->
							 action_group_sensitive,
							 "DirectoryNext");
			ctk_action_set_sensitive (action,
						  obj->priv->
						  current_location != NULL
						  && obj->priv->
						  current_location->prev !=
						  NULL);
		}

		check_current_item (obj, TRUE);
		g_free (uri);
	} else {
		g_message ("NO!");
	}
}

static void
on_model_set (GObject                *gobject,
	      GParamSpec             *arg1 G_GNUC_UNUSED,
	      LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (gobject));

	clear_signals (obj);

	if (LAPIZ_IS_FILE_BOOKMARKS_STORE (model)) {
		clear_next_locations (obj);

		/* Add the current location to the back menu */
		if (obj->priv->current_location) {
			CtkAction *action;

			ctk_menu_shell_prepend (CTK_MENU_SHELL (obj->priv->location_previous_menu),
						obj->priv->current_location_menu_item);

			g_object_unref (obj->priv->current_location_menu_item);
			obj->priv->current_location = NULL;
			obj->priv->current_location_menu_item = NULL;

			action = ctk_action_group_get_action (obj->priv->action_group_sensitive,
							      "DirectoryPrevious");
			ctk_action_set_sensitive (action, TRUE);
		}

		ctk_widget_set_sensitive (obj->priv->filter_expander, FALSE);

		add_signal (obj, gobject,
			    g_signal_connect (gobject, "bookmark-activated",
					      G_CALLBACK
					      (on_bookmark_activated), obj));
	} else if (LAPIZ_IS_FILE_BROWSER_STORE (model)) {
		/* make sure any async operation is cancelled */
		cancel_async_operation (obj);

		add_signal (obj, gobject,
			    g_signal_connect (gobject, "file-activated",
					      G_CALLBACK
					      (on_file_activated), obj));

		add_signal (obj, model,
			    g_signal_connect (model, "no-trash",
			    		      G_CALLBACK
			    		      (on_file_store_no_trash), obj));

		ctk_widget_set_sensitive (obj->priv->filter_expander, TRUE);
	}

	update_sensitivity (obj);
}

static void
on_file_store_error (LapizFileBrowserStore  *store G_GNUC_UNUSED,
		     guint                   code,
		     gchar                  *message,
		     LapizFileBrowserWidget *obj)
{
	g_signal_emit (obj, signals[ERROR], 0, code, message);
}

static void
on_treeview_error (LapizFileBrowserView   *tree_view G_GNUC_UNUSED,
		   guint                   code,
		   gchar                  *message,
		   LapizFileBrowserWidget *obj)
{
	g_signal_emit (obj, signals[ERROR], 0, code, message);
}

static void
on_combo_changed (CtkComboBox * combo, LapizFileBrowserWidget * obj)
{
	CtkTreeIter iter;
	guint id;
	gchar * uri;
	GFile * file;

	if (!ctk_combo_box_get_active_iter (combo, &iter))
		return;

	ctk_tree_model_get (CTK_TREE_MODEL (obj->priv->combo_model), &iter,
			    COLUMN_ID, &id, -1);

	switch (id) {
	case BOOKMARKS_ID:
		lapiz_file_browser_widget_show_bookmarks (obj);
		break;

	case PATH_ID:
		ctk_tree_model_get (CTK_TREE_MODEL
				    (obj->priv->combo_model), &iter,
				    COLUMN_FILE, &file, -1);

		uri = g_file_get_uri (file);
		lapiz_file_browser_store_set_virtual_root_from_string
		    (obj->priv->file_store, uri);

		g_free (uri);
		g_object_unref (file);
		break;
	}
}

static gboolean
on_treeview_popup_menu (LapizFileBrowserView * treeview,
			LapizFileBrowserWidget * obj)
{
	return popup_menu (obj, NULL, ctk_tree_view_get_model (CTK_TREE_VIEW (treeview)));
}

static gboolean
on_treeview_button_press_event (LapizFileBrowserView * treeview,
				CdkEventButton * event,
				LapizFileBrowserWidget * obj)
{
	if (event->type == CDK_BUTTON_PRESS && event->button == 3) {
		return popup_menu (obj, event,
				   ctk_tree_view_get_model (CTK_TREE_VIEW (treeview)));
	}

	return FALSE;
}

static gboolean
do_change_directory (LapizFileBrowserWidget * obj,
                     CdkEventKey            * event)
{
	CtkAction * action = NULL;

	if ((event->state &
	    (~CDK_CONTROL_MASK & ~CDK_SHIFT_MASK & ~CDK_MOD1_MASK)) ==
	     event->state && event->keyval == CDK_KEY_BackSpace)
		action = ctk_action_group_get_action (obj->priv->
		                                      action_group_sensitive,
		                                      "DirectoryPrevious");
	else if (!((event->state & CDK_MOD1_MASK) &&
	    (event->state & (~CDK_CONTROL_MASK & ~CDK_SHIFT_MASK)) == event->state))
		return FALSE;

	switch (event->keyval) {
		case CDK_KEY_Left:
			action = ctk_action_group_get_action (obj->priv->
			                                      action_group_sensitive,
			                                      "DirectoryPrevious");
		break;
		case CDK_KEY_Right:
			action = ctk_action_group_get_action (obj->priv->
			                                      action_group_sensitive,
			                                      "DirectoryNext");
		break;
		case CDK_KEY_Up:
			action = ctk_action_group_get_action (obj->priv->
			                                      action_group,
			                                      "DirectoryUp");
		break;
		default:
		break;
	}

	if (action != NULL) {
		ctk_action_activate (action);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_treeview_key_press_event (LapizFileBrowserView * treeview,
			     CdkEventKey * event,
			     LapizFileBrowserWidget * obj)
{
	guint modifiers;

	if (do_change_directory (obj, event))
		return TRUE;

	if (!LAPIZ_IS_FILE_BROWSER_STORE
	    (ctk_tree_view_get_model (CTK_TREE_VIEW (treeview))))
		return FALSE;

	modifiers = ctk_accelerator_get_default_mod_mask ();

	if (event->keyval == CDK_KEY_Delete
	    || event->keyval == CDK_KEY_KP_Delete) {

		if ((event->state & modifiers) == CDK_SHIFT_MASK) {
			if (obj->priv->enable_delete) {
				delete_selected_files (obj, FALSE);
				return TRUE;
			}
		} else if ((event->state & modifiers) == 0) {
			delete_selected_files (obj, TRUE);
			return TRUE;
		}
	}

	if ((event->keyval == CDK_KEY_F2)
	    && (event->state & modifiers) == 0) {
		rename_selected_file (obj);

		return TRUE;
	}

	return FALSE;
}

static void
on_selection_changed (CtkTreeSelection       *selection G_GNUC_UNUSED,
		      LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	guint selected = 0;
	guint files = 0;
	guint dirs = 0;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (LAPIZ_IS_FILE_BROWSER_STORE (model))
	{
		selected = lapiz_file_browser_widget_get_num_selected_files_or_directories (obj,
											    &files,
											    &dirs);
	}

	ctk_action_group_set_sensitive (obj->priv->action_group_selection,
					selected > 0);
	ctk_action_group_set_sensitive (obj->priv->action_group_file_selection,
					(selected > 0) && (selected == files));
	ctk_action_group_set_sensitive (obj->priv->action_group_single_selection,
					selected == 1);
	ctk_action_group_set_sensitive (obj->priv->action_group_single_most_selection,
					selected <= 1);
}

static gboolean
on_entry_filter_activate (LapizFileBrowserWidget * obj)
{
	gchar const *text;

	text = ctk_entry_get_text (CTK_ENTRY (obj->priv->filter_entry));
	set_filter_pattern_real (obj, text, FALSE);

	return FALSE;
}

static void
on_location_jump_activate (CtkMenuItem * item,
			   LapizFileBrowserWidget * obj)
{
	GList *location;

	location = g_object_get_data (G_OBJECT (item), LOCATION_DATA_KEY);

	if (obj->priv->current_location) {
		jump_to_location (obj, location,
				  g_list_position (obj->priv->locations,
						   location) >
				  g_list_position (obj->priv->locations,
						   obj->priv->
						   current_location));
	} else {
		jump_to_location (obj, location, TRUE);
	}

}

static void
on_bookmarks_row_changed (CtkTreeModel           *model G_GNUC_UNUSED,
			  CtkTreePath            *path G_GNUC_UNUSED,
			  CtkTreeIter            *iter,
			  LapizFileBrowserWidget *obj)
{
	add_bookmark_hash (obj, iter);
}

static void
on_bookmarks_row_deleted (CtkTreeModel * model,
                          CtkTreePath * path,
                          LapizFileBrowserWidget *obj)
{
	CtkTreeIter iter;
	gchar * uri;
	GFile * file;

	if (!ctk_tree_model_get_iter (model, &iter, path))
		return;

	uri = lapiz_file_bookmarks_store_get_uri (obj->priv->bookmarks_store, &iter);

	if (!uri)
		return;

	file = g_file_new_for_uri (uri);
	g_hash_table_remove (obj->priv->bookmarks_hash, file);

	g_object_unref (file);
	g_free (uri);
}

static void
on_filter_mode_changed (LapizFileBrowserStore  *model,
                        GParamSpec             *param G_GNUC_UNUSED,
                        LapizFileBrowserWidget *obj)
{
	gint mode;
	CtkToggleAction * action;
	gboolean active;

	mode = lapiz_file_browser_store_get_filter_mode (model);

	action = CTK_TOGGLE_ACTION (ctk_action_group_get_action (obj->priv->action_group,
	                                                         "FilterHidden"));
	active = !(mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN);

	if (active != ctk_toggle_action_get_active (action))
		ctk_toggle_action_set_active (action, active);

	action = CTK_TOGGLE_ACTION (ctk_action_group_get_action (obj->priv->action_group,
	                                                         "FilterBinary"));
	active = !(mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY);

	if (active != ctk_toggle_action_get_active (action))
		ctk_toggle_action_set_active (action, active);
}

static void
on_action_directory_next (CtkAction              *action G_GNUC_UNUSED,
			  LapizFileBrowserWidget *obj)
{
	lapiz_file_browser_widget_history_forward (obj);
}

static void
on_action_directory_previous (CtkAction              *action G_GNUC_UNUSED,
			      LapizFileBrowserWidget *obj)
{
	lapiz_file_browser_widget_history_back (obj);
}

static void
on_action_directory_up (CtkAction              *action G_GNUC_UNUSED,
			LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	lapiz_file_browser_store_set_virtual_root_up (LAPIZ_FILE_BROWSER_STORE (model));
}

static void
on_action_directory_new (CtkAction              *action G_GNUC_UNUSED,
			 LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	CtkTreeIter parent;
	CtkTreeIter iter;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	if (!lapiz_file_browser_widget_get_selected_directory (obj, &parent))
		return;

	if (lapiz_file_browser_store_new_directory
	    (LAPIZ_FILE_BROWSER_STORE (model), &parent, &iter)) {
		lapiz_file_browser_view_start_rename (obj->priv->treeview,
						      &iter);
	}
}

static void
on_action_file_open (CtkAction              *action G_GNUC_UNUSED,
		     LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	GList *rows;
	GList *row;
	CtkTreeIter iter;
	CtkTreePath *path;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	rows = ctk_tree_selection_get_selected_rows (selection, &model);

	for (row = rows; row; row = row->next) {
		path = (CtkTreePath *)(row->data);

		if (ctk_tree_model_get_iter (model, &iter, path))
			file_open (obj, model, &iter);

		ctk_tree_path_free (path);
	}

	g_list_free (rows);
}

static void
on_action_file_new (CtkAction              *action G_GNUC_UNUSED,
		    LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	CtkTreeIter parent;
	CtkTreeIter iter;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	if (!lapiz_file_browser_widget_get_selected_directory (obj, &parent))
		return;

	if (lapiz_file_browser_store_new_file
	    (LAPIZ_FILE_BROWSER_STORE (model), &parent, &iter)) {
		lapiz_file_browser_view_start_rename (obj->priv->treeview,
						      &iter);
	}
}

static void
on_action_file_rename (CtkAction              *action G_GNUC_UNUSED,
		       LapizFileBrowserWidget *obj)
{
	rename_selected_file (obj);
}

static void
on_action_file_delete (CtkAction              *action G_GNUC_UNUSED,
		       LapizFileBrowserWidget *obj)
{
	delete_selected_files (obj, FALSE);
}

static void
on_action_file_move_to_trash (CtkAction              *action G_GNUC_UNUSED,
			      LapizFileBrowserWidget *obj)
{
	delete_selected_files (obj, TRUE);
}

static void
on_action_directory_refresh (CtkAction              *action G_GNUC_UNUSED,
			     LapizFileBrowserWidget *obj)
{
	lapiz_file_browser_widget_refresh (obj);
}

static void
on_action_directory_open (CtkAction              *action G_GNUC_UNUSED,
			  LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	GList *rows;
	GList *row;
	gboolean directory_opened = FALSE;
	CtkTreeIter iter;
	CtkTreePath *path;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	rows = ctk_tree_selection_get_selected_rows (selection, &model);

	for (row = rows; row; row = row->next) {
		path = (CtkTreePath *)(row->data);

		if (ctk_tree_model_get_iter (model, &iter, path))
			directory_opened |= directory_open (obj, model, &iter);

		ctk_tree_path_free (path);
	}

	if (!directory_opened) {
		if (lapiz_file_browser_widget_get_selected_directory (obj, &iter))
			directory_open (obj, model, &iter);
	}

	g_list_free (rows);
}

static void
on_action_filter_hidden (CtkAction * action, LapizFileBrowserWidget * obj)
{
	update_filter_mode (obj,
	                    action,
	                    LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN);
}

static void
on_action_filter_binary (CtkAction * action, LapizFileBrowserWidget * obj)
{
	update_filter_mode (obj,
	                    action,
	                    LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY);
}

static void
on_action_bookmark_open (CtkAction              *action G_GNUC_UNUSED,
			 LapizFileBrowserWidget *obj)
{
	CtkTreeModel *model;
	CtkTreeSelection *selection;
	CtkTreeIter iter;

	model = ctk_tree_view_get_model (CTK_TREE_VIEW (obj->priv->treeview));
	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (obj->priv->treeview));

	if (!LAPIZ_IS_FILE_BOOKMARKS_STORE (model))
		return;

	if (ctk_tree_selection_get_selected (selection, NULL, &iter))
		bookmark_open (obj, model, &iter);
}

void
_lapiz_file_browser_widget_register_type (GTypeModule *type_module)
{
	lapiz_file_browser_widget_register_type (type_module);
}

// ex:ts=8:noet:
