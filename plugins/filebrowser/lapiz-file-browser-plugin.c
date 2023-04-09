/*
 * lapiz-file-browser-plugin.c - Lapiz plugin providing easy file access
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

#include <lapiz/lapiz-commands.h>
#include <lapiz/lapiz-utils.h>
#include <lapiz/lapiz-app.h>
#include <glib/gi18n-lib.h>
#include <lapiz/lapiz-window.h>
#include <lapiz/lapiz-debug.h>
#include <gio/gio.h>
#include <string.h>
#include <libpeas/peas-activatable.h>

#include "lapiz-file-browser-enum-types.h"
#include "lapiz-file-browser-plugin.h"
#include "lapiz-file-browser-utils.h"
#include "lapiz-file-browser-error.h"
#include "lapiz-file-browser-widget.h"
#include "lapiz-file-browser-messages.h"

#define FILE_BROWSER_SCHEMA 		"org.mate.lapiz.plugins.filebrowser"
#define FILE_BROWSER_ONLOAD_SCHEMA 	"org.mate.lapiz.plugins.filebrowser.on-load"
#define CAJA_SCHEMA					"org.mate.caja.preferences"
#define CAJA_CLICK_POLICY_KEY		"click-policy"
#define CAJA_ENABLE_DELETE_KEY		"enable-delete"
#define CAJA_CONFIRM_TRASH_KEY		"confirm-trash"
#define TERMINAL_SCHEMA				"org.mate.applications-terminal"
#define TERMINAL_EXEC_KEY			"exec"

struct _LapizFileBrowserPluginPrivate
{
	GtkWidget               *window;

	LapizFileBrowserWidget * tree_widget;
	gulong                   merge_id;
	GtkActionGroup         * action_group;
	GtkActionGroup	       * single_selection_action_group;
	gboolean	         auto_root;
	gulong                   end_loading_handle;
	gboolean		 confirm_trash;

	GSettings *settings;
	GSettings *onload_settings;
	GSettings *caja_settings;
	GSettings *terminal_settings;
};

enum {
	PROP_0,
	PROP_OBJECT
};

static void on_uri_activated_cb          (LapizFileBrowserWidget * widget,
                                          gchar const *uri,
                                          LapizWindow * window);
static void on_error_cb                  (LapizFileBrowserWidget * widget,
                                          guint code,
                                          gchar const *message,
                                          LapizFileBrowserPluginPrivate * data);
static void on_model_set_cb              (LapizFileBrowserView * widget,
                                          GParamSpec *arg1,
                                          LapizFileBrowserPluginPrivate * data);
static void on_virtual_root_changed_cb   (LapizFileBrowserStore * model,
                                          GParamSpec * param,
                                          LapizFileBrowserPluginPrivate * data);
static void on_filter_mode_changed_cb    (LapizFileBrowserStore * model,
                                          GParamSpec * param,
                                          LapizFileBrowserPluginPrivate * data);
static void on_rename_cb		 (LapizFileBrowserStore * model,
					  const gchar * olduri,
					  const gchar * newuri,
					  LapizWindow * window);
static void on_filter_pattern_changed_cb (LapizFileBrowserWidget * widget,
                                          GParamSpec * param,
                                          LapizFileBrowserPluginPrivate * data);
static void on_tab_added_cb              (LapizWindow * window,
                                          LapizTab * tab,
                                          LapizFileBrowserPluginPrivate * data);
static gboolean on_confirm_delete_cb     (LapizFileBrowserWidget * widget,
                                          LapizFileBrowserStore * store,
                                          GList * rows,
                                          LapizFileBrowserPluginPrivate * data);
static gboolean on_confirm_no_trash_cb   (LapizFileBrowserWidget * widget,
                                          GList * files,
                                          LapizWindow * window);

static void peas_activatable_iface_init (PeasActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (LapizFileBrowserPlugin,
                                lapiz_file_browser_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (LapizFileBrowserPlugin)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               peas_activatable_iface_init)    \
                                                                                               \
                                lapiz_file_browser_enum_and_flag_register_type (type_module);  \
                                _lapiz_file_browser_store_register_type        (type_module);  \
                                _lapiz_file_bookmarks_store_register_type      (type_module);  \
                                _lapiz_file_browser_view_register_type         (type_module);  \
                                _lapiz_file_browser_widget_register_type       (type_module);  \
)

static void
lapiz_file_browser_plugin_init (LapizFileBrowserPlugin * plugin)
{
	plugin->priv = lapiz_file_browser_plugin_get_instance_private (plugin);
}

static void
lapiz_file_browser_plugin_dispose (GObject * object)
{
	LapizFileBrowserPlugin *plugin = LAPIZ_FILE_BROWSER_PLUGIN (object);

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	G_OBJECT_CLASS (lapiz_file_browser_plugin_parent_class)->dispose (object);
}

static void
lapiz_file_browser_plugin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	LapizFileBrowserPlugin *plugin = LAPIZ_FILE_BROWSER_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			plugin->priv->window = GTK_WIDGET (g_value_dup_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
lapiz_file_browser_plugin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	LapizFileBrowserPlugin *plugin = LAPIZ_FILE_BROWSER_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			g_value_set_object (value, plugin->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
on_end_loading_cb (LapizFileBrowserStore      * store,
                   GtkTreeIter                * iter,
                   LapizFileBrowserPluginPrivate * data)
{
	/* Disconnect the signal */
	g_signal_handler_disconnect (store, data->end_loading_handle);
	data->end_loading_handle = 0;
	data->auto_root = FALSE;
}

