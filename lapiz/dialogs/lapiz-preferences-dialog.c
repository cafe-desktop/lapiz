/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-preferences-dialog.c
 * This file is part of lapiz
 *
 * Copyright (C) 2001-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 2001-2003. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <ctksourceview/ctksource.h>
#include <libbean-ctk/bean-ctk-plugin-manager.h>

#include <lapiz/lapiz-prefs-manager.h>

#include "lapiz-preferences-dialog.h"
#include "lapiz-utils.h"
#include "lapiz-debug.h"
#include "lapiz-document.h"
#include "lapiz-style-scheme-manager.h"
#include "lapiz-help.h"
#include "lapiz-dirs.h"

/*
 * lapiz-preferences dialog is a singleton since we don't
 * want two dialogs showing an inconsistent state of the
 * preferences.
 * When lapiz_show_preferences_dialog is called and there
 * is already a prefs dialog dialog open, it is reparented
 * and shown.
 */

static CtkWidget *preferences_dialog = NULL;


enum
{
	ID_COLUMN = 0,
	NAME_COLUMN,
	DESC_COLUMN,
	NUM_COLUMNS
};

typedef enum
{
	DRAW_NONE = 0,
	DRAW_TRAILING = 1,
	DRAW_ALL = 2
} DrawSpacesSettings;

struct _LapizPreferencesDialogPrivate
{
	CtkWidget	*notebook;

	/* Font */
	CtkWidget	*default_font_checkbutton;
	CtkWidget	*font_button;
	CtkWidget	*font_hbox;

	/* Style Scheme */
	CtkListStore	*schemes_treeview_model;
	CtkWidget	*schemes_treeview;
	CtkWidget	*install_scheme_button;
	CtkWidget	*uninstall_scheme_button;

	CtkWidget	*install_scheme_file_schooser;

	/* Tabs */
	CtkWidget	*tabs_width_spinbutton;
	CtkWidget	*insert_spaces_checkbutton;
	CtkWidget	*tabs_width_hbox;

	/* Auto indentation */
	CtkWidget	*auto_indent_checkbutton;

	/* Draw spaces... */
	CtkWidget       *draw_spaces_checkbutton;
	CtkWidget       *draw_trailing_spaces_checkbutton;
	CtkWidget       *draw_tabs_checkbutton;
	CtkWidget       *draw_trailing_tabs_checkbutton;
	CtkWidget       *draw_newlines_checkbutton;

	/* Text Wrapping */
	CtkWidget	*wrap_text_checkbutton;
	CtkWidget	*split_checkbutton;

	/* File Saving */
	CtkWidget	*backup_copy_checkbutton;
	CtkWidget	*auto_save_checkbutton;
	CtkWidget	*auto_save_spinbutton;
	CtkWidget	*autosave_hbox;

	/* Line numbers */
	CtkWidget	*display_line_numbers_checkbutton;

	/* Highlight current line */
	CtkWidget	*highlight_current_line_checkbutton;

	/* Highlight matching bracket */
	CtkWidget	*bracket_matching_checkbutton;

	/* Right margin */
	CtkWidget	*right_margin_checkbutton;
	CtkWidget	*right_margin_position_spinbutton;
	CtkWidget	*right_margin_position_hbox;

	/* Plugins manager */
	CtkWidget	*plugin_manager_place_holder;
};


G_DEFINE_TYPE_WITH_PRIVATE (LapizPreferencesDialog, lapiz_preferences_dialog, CTK_TYPE_DIALOG)


static void
lapiz_preferences_dialog_class_init (LapizPreferencesDialogClass *klass G_GNUC_UNUSED)
{
}

static void
dialog_response_handler (CtkDialog *dlg,
			 gint       res_id)
{
	lapiz_debug (DEBUG_PREFS);

	switch (res_id)
	{
		case CTK_RESPONSE_HELP:
			lapiz_help_display (CTK_WINDOW (dlg),
					    NULL,
					    "lapiz-prefs");

			g_signal_stop_emission_by_name (dlg, "response");

			break;

		default:
			ctk_widget_destroy (CTK_WIDGET(dlg));
	}
}

static void
tabs_width_spinbutton_value_changed (CtkSpinButton          *spin_button,
				     LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (spin_button == CTK_SPIN_BUTTON (dlg->priv->tabs_width_spinbutton));

	lapiz_prefs_manager_set_tabs_size (ctk_spin_button_get_value_as_int (spin_button));
}

static void
insert_spaces_checkbutton_toggled (CtkToggleButton        *button,
				   LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->insert_spaces_checkbutton));

	lapiz_prefs_manager_set_insert_spaces (ctk_toggle_button_get_active (button));
}

static void
auto_indent_checkbutton_toggled (CtkToggleButton        *button,
				 LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->auto_indent_checkbutton));

	lapiz_prefs_manager_set_auto_indent (ctk_toggle_button_get_active (button));
}

static void
draw_spaces_checkbutton_toggled (CtkToggleButton        *button,
                                 LapizPreferencesDialog *dlg)
{
	DrawSpacesSettings setting;
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->draw_spaces_checkbutton));

	if (ctk_toggle_button_get_active (button))
		setting = DRAW_ALL;
	else
		setting = DRAW_NONE;

	lapiz_prefs_manager_set_draw_spaces (setting);
#ifdef CTK_SOURCE_VERSION_3_24
	if (setting == DRAW_NONE)
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton), FALSE);
	ctk_widget_set_sensitive (CTK_WIDGET (dlg->priv->draw_trailing_spaces_checkbutton), setting > DRAW_NONE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton), setting == DRAW_NONE);
#endif
}

