/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-commands.h
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

#ifndef __LAPIZ_COMMANDS_H__
#define __LAPIZ_COMMANDS_H__

#include <ctk/ctk.h>
#include <lapiz/lapiz-window.h>

G_BEGIN_DECLS

/* Do nothing if URI does not exist */
void		 lapiz_commands_load_uri		(LapizWindow         *window,
							 const gchar         *uri,
							 const LapizEncoding *encoding,
							 gint                 line_pos);

/* Ignore non-existing URIs */
gint		 lapiz_commands_load_uris		(LapizWindow         *window,
							 const GSList        *uris,
							 const LapizEncoding *encoding,
							 gint                 line_pos);

void		 lapiz_commands_save_document		(LapizWindow         *window,
                                                         LapizDocument       *document);

void		 lapiz_commands_save_all_documents 	(LapizWindow         *window);

/*
 * Non-exported functions
 */

/* Create titled documens for non-existing URIs */
gint		_lapiz_cmd_load_files_from_prompt	(LapizWindow         *window,
							 GSList              *files,
							 const LapizEncoding *encoding,
							 gint                 line_pos);

void		_lapiz_cmd_file_new			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_open			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_save			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_save_as			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_save_all		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_revert			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_open_uri		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_print_preview		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_print			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_close			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_close_all		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_file_quit			(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_edit_undo			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_redo			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_cut			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_copy			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_paste			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_delete			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_select_all		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_edit_preferences		(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_view_show_toolbar		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_view_show_statusbar		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_view_show_side_pane		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_view_show_bottom_pane	(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_view_toggle_fullscreen_mode	(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_view_leave_fullscreen_mode	(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_search_find			(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_find_next		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_find_prev		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_replace		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_clear_highlight	(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_goto_line		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_search_incremental_search	(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_documents_previous_document	(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_documents_next_document	(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_documents_move_to_new_window	(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_help_contents		(CtkAction   *action,
							 LapizWindow *window);
void		_lapiz_cmd_help_about			(CtkAction   *action,
							 LapizWindow *window);

void		_lapiz_cmd_file_close_tab 		(LapizTab    *tab,
							 LapizWindow *window);

void		_lapiz_cmd_file_save_documents_list	(LapizWindow *window,
							 GList       *docs);

G_END_DECLS

#endif /* __LAPIZ_COMMANDS_H__ */