static void
prepare_auto_root (LapizFileBrowserPluginPrivate *data)
{
	LapizFileBrowserStore *store;

	data->auto_root = TRUE;

	store = lapiz_file_browser_widget_get_browser_store (data->tree_widget);

	if (data->end_loading_handle != 0) {
		g_signal_handler_disconnect (store, data->end_loading_handle);
		data->end_loading_handle = 0;
	}

	data->end_loading_handle = g_signal_connect (store,
	                                             "end-loading",
	                                             G_CALLBACK (on_end_loading_cb),
	                                             data);
}

static void
restore_default_location (LapizFileBrowserPluginPrivate *data)
{
	gchar * root;
	gchar * virtual_root;
	gboolean bookmarks;
	gboolean remote;

	bookmarks = !g_settings_get_boolean (data->onload_settings, "tree-view");

	if (bookmarks) {
		lapiz_file_browser_widget_show_bookmarks (data->tree_widget);
		return;
	}

	root = g_settings_get_string (data->onload_settings, "root");
	virtual_root = g_settings_get_string (data->onload_settings, "virtual-root");

	remote = g_settings_get_boolean (data->onload_settings, "enable-remote");

	if (root != NULL && *root != '\0') {
		GFile *file;

		file = g_file_new_for_uri (root);

		if (remote || g_file_is_native (file)) {
			if (virtual_root != NULL && *virtual_root != '\0') {
				prepare_auto_root (data);
				lapiz_file_browser_widget_set_root_and_virtual_root (data->tree_widget,
					                                             root,
					                                             virtual_root);
			} else {
				prepare_auto_root (data);
				lapiz_file_browser_widget_set_root (data->tree_widget,
					                            root,
					                            TRUE);
			}
		}

		g_object_unref (file);
	}

	g_free (root);
	g_free (virtual_root);
}

static void
restore_filter (LapizFileBrowserPluginPrivate *data)
{
	gchar *filter_mode;
	LapizFileBrowserStoreFilterMode mode;
	gchar *pattern;

	/* Get filter_mode */
	filter_mode = g_settings_get_string (data->settings, "filter-mode");

	/* Filter mode */
	mode = lapiz_file_browser_store_filter_mode_get_default ();

	if (filter_mode != NULL) {
		if (strcmp (filter_mode, "hidden") == 0) {
			mode = LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN;
		} else if (strcmp (filter_mode, "binary") == 0) {
			mode = LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY;
		} else if (strcmp (filter_mode, "hidden_and_binary") == 0 ||
		         strcmp (filter_mode, "binary_and_hidden") == 0) {
			mode = LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN |
			       LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY;
		} else if (strcmp (filter_mode, "none") == 0 ||
		           *filter_mode == '\0') {
			mode = LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_NONE;
		}
	}

	/* Set the filter mode */
	lapiz_file_browser_store_set_filter_mode (
	    lapiz_file_browser_widget_get_browser_store (data->tree_widget),
	    mode);

	pattern = g_settings_get_string (data->settings, "filter-pattern");

	lapiz_file_browser_widget_set_filter_pattern (data->tree_widget,
	                                              pattern);

	g_free (filter_mode);
	g_free (pattern);
}