static void
draw_trailing_spaces_checkbutton_toggled (CtkToggleButton        *button,
                                          LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton));

	if (ctk_toggle_button_get_active (button))
		lapiz_prefs_manager_set_draw_spaces (DRAW_TRAILING);
	else
	{
		if (lapiz_prefs_manager_get_draw_spaces ())
			lapiz_prefs_manager_set_draw_spaces (DRAW_ALL);
		else
			lapiz_prefs_manager_set_draw_spaces (DRAW_NONE);
	}
}

static void
draw_tabs_checkbutton_toggled (CtkToggleButton        *button,
                               LapizPreferencesDialog *dlg)
{
	DrawSpacesSettings setting;
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON(dlg->priv->draw_tabs_checkbutton));

	if (ctk_toggle_button_get_active (button))
		setting = DRAW_ALL;
	else
		setting = DRAW_NONE;

	lapiz_prefs_manager_set_draw_tabs (setting);
#ifdef CTK_SOURCE_VERSION_3_24
	if (setting == DRAW_NONE)
		ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton), FALSE);
	ctk_widget_set_sensitive (CTK_WIDGET(dlg->priv->draw_trailing_tabs_checkbutton), setting > DRAW_NONE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON(dlg->priv->draw_trailing_tabs_checkbutton), setting == DRAW_NONE);
#endif
}

static void
draw_trailing_tabs_checkbutton_toggled (CtkToggleButton        *button,
                                        LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton));

	if (ctk_toggle_button_get_active (button))
		lapiz_prefs_manager_set_draw_tabs (DRAW_TRAILING);
	else
	{
		if (lapiz_prefs_manager_get_draw_tabs ())
			lapiz_prefs_manager_set_draw_tabs (DRAW_ALL);
		else
			lapiz_prefs_manager_set_draw_tabs (DRAW_NONE);
	}
}

static void
draw_newlines_checkbutton_toggled (CtkToggleButton        *button,
                                   LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->draw_newlines_checkbutton));

	lapiz_prefs_manager_set_draw_newlines (ctk_toggle_button_get_active (button));
}

static void
auto_save_checkbutton_toggled (CtkToggleButton        *button,
			       LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->auto_save_checkbutton));

	if (ctk_toggle_button_get_active (button))
	{
		ctk_widget_set_sensitive (dlg->priv->auto_save_spinbutton,
					  lapiz_prefs_manager_auto_save_interval_can_set());

		lapiz_prefs_manager_set_auto_save (TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (dlg->priv->auto_save_spinbutton, FALSE);
		lapiz_prefs_manager_set_auto_save (FALSE);
	}
}

static void
backup_copy_checkbutton_toggled (CtkToggleButton        *button,
				 LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->backup_copy_checkbutton));

	lapiz_prefs_manager_set_create_backup_copy (ctk_toggle_button_get_active (button));
}

static void
auto_save_spinbutton_value_changed (CtkSpinButton          *spin_button,
				    LapizPreferencesDialog *dlg)
{
	g_return_if_fail (spin_button == CTK_SPIN_BUTTON (dlg->priv->auto_save_spinbutton));

	lapiz_prefs_manager_set_auto_save_interval (
			MAX (1, ctk_spin_button_get_value_as_int (spin_button)));
}

static void
setup_editor_page (LapizPreferencesDialog *dlg)
{
	gboolean auto_save;
	gint auto_save_interval;

	lapiz_debug (DEBUG_PREFS);

	/* Set initial state */
	ctk_spin_button_set_value (CTK_SPIN_BUTTON (dlg->priv->tabs_width_spinbutton),
				   (guint) lapiz_prefs_manager_get_tabs_size ());
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->insert_spaces_checkbutton),
				      lapiz_prefs_manager_get_insert_spaces ());
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->auto_indent_checkbutton),
				      lapiz_prefs_manager_get_auto_indent ());
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_spaces_checkbutton),
				      lapiz_prefs_manager_get_draw_spaces () > DRAW_NONE);
#ifdef CTK_SOURCE_VERSION_3_24
	ctk_widget_set_sensitive (CTK_WIDGET (dlg->priv->draw_trailing_spaces_checkbutton),
	                          lapiz_prefs_manager_get_draw_spaces () > DRAW_NONE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton),
	                                    lapiz_prefs_manager_get_draw_spaces () == DRAW_NONE);
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton),
	                              lapiz_prefs_manager_get_draw_spaces () == DRAW_TRAILING);
#else
	ctk_widget_set_sensitive (CTK_WIDGET (dlg->priv->draw_trailing_spaces_checkbutton), FALSE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton), TRUE);
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_spaces_checkbutton), FALSE);
#endif
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_tabs_checkbutton),
	                              lapiz_prefs_manager_get_draw_tabs () > DRAW_NONE);
#ifdef CTK_SOURCE_VERSION_3_24
	ctk_widget_set_sensitive (CTK_WIDGET (dlg->priv->draw_trailing_tabs_checkbutton),
	                          lapiz_prefs_manager_get_draw_tabs () > DRAW_NONE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton),
	                                    lapiz_prefs_manager_get_draw_tabs () == DRAW_NONE);
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton),
	                              lapiz_prefs_manager_get_draw_tabs () == DRAW_TRAILING);
