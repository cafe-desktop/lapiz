/*
 * lapiz-commands-edit.c
 * This file is part of lapiz
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Modified by the lapiz Team, 1998-2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctk/ctk.h>

#include "lapiz-commands.h"
#include "lapiz-window.h"
#include "lapiz-debug.h"
#include "lapiz-view.h"
#include "dialogs/lapiz-preferences-dialog.h"

void
_lapiz_cmd_edit_undo (CtkAction   *action G_GNUC_UNUSED,
		     LapizWindow *window)
{
	LapizView *active_view;
	CtkSourceBuffer *active_document;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	active_document = CTK_SOURCE_BUFFER (ctk_text_view_get_buffer (CTK_TEXT_VIEW (active_view)));

	ctk_source_buffer_undo (active_document);

	lapiz_view_scroll_to_cursor (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_redo (CtkAction   *action G_GNUC_UNUSED,
		     LapizWindow *window)
{
	LapizView *active_view;
	CtkSourceBuffer *active_document;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	active_document = CTK_SOURCE_BUFFER (ctk_text_view_get_buffer (CTK_TEXT_VIEW (active_view)));

	ctk_source_buffer_redo (active_document);

	lapiz_view_scroll_to_cursor (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_cut (CtkAction   *action G_GNUC_UNUSED,
		    LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	lapiz_view_cut_clipboard (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_copy (CtkAction   *action G_GNUC_UNUSED,
		     LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	lapiz_view_copy_clipboard (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_paste (CtkAction   *action G_GNUC_UNUSED,
		      LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	lapiz_view_paste_clipboard (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_delete (CtkAction   *action G_GNUC_UNUSED,
		       LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	lapiz_view_delete_selection (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_select_all (CtkAction   *action G_GNUC_UNUSED,
			   LapizWindow *window)
{
	LapizView *active_view;

	lapiz_debug (DEBUG_COMMANDS);

	active_view = lapiz_window_get_active_view (window);
	g_return_if_fail (active_view);

	lapiz_view_select_all (active_view);

	ctk_widget_grab_focus (CTK_WIDGET (active_view));
}

void
_lapiz_cmd_edit_preferences (CtkAction   *action G_GNUC_UNUSED,
			    LapizWindow *window)
{
	lapiz_debug (DEBUG_COMMANDS);

	lapiz_show_preferences_dialog (window);
}