static LapizFileBrowserViewClickPolicy
click_policy_from_string (gchar const *click_policy)
{
	if (click_policy && strcmp (click_policy, "single") == 0)
		return LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE;
	else
		return LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE;
}

static void
on_click_policy_changed (GSettings *settings,
			 gchar *key,
			 gpointer user_data)
{
	LapizFileBrowserPluginPrivate * data;
	gchar *click_policy;
	LapizFileBrowserViewClickPolicy policy = LAPIZ_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE;
	LapizFileBrowserView *view;

	data = (LapizFileBrowserPluginPrivate *)(user_data);

	click_policy = g_settings_get_string (settings, key);
	policy = click_policy_from_string (click_policy);

	view = lapiz_file_browser_widget_get_browser_view (data->tree_widget);
	lapiz_file_browser_view_set_click_policy (view, policy);
	g_free (click_policy);
}

static void
on_enable_delete_changed (GSettings *settings,
			  gchar *key,
			  gpointer user_data)
{
	LapizFileBrowserPluginPrivate *data;
	gboolean enable = FALSE;

	data = (LapizFileBrowserPluginPrivate *)(user_data);
	enable = g_settings_get_boolean (settings, key);

	g_object_set (G_OBJECT (data->tree_widget), "enable-delete", enable, NULL);
}

static void
on_confirm_trash_changed (GSettings *settings,
		 	  gchar *key,
			  gpointer user_data)
{
	LapizFileBrowserPluginPrivate *data;
	gboolean enable = FALSE;

	data = (LapizFileBrowserPluginPrivate *)(user_data);
	enable = g_settings_get_boolean (settings, key);

	data->confirm_trash = enable;
}

static gboolean
have_click_policy (void)
{
	GSettings *settings = g_settings_new (CAJA_SCHEMA);
	gchar *pref = g_settings_get_string (settings, CAJA_CLICK_POLICY_KEY);
	gboolean result = (pref != NULL);

	g_free (pref);
	g_object_unref (settings);
	return result;
}

static void
install_caja_prefs (LapizFileBrowserPluginPrivate *data)
{
	gchar *pref;
	gboolean prefb;
	LapizFileBrowserViewClickPolicy policy;
	LapizFileBrowserView *view;

	if (have_click_policy ()) {
		g_signal_connect (data->caja_settings,
		                  "changed::" CAJA_CLICK_POLICY_KEY,
		                  G_CALLBACK (on_click_policy_changed),
		                  data);
	}

	g_signal_connect (data->caja_settings,
	                  "changed::" CAJA_ENABLE_DELETE_KEY,
	                  G_CALLBACK (on_enable_delete_changed),
	                  data);

	g_signal_connect (data->caja_settings,
	                  "changed::" CAJA_CONFIRM_TRASH_KEY,
	                  G_CALLBACK (on_confirm_trash_changed),
	                  data);

	pref = g_settings_get_string (data->caja_settings, CAJA_CLICK_POLICY_KEY);
	policy = click_policy_from_string (pref);
	g_free (pref);

	view = lapiz_file_browser_widget_get_browser_view (data->tree_widget);
	lapiz_file_browser_view_set_click_policy (view, policy);

	prefb = g_settings_get_boolean (data->caja_settings, CAJA_ENABLE_DELETE_KEY);
	g_object_set (G_OBJECT (data->tree_widget), "enable-delete", prefb, NULL);

	prefb = g_settings_get_boolean (data->caja_settings, CAJA_CONFIRM_TRASH_KEY);
	data->confirm_trash = prefb;
}

static void
set_root_from_doc (LapizFileBrowserPluginPrivate * data,
                   LapizDocument * doc)
{
	GFile *file;
	GFile *parent;

	if (doc == NULL)
		return;

	file = lapiz_document_get_location (doc);
	if (file == NULL)
		return;

	parent = g_file_get_parent (file);

	if (parent != NULL) {
		gchar * root;

		root = g_file_get_uri (parent);

		lapiz_file_browser_widget_set_root (data->tree_widget,
				                    root,
				                    TRUE);

		g_object_unref (parent);
		g_free (root);
	}

	g_object_unref (file);
}