#else
	ctk_widget_set_sensitive (CTK_WIDGET (dlg->priv->draw_trailing_tabs_checkbutton), FALSE);
	ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton), FALSE);
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_trailing_tabs_checkbutton), FALSE);
#endif
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->draw_newlines_checkbutton),
	                              lapiz_prefs_manager_get_draw_newlines ());
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->backup_copy_checkbutton),
				      lapiz_prefs_manager_get_create_backup_copy ());

	auto_save = lapiz_prefs_manager_get_auto_save ();
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->auto_save_checkbutton),
				      auto_save);

	auto_save_interval = lapiz_prefs_manager_get_auto_save_interval ();
	if (auto_save_interval <= 0)
		auto_save_interval = GPM_DEFAULT_AUTO_SAVE_INTERVAL;

	ctk_spin_button_set_value (CTK_SPIN_BUTTON (dlg->priv->auto_save_spinbutton),
				   auto_save_interval);

	/* Set widget sensitivity */
	ctk_widget_set_sensitive (dlg->priv->tabs_width_hbox,
				  lapiz_prefs_manager_tabs_size_can_set ());
	ctk_widget_set_sensitive (dlg->priv->insert_spaces_checkbutton,
				  lapiz_prefs_manager_insert_spaces_can_set ());
	ctk_widget_set_sensitive (dlg->priv->auto_indent_checkbutton,
				  lapiz_prefs_manager_auto_indent_can_set ());
	ctk_widget_set_sensitive (dlg->priv->draw_spaces_checkbutton,
				  lapiz_prefs_manager_draw_spaces_can_set ());
	ctk_widget_set_sensitive (dlg->priv->draw_tabs_checkbutton,
				  lapiz_prefs_manager_draw_tabs_can_set ());
	ctk_widget_set_sensitive (dlg->priv->draw_newlines_checkbutton,
				  lapiz_prefs_manager_draw_newlines_can_set ());
	ctk_widget_set_sensitive (dlg->priv->backup_copy_checkbutton,
				  lapiz_prefs_manager_create_backup_copy_can_set ());
	ctk_widget_set_sensitive (dlg->priv->autosave_hbox,
				  lapiz_prefs_manager_auto_save_can_set ());
	ctk_widget_set_sensitive (dlg->priv->auto_save_spinbutton,
			          auto_save &&
				  lapiz_prefs_manager_auto_save_interval_can_set ());

	/* Connect signal */
	g_signal_connect (dlg->priv->tabs_width_spinbutton,
			  "value_changed",
			  G_CALLBACK (tabs_width_spinbutton_value_changed),
			  dlg);
	g_signal_connect (dlg->priv->insert_spaces_checkbutton,
			 "toggled",
			  G_CALLBACK (insert_spaces_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->auto_indent_checkbutton,
			  "toggled",
			  G_CALLBACK (auto_indent_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->draw_spaces_checkbutton,
	                  "toggled",
	                  G_CALLBACK (draw_spaces_checkbutton_toggled),
	                  dlg);
	g_signal_connect (dlg->priv->draw_trailing_spaces_checkbutton,
	                  "toggled",
	                  G_CALLBACK (draw_trailing_spaces_checkbutton_toggled),
	                  dlg);
	g_signal_connect (dlg->priv->draw_tabs_checkbutton,
	                  "toggled",
	                  G_CALLBACK (draw_tabs_checkbutton_toggled),
	                  dlg);
	g_signal_connect (dlg->priv->draw_trailing_tabs_checkbutton,
	                  "toggled",
	                  G_CALLBACK (draw_trailing_tabs_checkbutton_toggled),
	                  dlg);
	g_signal_connect (dlg->priv->draw_newlines_checkbutton,
	                  "toggled",
	                  G_CALLBACK (draw_newlines_checkbutton_toggled),
	                  dlg);
	g_signal_connect (dlg->priv->auto_save_checkbutton,
			  "toggled",
			  G_CALLBACK (auto_save_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->backup_copy_checkbutton,
			  "toggled",
			  G_CALLBACK (backup_copy_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->auto_save_spinbutton,
			  "value_changed",
			  G_CALLBACK (auto_save_spinbutton_value_changed),
			  dlg);
}

static void
display_line_numbers_checkbutton_toggled (CtkToggleButton        *button,
					  LapizPreferencesDialog *dlg)
{
	g_return_if_fail (button ==
			CTK_TOGGLE_BUTTON (dlg->priv->display_line_numbers_checkbutton));

	lapiz_prefs_manager_set_display_line_numbers (ctk_toggle_button_get_active (button));
}

static void
highlight_current_line_checkbutton_toggled (CtkToggleButton        *button,
					    LapizPreferencesDialog *dlg)
{
	g_return_if_fail (button ==
			CTK_TOGGLE_BUTTON (dlg->priv->highlight_current_line_checkbutton));

	lapiz_prefs_manager_set_highlight_current_line (ctk_toggle_button_get_active (button));
}

static void
bracket_matching_checkbutton_toggled (CtkToggleButton        *button,
				      LapizPreferencesDialog *dlg)
{
	g_return_if_fail (button ==
			CTK_TOGGLE_BUTTON (dlg->priv->bracket_matching_checkbutton));

	lapiz_prefs_manager_set_bracket_matching (
				ctk_toggle_button_get_active (button));
}

static gboolean split_button_state = TRUE;

static void
wrap_mode_checkbutton_toggled (CtkToggleButton        *button G_GNUC_UNUSED,
			       LapizPreferencesDialog *dlg)
{
	if (!ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dlg->priv->wrap_text_checkbutton)))
	{
		lapiz_prefs_manager_set_wrap_mode (CTK_WRAP_NONE);

		ctk_widget_set_sensitive (dlg->priv->split_checkbutton,
					  FALSE);
		ctk_toggle_button_set_inconsistent (
			CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (dlg->priv->split_checkbutton,
					  TRUE);

		ctk_toggle_button_set_inconsistent (
			CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), FALSE);


		if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton)))
		{
			split_button_state = TRUE;

			lapiz_prefs_manager_set_wrap_mode (CTK_WRAP_WORD);
		}
		else
		{
			split_button_state = FALSE;

			lapiz_prefs_manager_set_wrap_mode (CTK_WRAP_CHAR);
		}
	}
}