static void
on_action_set_active_root (GtkAction * action,
                           LapizFileBrowserPluginPrivate * data)
{
	set_root_from_doc (data,
	                   lapiz_window_get_active_document (LAPIZ_WINDOW (data->window)));
}

static gchar *
get_terminal (LapizFileBrowserPluginPrivate * data)
{
	gchar * terminal;

	terminal = g_settings_get_string (data->terminal_settings,
					    TERMINAL_EXEC_KEY);

	if (terminal == NULL) {
		const gchar *term = g_getenv ("TERM");

		if (term != NULL)
			terminal = g_strdup (term);
		else
			terminal = g_strdup ("xterm");
	}

	return terminal;
}

static void
on_action_open_terminal (GtkAction * action,
                         LapizFileBrowserPluginPrivate * data)
{
	gchar * terminal;
	gchar * wd = NULL;
	gchar * local;
	gchar * argv[2];
	GFile * file;

	GtkTreeIter iter;
	LapizFileBrowserStore * store;

	/* Get the current directory */
	if (!lapiz_file_browser_widget_get_selected_directory (data->tree_widget, &iter))
		return;

	store = lapiz_file_browser_widget_get_browser_store (data->tree_widget);
	gtk_tree_model_get (GTK_TREE_MODEL (store),
	                    &iter,
	                    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
	                    &wd,
	                    -1);

	if (wd == NULL)
		return;

	terminal = get_terminal (data);

	file = g_file_new_for_uri (wd);
	local = g_file_get_path (file);
	g_object_unref (file);

	argv[0] = terminal;
	argv[1] = NULL;

	g_spawn_async (local,
	               argv,
	               NULL,
	               G_SPAWN_SEARCH_PATH,
	               NULL,
	               NULL,
	               NULL,
	               NULL);

	g_free (terminal);
	g_free (wd);
	g_free (local);
}

static void
on_selection_changed_cb (GtkTreeSelection *selection,
			 LapizFileBrowserPluginPrivate *data)
{
	GtkTreeView * tree_view;
	GtkTreeModel * model;
	GtkTreeIter iter;
	gboolean sensitive;
	gchar * uri;

	tree_view = GTK_TREE_VIEW (lapiz_file_browser_widget_get_browser_view (data->tree_widget));
	model = gtk_tree_view_get_model (tree_view);

	if (!LAPIZ_IS_FILE_BROWSER_STORE (model))
		return;

	sensitive = lapiz_file_browser_widget_get_selected_directory (data->tree_widget, &iter);

	if (sensitive) {
		gtk_tree_model_get (model, &iter,
				    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI,
				    &uri, -1);

		sensitive = lapiz_utils_uri_has_file_scheme (uri);
		g_free (uri);
	}

	gtk_action_set_sensitive (
		gtk_action_group_get_action (data->single_selection_action_group,
                                            "OpenTerminal"),
		sensitive);
}

#define POPUP_UI ""                             \
"<ui>"                                          \
"  <popup name=\"FilePopup\">"                  \
"    <placeholder name=\"FilePopup_Opt1\">"     \
"      <menuitem action=\"SetActiveRoot\"/>"    \
"    </placeholder>"                            \
"    <placeholder name=\"FilePopup_Opt4\">"     \
"      <menuitem action=\"OpenTerminal\"/>"     \
"    </placeholder>"                            \
"  </popup>"                                    \
"  <popup name=\"BookmarkPopup\">"              \
"    <placeholder name=\"BookmarkPopup_Opt1\">" \
"      <menuitem action=\"SetActiveRoot\"/>"    \
"    </placeholder>"                            \
"  </popup>"                                    \
"</ui>"

static GtkActionEntry extra_actions[] =
{
	{"SetActiveRoot", "go-jump", N_("_Set root to active document"),
	 NULL,
	 N_("Set the root to the active document location"),
	 G_CALLBACK (on_action_set_active_root)}
};

static GtkActionEntry extra_single_selection_actions[] = {
	{"OpenTerminal", "utilities-terminal", N_("_Open terminal here"),
	 NULL,
	 N_("Open a terminal at the currently opened directory"),
	 G_CALLBACK (on_action_open_terminal)}
};