static void
right_margin_checkbutton_toggled (CtkToggleButton        *button,
				  LapizPreferencesDialog *dlg)
{
	gboolean active;

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->right_margin_checkbutton));

	active = ctk_toggle_button_get_active (button);

	lapiz_prefs_manager_set_display_right_margin (active);

	ctk_widget_set_sensitive (dlg->priv->right_margin_position_hbox,
				  active &&
				  lapiz_prefs_manager_right_margin_position_can_set ());
}

static void
right_margin_position_spinbutton_value_changed (CtkSpinButton          *spin_button,
						LapizPreferencesDialog *dlg)
{
	gint value;

	g_return_if_fail (spin_button == CTK_SPIN_BUTTON (dlg->priv->right_margin_position_spinbutton));

	value = CLAMP (ctk_spin_button_get_value_as_int (spin_button), 1, 160);

	lapiz_prefs_manager_set_right_margin_position (value);
}

static void
setup_view_page (LapizPreferencesDialog *dlg)
{
	CtkWrapMode wrap_mode;
	gboolean display_right_margin;
	gboolean wrap_mode_can_set;

	lapiz_debug (DEBUG_PREFS);

	/* Set initial state */
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->display_line_numbers_checkbutton),
				      lapiz_prefs_manager_get_display_line_numbers ());

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->highlight_current_line_checkbutton),
				      lapiz_prefs_manager_get_highlight_current_line ());

	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->bracket_matching_checkbutton),
				      lapiz_prefs_manager_get_bracket_matching ());

	wrap_mode = lapiz_prefs_manager_get_wrap_mode ();
	switch (wrap_mode )
	{
		case CTK_WRAP_WORD:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->wrap_text_checkbutton), TRUE);
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), TRUE);
			break;
		case CTK_WRAP_CHAR:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->wrap_text_checkbutton), TRUE);
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), FALSE);
			break;
		default:
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->wrap_text_checkbutton), FALSE);
			ctk_toggle_button_set_active (
				CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), split_button_state);
			ctk_toggle_button_set_inconsistent (
				CTK_TOGGLE_BUTTON (dlg->priv->split_checkbutton), TRUE);

	}

	display_right_margin = lapiz_prefs_manager_get_display_right_margin ();

	ctk_toggle_button_set_active (
		CTK_TOGGLE_BUTTON (dlg->priv->right_margin_checkbutton),
		display_right_margin);

	ctk_spin_button_set_value (
		CTK_SPIN_BUTTON (dlg->priv->right_margin_position_spinbutton),
		(guint)CLAMP (lapiz_prefs_manager_get_right_margin_position (), 1, 160));

	/* Set widgets sensitivity */
	ctk_widget_set_sensitive (dlg->priv->display_line_numbers_checkbutton,
				  lapiz_prefs_manager_display_line_numbers_can_set ());
	ctk_widget_set_sensitive (dlg->priv->highlight_current_line_checkbutton,
				  lapiz_prefs_manager_highlight_current_line_can_set ());
	ctk_widget_set_sensitive (dlg->priv->bracket_matching_checkbutton,
				  lapiz_prefs_manager_bracket_matching_can_set ());
	wrap_mode_can_set = lapiz_prefs_manager_wrap_mode_can_set ();
	ctk_widget_set_sensitive (dlg->priv->wrap_text_checkbutton,
				  wrap_mode_can_set);
	ctk_widget_set_sensitive (dlg->priv->split_checkbutton,
				  wrap_mode_can_set &&
				  (wrap_mode != CTK_WRAP_NONE));
	ctk_widget_set_sensitive (dlg->priv->right_margin_checkbutton,
				  lapiz_prefs_manager_display_right_margin_can_set ());
	ctk_widget_set_sensitive (dlg->priv->right_margin_position_hbox,
				  display_right_margin &&
				  lapiz_prefs_manager_right_margin_position_can_set ());

	/* Connect signals */
	g_signal_connect (dlg->priv->display_line_numbers_checkbutton,
			  "toggled",
			  G_CALLBACK (display_line_numbers_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->highlight_current_line_checkbutton,
			  "toggled",
			  G_CALLBACK (highlight_current_line_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->bracket_matching_checkbutton,
			  "toggled",
			  G_CALLBACK (bracket_matching_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->wrap_text_checkbutton,
			  "toggled",
			  G_CALLBACK (wrap_mode_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->split_checkbutton,
			  "toggled",
			  G_CALLBACK (wrap_mode_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->right_margin_checkbutton,
			  "toggled",
			  G_CALLBACK (right_margin_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->right_margin_position_spinbutton,
			  "value_changed",
			  G_CALLBACK (right_margin_position_spinbutton_value_changed),
			  dlg);
}

static void
default_font_font_checkbutton_toggled (CtkToggleButton        *button,
				       LapizPreferencesDialog *dlg)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (button == CTK_TOGGLE_BUTTON (dlg->priv->default_font_checkbutton));

	if (ctk_toggle_button_get_active (button))
	{
		ctk_widget_set_sensitive (dlg->priv->font_hbox, FALSE);
		lapiz_prefs_manager_set_use_default_font (TRUE);
	}
	else
	{
		ctk_widget_set_sensitive (dlg->priv->font_hbox,
					  lapiz_prefs_manager_editor_font_can_set ());
		lapiz_prefs_manager_set_use_default_font (FALSE);
	}
}

static void
editor_font_button_font_set (CtkFontChooser         *font_button,
			     LapizPreferencesDialog *dlg)
{
	const gchar *font_name;

	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (font_button == CTK_FONT_CHOOSER (dlg->priv->font_button));

	/* FIXME: Can this fail? Ctk docs are a bit terse... 21-02-2004 pbor */
	font_name = ctk_font_chooser_get_font (font_button);
	if (!font_name)
	{
		g_warning ("Could not get font name");
		return;
	}

	lapiz_prefs_manager_set_editor_font (font_name);
}

static void
setup_font_colors_page_font_section (LapizPreferencesDialog *dlg)
{
	gboolean use_default_font;
	gchar *editor_font = NULL;
	gchar *label;

	lapiz_debug (DEBUG_PREFS);

	ctk_widget_set_tooltip_text (dlg->priv->font_button,
			 _("Click on this button to select the font to be used by the editor"));

	lapiz_utils_set_atk_relation (dlg->priv->font_button,
				      dlg->priv->default_font_checkbutton,
				      ATK_RELATION_CONTROLLED_BY);
	lapiz_utils_set_atk_relation (dlg->priv->default_font_checkbutton,
				      dlg->priv->font_button,
				      ATK_RELATION_CONTROLLER_FOR);

	editor_font = lapiz_prefs_manager_get_system_font ();
	label = g_strdup_printf(_("_Use the system fixed width font (%s)"),
				editor_font);
	ctk_button_set_label (CTK_BUTTON (dlg->priv->default_font_checkbutton),
			      label);
	g_free (editor_font);
	g_free (label);

	/* read current config and setup initial state */
	use_default_font = lapiz_prefs_manager_get_use_default_font ();
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dlg->priv->default_font_checkbutton),
				      use_default_font);

	editor_font = lapiz_prefs_manager_get_editor_font ();
	if (editor_font != NULL)
	{
		ctk_font_chooser_set_font (CTK_FONT_CHOOSER (dlg->priv->font_button),
					   editor_font);
		g_free (editor_font);
	}

	/* Connect signals */
	g_signal_connect (dlg->priv->default_font_checkbutton,
			  "toggled",
			  G_CALLBACK (default_font_font_checkbutton_toggled),
			  dlg);
	g_signal_connect (dlg->priv->font_button,
			  "font_set",
			  G_CALLBACK (editor_font_button_font_set),
			  dlg);

	/* Set initial widget sensitivity */
	ctk_widget_set_sensitive (dlg->priv->default_font_checkbutton,
				  lapiz_prefs_manager_use_default_font_can_set ());

	if (use_default_font)
		ctk_widget_set_sensitive (dlg->priv->font_hbox, FALSE);
	else
		ctk_widget_set_sensitive (dlg->priv->font_hbox,
					  lapiz_prefs_manager_editor_font_can_set ());
}

static void
set_buttons_sensisitivity_according_to_scheme (LapizPreferencesDialog *dlg,
					       const gchar            *scheme_id)
{
	gboolean editable;

	editable = (scheme_id != NULL) &&
	           _lapiz_style_scheme_manager_scheme_is_lapiz_user_scheme (
						lapiz_get_style_scheme_manager (),
						scheme_id);

	ctk_widget_set_sensitive (dlg->priv->uninstall_scheme_button,
				  editable);
}

static void
style_scheme_changed (CtkWidget              *treeview G_GNUC_UNUSED,
		      LapizPreferencesDialog *dlg)
{
	CtkTreePath *path;
	CtkTreeIter iter;
	gchar *id;

	ctk_tree_view_get_cursor (CTK_TREE_VIEW (dlg->priv->schemes_treeview), &path, NULL);
	ctk_tree_model_get_iter (CTK_TREE_MODEL (dlg->priv->schemes_treeview_model),
				 &iter, path);
	ctk_tree_path_free (path);
	ctk_tree_model_get (CTK_TREE_MODEL (dlg->priv->schemes_treeview_model),
			    &iter, ID_COLUMN, &id, -1);

	lapiz_prefs_manager_set_source_style_scheme (id);

	set_buttons_sensisitivity_according_to_scheme (dlg, id);

	g_free (id);
}

static const gchar *
ensure_color_scheme_id (const gchar *id)
{
	CtkSourceStyleScheme *scheme = NULL;
	CtkSourceStyleSchemeManager *manager = lapiz_get_style_scheme_manager ();

	if (id == NULL)
	{
		gchar *pref_id;

		pref_id = lapiz_prefs_manager_get_source_style_scheme ();
		scheme = ctk_source_style_scheme_manager_get_scheme (manager,
								     pref_id);
		g_free (pref_id);
	}
	else
	{
		scheme = ctk_source_style_scheme_manager_get_scheme (manager,
								     id);
	}

	if (scheme == NULL)
	{
		/* Fall-back to classic style scheme */
		scheme = ctk_source_style_scheme_manager_get_scheme (manager,
								     "classic");
	}

	if (scheme == NULL)
	{
		/* Cannot determine default style scheme -> broken CtkSourceView installation */
		return NULL;
	}

	return 	ctk_source_style_scheme_get_id (scheme);
}

/* If def_id is NULL, use the default scheme as returned by
 * lapiz_style_scheme_manager_get_default_scheme. If this one returns NULL
 * use the first available scheme as default */
static const gchar *
populate_color_scheme_list (LapizPreferencesDialog *dlg, const gchar *def_id)
{
	GSList *schemes;
	GSList *l;

	ctk_list_store_clear (dlg->priv->schemes_treeview_model);

	def_id = ensure_color_scheme_id (def_id);
	if (def_id == NULL)
	{
		g_warning ("Cannot build the list of available color schemes.\n"
		           "Please check your CtkSourceView installation.");
		return NULL;
	}

	schemes = lapiz_style_scheme_manager_list_schemes_sorted (lapiz_get_style_scheme_manager ());
	l = schemes;
	while (l != NULL)
	{
		CtkSourceStyleScheme *scheme;
		const gchar *id;
		const gchar *name;
		const gchar *description;
		CtkTreeIter iter;

		scheme = CTK_SOURCE_STYLE_SCHEME (l->data);

		id = ctk_source_style_scheme_get_id (scheme);
		name = ctk_source_style_scheme_get_name (scheme);
		description = ctk_source_style_scheme_get_description (scheme);

		ctk_list_store_append (dlg->priv->schemes_treeview_model, &iter);
		ctk_list_store_set (dlg->priv->schemes_treeview_model,
				    &iter,
				    ID_COLUMN, id,
				    NAME_COLUMN, name,
				    DESC_COLUMN, description,
				    -1);

		g_return_val_if_fail (def_id != NULL, NULL);
		if (strcmp (id, def_id) == 0)
		{
			CtkTreeSelection *selection;

			selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->priv->schemes_treeview));
			ctk_tree_selection_select_iter (selection, &iter);
		}

		l = g_slist_next (l);
	}

	g_slist_free (schemes);

	return def_id;
}