static void
add_popup_ui (LapizFileBrowserPluginPrivate *data)
{
	GtkUIManager * manager;
	GtkActionGroup * action_group;
	GError * error = NULL;

	manager = lapiz_file_browser_widget_get_ui_manager (data->tree_widget);

	action_group = gtk_action_group_new ("FileBrowserPluginExtra");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      extra_actions,
				      G_N_ELEMENTS (extra_actions),
				      data);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	data->action_group = action_group;

	action_group = gtk_action_group_new ("FileBrowserPluginSingleSelectionExtra");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      extra_single_selection_actions,
				      G_N_ELEMENTS (extra_single_selection_actions),
				      data);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	data->single_selection_action_group = action_group;

	data->merge_id = gtk_ui_manager_add_ui_from_string (manager,
	                                                    POPUP_UI,
	                                                    -1,
	                                                    &error);

	if (data->merge_id == 0) {
		g_warning("Unable to merge UI: %s", error->message);
		g_error_free(error);
	}
}

static void
remove_popup_ui (LapizFileBrowserPluginPrivate *data)
{
	GtkUIManager * manager;

	manager = lapiz_file_browser_widget_get_ui_manager (data->tree_widget);
	gtk_ui_manager_remove_ui (manager, data->merge_id);

	gtk_ui_manager_remove_action_group (manager, data->action_group);
	g_object_unref (data->action_group);

	gtk_ui_manager_remove_action_group (manager, data->single_selection_action_group);
	g_object_unref (data->single_selection_action_group);
}

static void
lapiz_file_browser_plugin_update_state (PeasActivatable *activatable)
{
	LapizFileBrowserPluginPrivate *data;
	LapizDocument * doc;

	data = LAPIZ_FILE_BROWSER_PLUGIN (activatable)->priv;

	doc = lapiz_window_get_active_document (LAPIZ_WINDOW (data->window));

	gtk_action_set_sensitive (gtk_action_group_get_action (data->action_group,
	                                                       "SetActiveRoot"),
	                          doc != NULL &&
	                          !lapiz_document_is_untitled (doc));
}

static void
lapiz_file_browser_plugin_activate (PeasActivatable *activatable)
{
	LapizFileBrowserPluginPrivate *data;
	LapizWindow *window;
	LapizPanel * panel;
	GtkWidget * image;
	GdkPixbuf * pixbuf;
	LapizFileBrowserStore * store;
	gchar *data_dir;
	GSettingsSchemaSource *schema_source;
	GSettingsSchema *schema;

	data = LAPIZ_FILE_BROWSER_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (activatable));
	data->tree_widget = LAPIZ_FILE_BROWSER_WIDGET (lapiz_file_browser_widget_new (data_dir));
	g_free (data_dir);

	data->settings = g_settings_new (FILE_BROWSER_SCHEMA);
	data->onload_settings = g_settings_new (FILE_BROWSER_ONLOAD_SCHEMA);
	data->terminal_settings = g_settings_new (TERMINAL_SCHEMA);

	g_signal_connect (data->tree_widget,
			  "uri-activated",
			  G_CALLBACK (on_uri_activated_cb), window);

	g_signal_connect (data->tree_widget,
			  "error", G_CALLBACK (on_error_cb), data);

	g_signal_connect (data->tree_widget,
	                  "notify::filter-pattern",
	                  G_CALLBACK (on_filter_pattern_changed_cb),
	                  data);

	g_signal_connect (data->tree_widget,
	                  "confirm-delete",
	                  G_CALLBACK (on_confirm_delete_cb),
	                  data);

	g_signal_connect (data->tree_widget,
	                  "confirm-no-trash",
	                  G_CALLBACK (on_confirm_no_trash_cb),
	                  window);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW
			  (lapiz_file_browser_widget_get_browser_view
			  (data->tree_widget))),
			  "changed",
			  G_CALLBACK (on_selection_changed_cb),
			  data);

	panel = lapiz_window_get_side_panel (window);
	pixbuf = lapiz_file_browser_utils_pixbuf_from_theme("system-file-manager",
	                                                    GTK_ICON_SIZE_MENU);

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(pixbuf);
	} else {
		image = gtk_image_new_from_icon_name("gtk-index", GTK_ICON_SIZE_MENU);
	}

	gtk_widget_show(image);
	lapiz_panel_add_item (panel,
	                      GTK_WIDGET (data->tree_widget),
	                      _("File Browser"),
	                      image);
	gtk_widget_show (GTK_WIDGET (data->tree_widget));

	add_popup_ui (data);

	/* Restore filter options */
	restore_filter (data);

	/* Install caja preferences */
	schema_source = g_settings_schema_source_get_default();
	schema = g_settings_schema_source_lookup (schema_source, CAJA_SCHEMA, FALSE);
	if (schema != NULL) {
		data->caja_settings = g_settings_new (CAJA_SCHEMA);
		install_caja_prefs (data);
		g_settings_schema_unref (schema);
	}

	/* Connect signals to store the last visited location */
	g_signal_connect (lapiz_file_browser_widget_get_browser_view (data->tree_widget),
	                  "notify::model",
	                  G_CALLBACK (on_model_set_cb),
	                  data);

	store = lapiz_file_browser_widget_get_browser_store (data->tree_widget);
	g_signal_connect (store,
	                  "notify::virtual-root",
	                  G_CALLBACK (on_virtual_root_changed_cb),
	                  data);

	g_signal_connect (store,
	                  "notify::filter-mode",
	                  G_CALLBACK (on_filter_mode_changed_cb),
	                  data);

	g_signal_connect (store,
			  "rename",
			  G_CALLBACK (on_rename_cb),
			  window);

	g_signal_connect (window,
	                  "tab-added",
	                  G_CALLBACK (on_tab_added_cb),
	                  data);

	/* Register messages on the bus */
	lapiz_file_browser_messages_register (window, data->tree_widget);

	lapiz_file_browser_plugin_update_state (activatable);
}

static void
lapiz_file_browser_plugin_deactivate (PeasActivatable *activatable)
{
	LapizFileBrowserPluginPrivate *data;
	LapizWindow *window;
	LapizPanel * panel;

	data = LAPIZ_FILE_BROWSER_PLUGIN (activatable)->priv;
	window = LAPIZ_WINDOW (data->window);

	/* Unregister messages from the bus */
	lapiz_file_browser_messages_unregister (window);

	/* Disconnect signals */
	g_signal_handlers_disconnect_by_func (window,
	                                      G_CALLBACK (on_tab_added_cb),
	                                      data);

	g_object_unref (data->settings);
	g_object_unref (data->onload_settings);
	g_object_unref (data->terminal_settings);

	if (data->caja_settings)
		g_object_unref (data->caja_settings);

	remove_popup_ui (data);

	panel = lapiz_window_get_side_panel (window);
	lapiz_panel_remove_item (panel, GTK_WIDGET (data->tree_widget));
}

static void
lapiz_file_browser_plugin_class_init (LapizFileBrowserPluginClass * klass)
{
	GObjectClass  *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = lapiz_file_browser_plugin_dispose;
	object_class->set_property = lapiz_file_browser_plugin_set_property;
	object_class->get_property = lapiz_file_browser_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
lapiz_file_browser_plugin_class_finalize (LapizFileBrowserPluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
peas_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = lapiz_file_browser_plugin_activate;
	iface->deactivate = lapiz_file_browser_plugin_deactivate;
	iface->update_state = lapiz_file_browser_plugin_update_state;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	lapiz_file_browser_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            LAPIZ_TYPE_FILE_BROWSER_PLUGIN);
}

/* Callbacks */
static void
on_uri_activated_cb (LapizFileBrowserWidget * tree_widget,
		     gchar const *uri, LapizWindow * window)
{
	lapiz_commands_load_uri (window, uri, NULL, 0);
}