static void
add_scheme_chooser_response_cb (CtkDialog              *chooser,
				gint                    res_id,
				LapizPreferencesDialog *dlg)
{
	gchar* filename;
	const gchar *scheme_id;

	if (res_id != CTK_RESPONSE_ACCEPT)
	{
		ctk_widget_hide (CTK_WIDGET (chooser));
		return;
	}

	filename = ctk_file_chooser_get_filename (CTK_FILE_CHOOSER (chooser));
	if (filename == NULL)
		return;

	ctk_widget_hide (CTK_WIDGET (chooser));

	scheme_id = _lapiz_style_scheme_manager_install_scheme (
					lapiz_get_style_scheme_manager (),
					filename);
	g_free (filename);

	if (scheme_id == NULL)
	{
		lapiz_warning (CTK_WINDOW (dlg),
			       _("The selected color scheme cannot be installed."));

		return;
	}

	lapiz_prefs_manager_set_source_style_scheme (scheme_id);

	scheme_id = populate_color_scheme_list (dlg, scheme_id);

	set_buttons_sensisitivity_according_to_scheme (dlg, scheme_id);
}

static CtkWidget *
scheme_file_chooser_dialog_new_valist (const gchar          *title,
				       CtkWindow            *parent,
				       CtkFileChooserAction  action,
				       const gchar          *first_button_text,
				       va_list               varargs)
{
	CtkWidget *result;
	const char *button_text = first_button_text;
	gint response_id;

	result = g_object_new (CTK_TYPE_FILE_CHOOSER_DIALOG,
			       "title", title,
			       "action", action,
			       NULL);

	if (parent)
		ctk_window_set_transient_for (CTK_WINDOW (result), parent);

	while (button_text)
		{
			response_id = va_arg (varargs, gint);

			if (g_strcmp0 (button_text, "process-stop") == 0)
				lapiz_dialog_add_button (CTK_DIALOG (result), _("_Cancel"), button_text, response_id);
			else
				ctk_dialog_add_button (CTK_DIALOG (result), button_text, response_id);

			button_text = va_arg (varargs, const gchar *);
		}

	return result;
}

static CtkWidget *
scheme_file_chooser_dialog_new (const gchar          *title,
				CtkWindow            *parent,
				CtkFileChooserAction  action,
				const gchar          *first_button_text,
				...)
{
	CtkWidget *result;
	va_list varargs;

	va_start (varargs, first_button_text);
	result = scheme_file_chooser_dialog_new_valist (title, parent, action,
							first_button_text,
							varargs);
	va_end (varargs);

	return result;
}

static void
install_scheme_clicked (CtkButton              *button G_GNUC_UNUSED,
			LapizPreferencesDialog *dlg)
{
	CtkWidget      *chooser;
	CtkFileFilter  *filter;

	if (dlg->priv->install_scheme_file_schooser != NULL) {
		ctk_window_present (CTK_WINDOW (dlg->priv->install_scheme_file_schooser));
		ctk_widget_grab_focus (dlg->priv->install_scheme_file_schooser);
		return;
	}

	chooser = scheme_file_chooser_dialog_new (_("Add Scheme"),
						  CTK_WINDOW (dlg),
						  CTK_FILE_CHOOSER_ACTION_OPEN,
						  "process-stop", CTK_RESPONSE_CANCEL,
						  NULL);

	lapiz_dialog_add_button (CTK_DIALOG (chooser),
				 _("A_dd Scheme"),
				 "list-add",
				 CTK_RESPONSE_ACCEPT);

	ctk_window_set_destroy_with_parent (CTK_WINDOW (chooser), TRUE);

	/* Filters */
	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, _("Color Scheme Files"));
	ctk_file_filter_add_pattern (filter, "*.xml");
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (chooser), filter);

	ctk_file_chooser_set_filter (CTK_FILE_CHOOSER (chooser), filter);

	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, _("All Files"));
	ctk_file_filter_add_pattern (filter, "*");
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (chooser), filter);

	ctk_dialog_set_default_response (CTK_DIALOG (chooser), CTK_RESPONSE_ACCEPT);

	g_signal_connect (chooser,
			  "response",
			  G_CALLBACK (add_scheme_chooser_response_cb),
			  dlg);

	dlg->priv->install_scheme_file_schooser = chooser;

	g_object_add_weak_pointer (G_OBJECT (chooser),
				   (gpointer) &dlg->priv->install_scheme_file_schooser);

	ctk_widget_show (chooser);
}

static void
uninstall_scheme_clicked (CtkButton              *button G_GNUC_UNUSED,
			  LapizPreferencesDialog *dlg)
{
	CtkTreeSelection *selection;
	CtkTreeModel *model;
	CtkTreeIter iter;

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->priv->schemes_treeview));
	model = CTK_TREE_MODEL (dlg->priv->schemes_treeview_model);

	if (ctk_tree_selection_get_selected (selection,
					     &model,
					     &iter))
	{
		gchar *id;
		gchar *name;

		ctk_tree_model_get (model, &iter,
				    ID_COLUMN, &id,
				    NAME_COLUMN, &name,
				    -1);

		if (!_lapiz_style_scheme_manager_uninstall_scheme (lapiz_get_style_scheme_manager (), id))
		{
			lapiz_warning (CTK_WINDOW (dlg),
				       _("Could not remove color scheme \"%s\"."),
				       name);
		}
		else
		{
			const gchar *real_new_id;
			gchar *new_id = NULL;
			CtkTreePath *path;
			CtkTreeIter new_iter;
			gboolean new_iter_set = FALSE;

			/* If the removed style scheme is the last of the list,
			 * set as new default style scheme the previous one,
			 * otherwise set the next one.
			 * To make this possible, we need to get the id of the
			 * new default style scheme before re-populating the list.
			 * Fall back to "classic" if it is not possible to get
			 * the id
			 */
			path = ctk_tree_model_get_path (model, &iter);

			/* Try to move to the next path */
			ctk_tree_path_next (path);
			if (!ctk_tree_model_get_iter (model, &new_iter, path))
			{
				/* It seems the removed style scheme was the
				 * last of the list. Try to move to the
				 * previous one */
				ctk_tree_path_free (path);

				path = ctk_tree_model_get_path (model, &iter);

				ctk_tree_path_prev (path);
				if (ctk_tree_model_get_iter (model, &new_iter, path))
					new_iter_set = TRUE;
			}
			else
				new_iter_set = TRUE;

			ctk_tree_path_free (path);

			if (new_iter_set)
				ctk_tree_model_get (model, &new_iter,
						    ID_COLUMN, &new_id,
						    -1);

			real_new_id = populate_color_scheme_list (dlg, new_id);
			g_free (new_id);

			set_buttons_sensisitivity_according_to_scheme (dlg, real_new_id);

			if (real_new_id != NULL)
				lapiz_prefs_manager_set_source_style_scheme (real_new_id);
		}

		g_free (id);
		g_free (name);
	}
}

static void
scheme_description_cell_data_func (CtkTreeViewColumn *column G_GNUC_UNUSED,
				   CtkCellRenderer   *renderer,
				   CtkTreeModel      *model,
				   CtkTreeIter       *iter,
				   gpointer           data G_GNUC_UNUSED)
{
	gchar *name;
	gchar *desc;
	gchar *text;

	ctk_tree_model_get (model, iter,
			    NAME_COLUMN, &name,
			    DESC_COLUMN, &desc,
			    -1);

	if (desc != NULL)
	{
		text = g_markup_printf_escaped ("<b>%s</b> - %s",
						name,
						desc);
	}
	else
	{
		text = g_markup_printf_escaped ("<b>%s</b>",
						name);
	}

	g_free (name);
	g_free (desc);

	g_object_set (G_OBJECT (renderer),
		      "markup",
		      text,
		      NULL);

	g_free (text);
}

static void
setup_font_colors_page_style_scheme_section (LapizPreferencesDialog *dlg)
{
	CtkCellRenderer *renderer;
	CtkTreeViewColumn *column;
	CtkTreeSelection *selection;
	const gchar *def_id;

	lapiz_debug (DEBUG_PREFS);

	/* Create CtkListStore for styles & setup treeview. */
	dlg->priv->schemes_treeview_model = ctk_list_store_new (NUM_COLUMNS,
								G_TYPE_STRING,
								G_TYPE_STRING,
								G_TYPE_STRING,
								G_TYPE_STRING);

	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (dlg->priv->schemes_treeview_model),
					      0,
					      CTK_SORT_ASCENDING);
	ctk_tree_view_set_model (CTK_TREE_VIEW (dlg->priv->schemes_treeview),
				 CTK_TREE_MODEL (dlg->priv->schemes_treeview_model));

	column = ctk_tree_view_column_new ();

	renderer = ctk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	ctk_tree_view_column_pack_start (column, renderer, TRUE);
	ctk_tree_view_column_set_cell_data_func (column,
						 renderer,
						 scheme_description_cell_data_func,
						 dlg,
						 NULL);

	ctk_tree_view_append_column (CTK_TREE_VIEW (dlg->priv->schemes_treeview),
				     column);

	selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (dlg->priv->schemes_treeview));
	ctk_tree_selection_set_mode (selection, CTK_SELECTION_BROWSE);

	def_id = populate_color_scheme_list (dlg, NULL);

	ctk_button_set_image (CTK_BUTTON (dlg->priv->uninstall_scheme_button),
			      ctk_image_new_from_icon_name ("list-remove", CTK_ICON_SIZE_BUTTON));

	/* Connect signals */
	g_signal_connect (dlg->priv->schemes_treeview,
			  "cursor-changed",
			  G_CALLBACK (style_scheme_changed),
			  dlg);
	g_signal_connect (dlg->priv->install_scheme_button,
			  "clicked",
			  G_CALLBACK (install_scheme_clicked),
			  dlg);
	g_signal_connect (dlg->priv->uninstall_scheme_button,
			  "clicked",
			  G_CALLBACK (uninstall_scheme_clicked),
			  dlg);

	/* Set initial widget sensitivity */
	set_buttons_sensisitivity_according_to_scheme (dlg, def_id);
}

static void
setup_font_colors_page (LapizPreferencesDialog *dlg)
{
	setup_font_colors_page_font_section (dlg);
	setup_font_colors_page_style_scheme_section (dlg);
}

static void
setup_plugins_page (LapizPreferencesDialog *dlg)
{
	CtkWidget *page_content;

	lapiz_debug (DEBUG_PREFS);

	page_content = bean_ctk_plugin_manager_new (NULL);
	g_return_if_fail (page_content != NULL);

	ctk_box_pack_start (CTK_BOX (dlg->priv->plugin_manager_place_holder),
			    page_content,
			    TRUE,
			    TRUE,
			    0);

	ctk_widget_show_all (page_content);
}

static void
lapiz_preferences_dialog_init (LapizPreferencesDialog *dlg)
{
	CtkWidget *error_widget;
	gboolean ret;
	gchar *file;
	gchar *root_objects[] = {
		"notebook",
		"adjustment1",
		"adjustment2",
		"adjustment3",
		"install_scheme_image",
		NULL
	};

	lapiz_debug (DEBUG_PREFS);

	dlg->priv = lapiz_preferences_dialog_get_instance_private (dlg);

	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Close"), "window-close", CTK_RESPONSE_CLOSE);
	lapiz_dialog_add_button (CTK_DIALOG (dlg), _("_Help"), "help-browser", CTK_RESPONSE_HELP);

	ctk_window_set_title (CTK_WINDOW (dlg), _("Lapiz Preferences"));
	ctk_window_set_resizable (CTK_WINDOW (dlg), FALSE);
	ctk_window_set_destroy_with_parent (CTK_WINDOW (dlg), TRUE);

	/* HIG defaults */
	ctk_container_set_border_width (CTK_CONTAINER (dlg), 5);
	ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))), 2); /* 2 * 5 + 2 = 12 */

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (dialog_response_handler),
			  NULL);

	file = lapiz_dirs_get_ui_file ("lapiz-preferences-dialog.ui");
	ret = lapiz_utils_get_ui_objects (file,
		root_objects,
		&error_widget,

		"notebook", &dlg->priv->notebook,

		"display_line_numbers_checkbutton", &dlg->priv->display_line_numbers_checkbutton,
		"highlight_current_line_checkbutton", &dlg->priv->highlight_current_line_checkbutton,
		"bracket_matching_checkbutton", &dlg->priv->bracket_matching_checkbutton,
		"wrap_text_checkbutton", &dlg->priv->wrap_text_checkbutton,
		"split_checkbutton", &dlg->priv->split_checkbutton,

		"right_margin_checkbutton", &dlg->priv->right_margin_checkbutton,
		"right_margin_position_spinbutton", &dlg->priv->right_margin_position_spinbutton,
		"right_margin_position_hbox", &dlg->priv->right_margin_position_hbox,

		"tabs_width_spinbutton", &dlg->priv->tabs_width_spinbutton,
		"tabs_width_hbox", &dlg->priv->tabs_width_hbox,
		"insert_spaces_checkbutton", &dlg->priv->insert_spaces_checkbutton,

		"auto_indent_checkbutton", &dlg->priv->auto_indent_checkbutton,

		"draw_spaces_checkbutton", &dlg->priv->draw_spaces_checkbutton,
		"draw_trailing_spaces_checkbutton", &dlg->priv->draw_trailing_spaces_checkbutton,
		"draw_tabs_checkbutton", &dlg->priv->draw_tabs_checkbutton,
		"draw_trailing_tabs_checkbutton", &dlg->priv->draw_trailing_tabs_checkbutton,
		"draw_newlines_checkbutton", &dlg->priv->draw_newlines_checkbutton,

		"autosave_hbox", &dlg->priv->autosave_hbox,
		"backup_copy_checkbutton", &dlg->priv->backup_copy_checkbutton,
		"auto_save_checkbutton", &dlg->priv->auto_save_checkbutton,
		"auto_save_spinbutton", &dlg->priv->auto_save_spinbutton,

		"default_font_checkbutton", &dlg->priv->default_font_checkbutton,
		"font_button", &dlg->priv->font_button,
		"font_hbox", &dlg->priv->font_hbox,

		"schemes_treeview", &dlg->priv->schemes_treeview,
		"install_scheme_button", &dlg->priv->install_scheme_button,
		"uninstall_scheme_button", &dlg->priv->uninstall_scheme_button,

		"plugin_manager_place_holder", &dlg->priv->plugin_manager_place_holder,

		NULL);
	g_free (file);

	if (!ret)
	{
		ctk_widget_show (error_widget);

		ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
		                    error_widget,
		                    TRUE, TRUE, 0);

		return;
	}

	ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (dlg))),
			    dlg->priv->notebook, FALSE, FALSE, 0);
	g_object_unref (dlg->priv->notebook);
	ctk_container_set_border_width (CTK_CONTAINER (dlg->priv->notebook), 5);

	setup_editor_page (dlg);
	setup_view_page (dlg);
	setup_font_colors_page (dlg);
	setup_plugins_page (dlg);
}

void
lapiz_show_preferences_dialog (LapizWindow *parent)
{
	lapiz_debug (DEBUG_PREFS);

	g_return_if_fail (LAPIZ_IS_WINDOW (parent));

	if (preferences_dialog == NULL)
	{
		preferences_dialog = CTK_WIDGET (g_object_new (LAPIZ_TYPE_PREFERENCES_DIALOG, NULL));
		g_signal_connect (preferences_dialog,
				  "destroy",
				  G_CALLBACK (ctk_widget_destroyed),
				  &preferences_dialog);
	}

	if (CTK_WINDOW (parent) != ctk_window_get_transient_for (CTK_WINDOW (preferences_dialog)))
	{
		ctk_window_set_transient_for (CTK_WINDOW (preferences_dialog),
					      CTK_WINDOW (parent));
	}

	ctk_window_present (CTK_WINDOW (preferences_dialog));
}