static void
on_error_cb (LapizFileBrowserWidget * tree_widget,
	     guint code, gchar const *message, LapizFileBrowserPluginPrivate * data)
{
	gchar * title;
	GtkWidget * dlg;

	/* Do not show the error when the root has been set automatically */
	if (data->auto_root && (code == LAPIZ_FILE_BROWSER_ERROR_SET_ROOT ||
	                        code == LAPIZ_FILE_BROWSER_ERROR_LOAD_DIRECTORY))
	{
		/* Show bookmarks */
		lapiz_file_browser_widget_show_bookmarks (data->tree_widget);
		return;
	}

	switch (code) {
	case LAPIZ_FILE_BROWSER_ERROR_NEW_DIRECTORY:
		title =
		    _("An error occurred while creating a new directory");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_NEW_FILE:
		title = _("An error occurred while creating a new file");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_RENAME:
		title =
		    _
		    ("An error occurred while renaming a file or directory");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_DELETE:
		title =
		    _
		    ("An error occurred while deleting a file or directory");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_OPEN_DIRECTORY:
		title =
		    _
		    ("An error occurred while opening a directory in the file manager");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_SET_ROOT:
		title =
		    _("An error occurred while setting a root directory");
		break;
	case LAPIZ_FILE_BROWSER_ERROR_LOAD_DIRECTORY:
		title =
		    _("An error occurred while loading a directory");
		break;
	default:
		title = _("An error occurred");
		break;
	}

	dlg = gtk_message_dialog_new (GTK_WINDOW (data->window),
				      GTK_DIALOG_MODAL |
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				      "%s", title);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
						  "%s", message);

	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

static void
on_model_set_cb (LapizFileBrowserView * widget,
                 GParamSpec *arg1,
                 LapizFileBrowserPluginPrivate * data)
{
	GtkTreeModel * model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (lapiz_file_browser_widget_get_browser_view (data->tree_widget)));

	if (model == NULL)
		return;

	g_settings_set_boolean (data->onload_settings,
	                       "tree-view",
	                       LAPIZ_IS_FILE_BROWSER_STORE (model));
}

static void
on_filter_mode_changed_cb (LapizFileBrowserStore * model,
                           GParamSpec * param,
                           LapizFileBrowserPluginPrivate * data)
{
	LapizFileBrowserStoreFilterMode mode;

	mode = lapiz_file_browser_store_get_filter_mode (model);

	if ((mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN) &&
	    (mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY)) {
		g_settings_set_string (data->settings, "filter-mode", "hidden_and_binary");
	} else if (mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN) {
		g_settings_set_string (data->settings, "filter-mode", "hidden");
	} else if (mode & LAPIZ_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY) {
		g_settings_set_string (data->settings, "filter-mode", "binary");
	} else {
		g_settings_set_string (data->settings, "filter-mode", "none");
	}
}

static void
on_rename_cb (LapizFileBrowserStore * store,
	      const gchar * olduri,
	      const gchar * newuri,
	      LapizWindow * window)
{
	LapizApp * app;
	GList * documents;
	GList * item;
	LapizDocument * doc;
	GFile * docfile;
	GFile * oldfile;
	GFile * newfile;
	gchar * uri;

	/* Find all documents and set its uri to newuri where it matches olduri */
	app = lapiz_app_get_default ();
	documents = lapiz_app_get_documents (app);

	oldfile = g_file_new_for_uri (olduri);
	newfile = g_file_new_for_uri (newuri);

	for (item = documents; item; item = item->next) {
		doc = LAPIZ_DOCUMENT (item->data);
		uri = lapiz_document_get_uri (doc);

		if (!uri)
			continue;

		docfile = g_file_new_for_uri (uri);

		if (g_file_equal (docfile, oldfile)) {
			lapiz_document_set_uri (doc, newuri);
		} else {
			gchar *relative;

			relative = g_file_get_relative_path (oldfile, docfile);

			if (relative) {
				/* relative now contains the part in docfile without
				   the prefix oldfile */

				g_object_unref (docfile);
				g_free (uri);

				docfile = g_file_get_child (newfile, relative);
				uri = g_file_get_uri (docfile);

				lapiz_document_set_uri (doc, uri);
			}

			g_free (relative);
		}

		g_free (uri);
		g_object_unref (docfile);
	}

	g_object_unref (oldfile);
	g_object_unref (newfile);

	g_list_free (documents);
}

static void
on_filter_pattern_changed_cb (LapizFileBrowserWidget * widget,
                              GParamSpec * param,
                              LapizFileBrowserPluginPrivate * data)
{
	gchar * pattern;

	g_object_get (G_OBJECT (widget), "filter-pattern", &pattern, NULL);

	if (pattern == NULL)
		g_settings_set_string (data->settings, "filter-pattern", "");
	else
		g_settings_set_string (data->settings, "filter-pattern", pattern);

	g_free (pattern);
}

static void
on_virtual_root_changed_cb (LapizFileBrowserStore * store,
                            GParamSpec * param,
                            LapizFileBrowserPluginPrivate * data)
{
	gchar * root;
	gchar * virtual_root;

	root = lapiz_file_browser_store_get_root (store);

	if (!root)
		return;

	g_settings_set_string (data->onload_settings, "root", root);

	virtual_root = lapiz_file_browser_store_get_virtual_root (store);

	if (!virtual_root) {
		/* Set virtual to same as root then */
		g_settings_set_string (data->onload_settings, "virtual-root", root);
	} else {
		g_settings_set_string (data->onload_settings, "virtual-root", virtual_root);
	}

	g_signal_handlers_disconnect_by_func (LAPIZ_WINDOW (data->window),
	                                      G_CALLBACK (on_tab_added_cb),
	                                      data);

	g_free (root);
	g_free (virtual_root);
}

static void
on_tab_added_cb (LapizWindow * window,
                 LapizTab * tab,
                 LapizFileBrowserPluginPrivate *data)
{
	gboolean open;
	gboolean load_default = TRUE;

	open = g_settings_get_boolean (data->settings, "open-at-first-doc");

	if (open) {
		LapizDocument *doc;
		gchar *uri;

		doc = lapiz_tab_get_document (tab);

		uri = lapiz_document_get_uri (doc);

		if (uri != NULL && lapiz_utils_uri_has_file_scheme (uri)) {
			prepare_auto_root (data);
			set_root_from_doc (data, doc);
			load_default = FALSE;
		}

		g_free (uri);
	}

	if (load_default)
		restore_default_location (data);

	/* Disconnect this signal, it's only called once */
	g_signal_handlers_disconnect_by_func (window,
	                                      G_CALLBACK (on_tab_added_cb),
	                                      data);
}

static gchar *
get_filename_from_path (GtkTreeModel *model, GtkTreePath *path)
{
	GtkTreeIter iter;
	gchar *uri;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    LAPIZ_FILE_BROWSER_STORE_COLUMN_URI, &uri,
			    -1);

	return lapiz_file_browser_utils_uri_basename (uri);
}

static gboolean
on_confirm_no_trash_cb (LapizFileBrowserWidget * widget,
                        GList * files,
                        LapizWindow * window)
{
	gchar *normal;
	gchar *message;
	gchar *secondary;
	gboolean result;

	message = _("Cannot move file to trash, do you\nwant to delete permanently?");

	if (files->next == NULL) {
		normal = lapiz_file_browser_utils_file_basename (G_FILE (files->data));
	    	secondary = g_strdup_printf (_("The file \"%s\" cannot be moved to the trash."), normal);
		g_free (normal);
	} else {
		secondary = g_strdup (_("The selected files cannot be moved to the trash."));
	}

	result = lapiz_file_browser_utils_confirmation_dialog (window,
	                                                       GTK_MESSAGE_QUESTION,
	                                                       message,
	                                                       secondary);
	g_free (secondary);

	return result;
}

static gboolean
on_confirm_delete_cb (LapizFileBrowserWidget *widget,
                      LapizFileBrowserStore *store,
                      GList *paths,
                      LapizFileBrowserPluginPrivate *data)
{
	gchar *normal;
	gchar *message;
	gchar *secondary;
	gboolean result;

	if (!data->confirm_trash)
		return TRUE;

	if (paths->next == NULL) {
		normal = get_filename_from_path (GTK_TREE_MODEL (store), (GtkTreePath *)(paths->data));
		message = g_strdup_printf (_("Are you sure you want to permanently delete \"%s\"?"), normal);
		g_free (normal);
	} else {
		message = g_strdup (_("Are you sure you want to permanently delete the selected files?"));
	}

	secondary = _("If you delete an item, it is permanently lost.");

	result = lapiz_file_browser_utils_confirmation_dialog (LAPIZ_WINDOW (data->window),
	                                                       GTK_MESSAGE_QUESTION,
	                                                       message,
	                                                       secondary);

	g_free (message);

	return result;
}

// ex:ts=8:noet:
